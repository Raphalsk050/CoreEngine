#include "core/render/render_system.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <tsl/robin_map.h>

#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/directional_light_component.h"
#include "core/ecs/components/environment_light_component.h"
#include "core/ecs/components/hierarchy_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/point_light_component.h"
#include "core/ecs/components/reflection_probe_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/node.h"
#include "core/ecs/world.h"
#include "core/log/logger.h"
#include "core/render/builtin_shaders.h"
#include "core/render/material.h"
#include "core/render/primitives.h"
#include "core/render/render_pass/default_scene_render_pass.h"
#include "core/render/render_pass/pbr_debug_pass.h"
#include "core/render/render_pass/pbr_ibl_pass.h"
#include "core/render/render_pass/pbr_shadow_pass.h"
#include "core/time/frame_clock.h"

namespace CoreEngine {
    struct RenderSystem::AsyncModelLoadRequest {
        ModelHandle handle;
        Future<ModelHandle> future;
    };

    struct RenderSystem::UploadedModelResources {
        std::vector<MeshHandle> meshes;
        std::vector<std::string> mesh_names;
        std::vector<MaterialHandle> materials;
        std::vector<std::uint32_t> mesh_material_indices;
        std::vector<ModelNodeAsset> nodes;
        std::string error_message;

        [[nodiscard]] bool IsSuccess() const { return error_message.empty() && !meshes.empty() && !materials.empty(); }
    };

    struct RenderSystem::ModelRegistry {
        struct Record {
            Record() = default;
            Record(const Record &) = delete;
            Record &operator=(const Record &) = delete;
            Record(Record &&) noexcept = default;
            Record &operator=(Record &&) noexcept = default;

            uint32_t generation = 0;
            ModelLoadState state = ModelLoadState::Invalid;
            std::vector<MeshHandle> meshes;
            std::vector<std::string> mesh_names;
            std::vector<MaterialHandle> materials;
            std::vector<std::uint32_t> mesh_material_indices;
            std::vector<ModelNodeAsset> nodes;
            std::unique_ptr<ModelLoadResult> decoded_result;
            std::string error_message;
            FuturePromise<ModelHandle> completion;
            ModelMaterialPipeline material_pipeline = ModelMaterialPipeline::Unlit;
        };

        struct LoadTask {
            ModelHandle handle;
            ModelLoadDesc desc;
        };

        tsl::robin_map<uint32_t, Record> records;
        std::unordered_map<std::string, TextureHandle> texture_cache;
        mutable std::mutex mutex;
        std::mutex load_queue_mutex;
        std::condition_variable_any load_event;
        std::deque<LoadTask> load_queue;
        std::jthread load_worker;
        bool load_worker_started = false;
        uint32_t next_model_id = 1;
        uint32_t model_generation = 1;
    };

    namespace {
        struct PendingModelUpload {
            ModelHandle handle;
            ModelLoadResult result;
            ModelMaterialPipeline material_pipeline = ModelMaterialPipeline::Unlit;
            FuturePromise<ModelHandle> completion;
        };

        using RenderCpuClock = std::chrono::steady_clock;

        [[nodiscard]] float CpuElapsedMilliseconds(RenderCpuClock::time_point begin,
                                                   RenderCpuClock::time_point end) {
            return std::chrono::duration<float, std::milli>(end - begin).count();
        }

        void AccumulateStageCpuTime(RenderDebugStats &stats, RenderPassStage stage, float elapsed_ms) {
            switch (stage) {
                case RenderPassStage::FrameSetup:         stats.frame_setup_cpu_ms += elapsed_ms; break;
                case RenderPassStage::Shadow:             stats.shadow_cpu_ms += elapsed_ms; break;
                case RenderPassStage::ForwardOpaque:      stats.forward_opaque_cpu_ms += elapsed_ms; break;
                case RenderPassStage::Debug:              stats.debug_cpu_ms += elapsed_ms; break;
                case RenderPassStage::UI:                 stats.ui_cpu_ms += elapsed_ms; break;
                case RenderPassStage::Present:            stats.present_cpu_ms += elapsed_ms; break;
                case RenderPassStage::DepthPrePass:
                case RenderPassStage::GBuffer:
                case RenderPassStage::Lighting:
                case RenderPassStage::ForwardTransparent:
                case RenderPassStage::PostProcess:        break;
            }
        }

        [[nodiscard]] const ModelTextureAsset *FindModelTexture(const ModelMaterialAsset &material,
                                                                ModelTextureSemantic semantic) {
            for (const ModelTextureAsset &texture: material.textures) {
                if (texture.semantic == semantic && texture.IsValid()) {
                    return &texture;
                }
            }

            return nullptr;
        }

        [[nodiscard]] std::uint32_t NormalizeModelMaterialIndex(std::uint32_t material_index,
                                                                std::size_t material_count) {
            if (material_count == 0u) {
                return 0u;
            }

            return material_index < material_count ? material_index : 0u;
        }

        [[nodiscard]] std::string ModelTextureCacheKey(const ModelTextureAsset &texture) {
            return texture.path + (texture.srgb ? "|srgb" : "|linear");
        }

        void DestroyUploadedMeshes(IRenderBackend &backend, std::vector<MeshHandle> &meshes) {
            for (MeshHandle mesh: meshes) {
                backend.DestroyMesh(mesh);
            }
            meshes.clear();
        }

        [[nodiscard]] std::string MakeModelNodeName(const ModelNodeAsset &node, std::size_t index) {
            if (!node.name.empty()) {
                return node.name;
            }

            return "ModelNode_" + std::to_string(index);
        }

        [[nodiscard]] std::string MakeModelMeshNodeName(const std::string &mesh_name, std::size_t mesh_index) {
            if (!mesh_name.empty()) {
                return mesh_name;
            }

            return "ModelMesh_" + std::to_string(mesh_index);
        }

        [[nodiscard]] Math::Mat4 ResolveCachedWorldMatrix(World &world, entt::entity entity,
                                                          std::unordered_map<entt::entity, Math::Mat4> &cache,
                                                          std::uint32_t depth = 0) {
            constexpr std::uint32_t kMaxHierarchyDepth = 1024u;
            if (entity == entt::null || !world.Registry().valid(entity) || depth >= kMaxHierarchyDepth) {
                return Math::Identity();
            }

            if (const auto it = cache.find(entity); it != cache.end()) {
                return it->second;
            }

            const TransformComponent *transform = world.TryGetComponent<TransformComponent>(entity);
            if (transform == nullptr) {
                return Math::Identity();
            }

            Math::Mat4 world_matrix = transform->WorldMatrix();
            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            if (hierarchy != nullptr && hierarchy->parent != entt::null && hierarchy->parent != entity &&
                world.Registry().valid(hierarchy->parent)) {
                world_matrix = ResolveCachedWorldMatrix(world, hierarchy->parent, cache, depth + 1u) * world_matrix;
            }

            cache.emplace(entity, world_matrix);
            return world_matrix;
        }

        [[nodiscard]] Math::Vec3 ExtractCameraPosition(const CameraData &camera) {
            const Math::Mat4 inverse_view = Math::Inverse(camera.view);
            return {inverse_view.data[12], inverse_view.data[13], inverse_view.data[14]};
        }

        [[nodiscard]] Math::Vec3 ExtractCameraForward(const CameraData &camera) {
            constexpr float kDirectionEpsilon = 1.0e-8f;
            const Math::Mat4 inverse_view = Math::Inverse(camera.view);
            Math::Vec3 forward{inverse_view.data[8], inverse_view.data[9], inverse_view.data[10]};
            if (Math::LengthSquared(forward) <= kDirectionEpsilon) {
                return {0.f, 0.f, 1.f};
            }

            return Math::Normalize(forward);
        }

        [[nodiscard]] Math::Vec3 ExtractWorldPosition(const Math::Mat4 &world_matrix) {
            return {world_matrix.data[12], world_matrix.data[13], world_matrix.data[14]};
        }

        struct DirectionalShadowCascadeData {
            Math::Mat4 view_proj{1.f};
            float split_depth = 0.f;
        };

        [[nodiscard]] Math::Vec3 TransformPoint(const Math::Mat4 &matrix, const Math::Vec3 &point) noexcept {
            constexpr float kHomogeneousEpsilon = 1.0e-6f;
            const float x = matrix.data[0] * point.x + matrix.data[4] * point.y + matrix.data[8] * point.z +
                            matrix.data[12];
            const float y = matrix.data[1] * point.x + matrix.data[5] * point.y + matrix.data[9] * point.z +
                            matrix.data[13];
            const float z = matrix.data[2] * point.x + matrix.data[6] * point.y + matrix.data[10] * point.z +
                            matrix.data[14];
            const float w = matrix.data[3] * point.x + matrix.data[7] * point.y + matrix.data[11] * point.z +
                            matrix.data[15];

            if (std::fabs(w) <= kHomogeneousEpsilon) {
                return {x, y, z};
            }

            const float inv_w = 1.f / w;
            return {x * inv_w, y * inv_w, z * inv_w};
        }

        [[nodiscard]] Math::Vec3 ExtractCameraRight(const CameraData &camera) {
            const Math::Mat4 inverse_view = Math::Inverse(camera.view);
            Math::Vec3 right{inverse_view.data[0], inverse_view.data[1], inverse_view.data[2]};
            return Math::LengthSquared(right) <= 1.0e-8f ? Math::Vec3{1.f, 0.f, 0.f} : Math::Normalize(right);
        }

        [[nodiscard]] Math::Vec3 ExtractCameraUp(const CameraData &camera) {
            const Math::Mat4 inverse_view = Math::Inverse(camera.view);
            Math::Vec3 up{inverse_view.data[4], inverse_view.data[5], inverse_view.data[6]};
            return Math::LengthSquared(up) <= 1.0e-8f ? Math::Vec3{0.f, 1.f, 0.f} : Math::Normalize(up);
        }

        [[nodiscard]] bool IsPerspectiveProjection(const Math::Mat4 &projection) noexcept {
            return std::fabs(projection.At(3, 2)) > 0.5f && std::fabs(projection.At(3, 3)) < 0.5f;
        }

        [[nodiscard]] float ExtractProjectionNear(const Math::Mat4 &projection) noexcept {
            constexpr float kProjectionEpsilon = 1.0e-5f;
            const float depth_scale = projection.At(2, 2);
            const float depth_bias = projection.At(2, 3);
            if (std::fabs(depth_scale) <= kProjectionEpsilon || !std::isfinite(depth_scale) ||
                !std::isfinite(depth_bias)) {
                return 0.01f;
            }

            return std::max(-depth_bias / depth_scale, 0.001f);
        }

        [[nodiscard]] float ExtractProjectionFar(const Math::Mat4 &projection, float near_z) noexcept {
            constexpr float kProjectionEpsilon = 1.0e-5f;
            const float depth_scale = projection.At(2, 2);
            const float depth_bias = projection.At(2, 3);
            float far_z = near_z + 1000.f;
            if (IsPerspectiveProjection(projection)) {
                const float denominator = 1.f - depth_scale;
                if (std::fabs(denominator) > kProjectionEpsilon && std::isfinite(depth_bias)) {
                    far_z = depth_bias / denominator;
                }
            } else if (std::fabs(depth_scale) > kProjectionEpsilon) {
                far_z = near_z + (1.f / depth_scale);
            }

            if (!std::isfinite(far_z) || far_z <= near_z + kProjectionEpsilon) {
                return near_z + 1000.f;
            }

            return far_z;
        }

        [[nodiscard]] float ProjectionHalfWidthAtDepth(const Math::Mat4 &projection, float depth) noexcept {
            constexpr float kProjectionEpsilon = 1.0e-5f;
            const float x_scale = std::fabs(projection.At(0, 0));
            if (x_scale <= kProjectionEpsilon || !std::isfinite(x_scale)) {
                return std::max(depth, 1.f);
            }

            return IsPerspectiveProjection(projection) ? depth / x_scale : 1.f / x_scale;
        }

        [[nodiscard]] float ProjectionHalfHeightAtDepth(const Math::Mat4 &projection, float depth) noexcept {
            constexpr float kProjectionEpsilon = 1.0e-5f;
            const float y_scale = std::fabs(projection.At(1, 1));
            if (y_scale <= kProjectionEpsilon || !std::isfinite(y_scale)) {
                return std::max(depth, 1.f);
            }

            return IsPerspectiveProjection(projection) ? depth / y_scale : 1.f / y_scale;
        }

        [[nodiscard]] std::array<Math::Vec3, 8> BuildFrustumSliceCorners(const CameraData &camera,
                                                                         const Math::Vec3 &camera_position,
                                                                         const Math::Vec3 &camera_right,
                                                                         const Math::Vec3 &camera_up,
                                                                         const Math::Vec3 &camera_forward,
                                                                         float slice_near, float slice_far) {
            const float near_half_width = ProjectionHalfWidthAtDepth(camera.projection, slice_near);
            const float near_half_height = ProjectionHalfHeightAtDepth(camera.projection, slice_near);
            const float far_half_width = ProjectionHalfWidthAtDepth(camera.projection, slice_far);
            const float far_half_height = ProjectionHalfHeightAtDepth(camera.projection, slice_far);
            const Math::Vec3 near_center = camera_position + camera_forward * slice_near;
            const Math::Vec3 far_center = camera_position + camera_forward * slice_far;

            return {
                    near_center - camera_right * near_half_width - camera_up * near_half_height,
                    near_center + camera_right * near_half_width - camera_up * near_half_height,
                    near_center - camera_right * near_half_width + camera_up * near_half_height,
                    near_center + camera_right * near_half_width + camera_up * near_half_height,
                    far_center - camera_right * far_half_width - camera_up * far_half_height,
                    far_center + camera_right * far_half_width - camera_up * far_half_height,
                    far_center - camera_right * far_half_width + camera_up * far_half_height,
                    far_center + camera_right * far_half_width + camera_up * far_half_height,
            };
        }

        [[nodiscard]] DirectionalShadowCascadeData BuildDirectionalShadowCascade(
                const CameraData &camera, const Math::Vec3 &camera_position, const Math::Vec3 &camera_right,
                const Math::Vec3 &camera_up, const Math::Vec3 &camera_forward, const Math::Vec3 &light_direction,
                float slice_near, float slice_far, std::uint32_t shadow_resolution,
                const PbrCascadeSettings &settings) {
            constexpr float kMinCascadeRadius = 1.0f;
            const std::array<Math::Vec3, 8> corners =
                    BuildFrustumSliceCorners(camera, camera_position, camera_right, camera_up, camera_forward,
                                             slice_near, slice_far);

            Math::Vec3 center{0.f, 0.f, 0.f};
            for (const Math::Vec3 &corner: corners) {
                center += corner;
            }
            center = center / static_cast<float>(corners.size());

            float radius = kMinCascadeRadius;
            for (const Math::Vec3 &corner: corners) {
                radius = std::max(radius, Math::Length(corner - center));
            }
            radius *= std::max(settings.bounds_padding, 1.f);

            const Math::Vec3 light_up = std::fabs(light_direction.y) < 0.95f ? Math::Vec3{0.f, 1.f, 0.f}
                                                                             : Math::Vec3{1.f, 0.f, 0.f};
            const Math::Mat4 light_view = Math::LookAtLH({0.f, 0.f, 0.f}, light_direction, light_up);
            Math::Vec3 light_center = TransformPoint(light_view, center);
            if (settings.texel_snap && shadow_resolution > 0u) {
                const float texel_size = (radius * 2.f) / static_cast<float>(shadow_resolution);
                if (texel_size > 0.f && std::isfinite(texel_size)) {
                    light_center.x = std::round(light_center.x / texel_size) * texel_size;
                    light_center.y = std::round(light_center.y / texel_size) * texel_size;
                }
            }

            float min_z = std::numeric_limits<float>::max();
            float max_z = std::numeric_limits<float>::lowest();
            for (const Math::Vec3 &corner: corners) {
                const Math::Vec3 light_corner = TransformPoint(light_view, corner);
                min_z = std::min(min_z, light_corner.z);
                max_z = std::max(max_z, light_corner.z);
            }

            const float depth_padding = radius * std::max(settings.caster_depth_padding, 0.f);
            const float near_z = min_z - depth_padding;
            const float far_z = std::max(max_z + depth_padding, near_z + 1.f);
            const Math::Mat4 projection =
                    Math::OrthoLH(light_center.x - radius, light_center.x + radius, light_center.y - radius,
                                  light_center.y + radius, near_z, far_z);

            return {.view_proj = projection * light_view, .split_depth = slice_far};
        }

        [[nodiscard]] Math::Vec3 PointShadowFaceDirection(std::uint32_t face) {
            static constexpr std::array<Math::Vec3, 6> kDirections{
                    Math::Vec3{1.f, 0.f, 0.f},  Math::Vec3{-1.f, 0.f, 0.f}, Math::Vec3{0.f, 1.f, 0.f},
                    Math::Vec3{0.f, -1.f, 0.f}, Math::Vec3{0.f, 0.f, 1.f},  Math::Vec3{0.f, 0.f, -1.f},
            };
            return kDirections[std::min<std::uint32_t>(face, 5u)];
        }

        [[nodiscard]] Math::Vec3 PointShadowFaceUp(std::uint32_t face) {
            static constexpr std::array<Math::Vec3, 6> kUp{
                    Math::Vec3{0.f, -1.f, 0.f}, Math::Vec3{0.f, -1.f, 0.f}, Math::Vec3{0.f, 0.f, 1.f},
                    Math::Vec3{0.f, 0.f, -1.f}, Math::Vec3{0.f, -1.f, 0.f}, Math::Vec3{0.f, -1.f, 0.f},
            };
            return kUp[std::min<std::uint32_t>(face, 5u)];
        }

        [[nodiscard]] float DebugModeFromName(std::string_view name) {
            if (name == "material/base-color") {
                return 1.f;
            }
            if (name == "material/world-normal") {
                return 2.f;
            }
            if (name == "material/metallic") {
                return 3.f;
            }
            if (name == "material/roughness") {
                return 4.f;
            }
            if (name == "material/ao") {
                return 5.f;
            }
            if (name == "material/emissive") {
                return 6.f;
            }
            if (name == "lighting/shadow-factor") {
                return 7.f;
            }
            if (name == "lighting/cascade-index") {
                return 8.f;
            }
            if (name == "lighting/light-counts") {
                return 9.f;
            }
            if (name == "probes/reflection-influence") {
                return 10.f;
            }
            if (name == "probes/reflection-selected") {
                return 11.f;
            }
            if (name == "ibl/specular-roughness-lod") {
                return 12.f;
            }

            return 0.f;
        }

        [[nodiscard]] std::uint64_t TextureBytes(std::uint32_t width, std::uint32_t height, std::uint32_t slices,
                                                 std::uint32_t bytes_per_pixel) {
            return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) *
                   static_cast<std::uint64_t>(slices) * static_cast<std::uint64_t>(bytes_per_pixel);
        }

        [[nodiscard]] bool FileExists(std::string_view path) {
            if (path.empty()) {
                return false;
            }

            std::error_code error;
            return std::filesystem::exists(std::filesystem::path{path}, error) && !error;
        }

        template <typename Paths>
        [[nodiscard]] std::string MakePrecomputedIblKey(const Paths &paths) {
            return "precomputed:" + paths.environment_cube_path + "|" + paths.irradiance_cube_path + "|" +
                   paths.prefiltered_specular_cube_path + "|" + paths.brdf_lut_path;
        }

        template <typename Paths>
        [[nodiscard]] std::string ResolvePrecomputedIblManifestPath(const Paths &paths) {
            if (!paths.manifest_path.empty()) {
                return paths.manifest_path;
            }
            if (!paths.environment_cube_path.empty()) {
                return paths.environment_cube_path + ".manifest";
            }
            return {};
        }

        [[nodiscard]] std::string MakePathIblSourceKey(std::string_view path) {
            std::string key = "path:" + std::string{path};
            const std::filesystem::path source_path{std::string{path}};
            std::error_code error;
            const auto file_size = std::filesystem::file_size(source_path, error);
            if (!error) {
                key += "|size:" + std::to_string(file_size);
            }
            error.clear();
            const auto write_time = std::filesystem::last_write_time(source_path, error);
            if (!error) {
                key += "|mtime:" + std::to_string(write_time.time_since_epoch().count());
            }
            return key;
        }

        [[nodiscard]] std::string MakeHandleIblSourceKey(std::string_view prefix, TextureHandle handle) {
            return std::string{prefix} + ":" + std::to_string(handle.id) + ":" + std::to_string(handle.generation);
        }

        [[nodiscard]] std::uint32_t ParseManifestUint(const std::unordered_map<std::string, std::string> &values,
                                                      std::string_view key) {
            const auto it = values.find(std::string{key});
            if (it == values.end()) {
                return 0u;
            }

            try {
                return static_cast<std::uint32_t>(std::stoul(it->second));
            } catch (...) {
                return 0u;
            }
        }

        [[nodiscard]] std::unordered_map<std::string, std::string> ReadPbrIblManifest(std::string_view path) {
            std::unordered_map<std::string, std::string> values;
            std::ifstream file{std::filesystem::path{std::string{path}}};
            if (!file) {
                return values;
            }

            std::string line;
            while (std::getline(file, line)) {
                const std::size_t separator = line.find('=');
                if (separator == std::string::npos || separator == 0u) {
                    continue;
                }
                values[line.substr(0u, separator)] = line.substr(separator + 1u);
            }

            return values;
        }

        template <typename Paths>
        [[nodiscard]] bool HasCurrentPrecomputedIbl(const Paths &paths, std::string_view source_key,
                                                    bool require_manifest, std::uint32_t environment_resolution,
                                                    std::uint32_t irradiance_resolution,
                                                    std::uint32_t prefiltered_resolution,
                                                    std::uint32_t prefiltered_mip_count,
                                                    std::uint32_t brdf_lut_resolution) {
            if (!paths.IsComplete() || !FileExists(paths.environment_cube_path) ||
                !FileExists(paths.irradiance_cube_path) || !FileExists(paths.prefiltered_specular_cube_path) ||
                !FileExists(paths.brdf_lut_path)) {
                return false;
            }

            const std::string manifest_path = ResolvePrecomputedIblManifestPath(paths);
            if (manifest_path.empty() || !FileExists(manifest_path)) {
                return !require_manifest;
            }
            if (source_key.empty()) {
                return true;
            }

            const std::unordered_map<std::string, std::string> values = ReadPbrIblManifest(manifest_path);
            const auto version_it = values.find("version");
            const auto source_it = values.find("source_key");
            if (version_it == values.end() || version_it->second != "2" || source_it == values.end() ||
                source_it->second != source_key) {
                return false;
            }

            return ParseManifestUint(values, "environment_resolution") == environment_resolution &&
                   ParseManifestUint(values, "irradiance_resolution") == irradiance_resolution &&
                   ParseManifestUint(values, "prefiltered_resolution") == prefiltered_resolution &&
                   ParseManifestUint(values, "prefiltered_mip_count") == prefiltered_mip_count &&
                   ParseManifestUint(values, "brdf_lut_resolution") == brdf_lut_resolution;
        }

        template <typename Paths>
        [[nodiscard]] bool WritePbrIblManifest(const Paths &paths, std::string_view source_key,
                                               std::uint32_t environment_resolution,
                                               std::uint32_t irradiance_resolution,
                                               std::uint32_t prefiltered_resolution,
                                               std::uint32_t prefiltered_mip_count,
                                               std::uint32_t brdf_lut_resolution) {
            const std::string manifest_path = ResolvePrecomputedIblManifestPath(paths);
            if (manifest_path.empty()) {
                return false;
            }

            const std::filesystem::path output_path{manifest_path};
            const std::filesystem::path parent = output_path.parent_path();
            if (!parent.empty()) {
                std::error_code error;
                std::filesystem::create_directories(parent, error);
                if (error) {
                    return false;
                }
            }

            std::ofstream file{output_path, std::ios::trunc};
            if (!file) {
                return false;
            }

            file << "version=2\n";
            file << "source_key=" << source_key << '\n';
            file << "environment_resolution=" << environment_resolution << '\n';
            file << "irradiance_resolution=" << irradiance_resolution << '\n';
            file << "prefiltered_resolution=" << prefiltered_resolution << '\n';
            file << "prefiltered_mip_count=" << prefiltered_mip_count << '\n';
            file << "brdf_lut_resolution=" << brdf_lut_resolution << '\n';
            return static_cast<bool>(file);
        }

        [[nodiscard]] bool IsColorFrameBufferFormat(FrameBufferFormat format) {
            switch (format) {
                case FrameBufferFormat::SwapChainColor:
                case FrameBufferFormat::RGBA8Unorm:
                case FrameBufferFormat::RGBA16Float:
                case FrameBufferFormat::R32Float:       return true;
                case FrameBufferFormat::SwapChainDepth:
                case FrameBufferFormat::Depth32Float:   return false;
            }

            return false;
        }

        [[nodiscard]] ToneMappingOperator SanitizeToneMapping(ToneMappingOperator tone_mapping) {
            switch (tone_mapping) {
                case ToneMappingOperator::None:
                case ToneMappingOperator::Reinhard:
                case ToneMappingOperator::AcesFilmic: return tone_mapping;
            }

            return ToneMappingOperator::AcesFilmic;
        }

        [[nodiscard]] PostProcessDesc SanitizePostProcess(PostProcessDesc desc) {
            constexpr float kMaxExposure = 65504.0f;
            desc.exposure = std::isfinite(desc.exposure) ? std::clamp(desc.exposure, 0.0f, kMaxExposure) : 1.0f;
            desc.tone_mapping = SanitizeToneMapping(desc.tone_mapping);
            return desc;
        }

        [[nodiscard]] float PositiveFiniteOrZero(float value) {
            return std::isfinite(value) ? std::max(value, 0.f) : 0.f;
        }

        [[nodiscard]] std::uint32_t ClampAtLeastOne(std::uint32_t value, std::uint32_t max_value) {
            return std::clamp(value == 0u ? 1u : value, 1u, max_value);
        }

        [[nodiscard]] PbrRenderSettings ApplyPbrPreset(PbrRenderSettings settings) {
            switch (settings.preset) {
                case PbrQualityPreset::Low:    return PbrRenderSettings::Low();
                case PbrQualityPreset::Medium: return PbrRenderSettings::Medium();
                case PbrQualityPreset::High:   return PbrRenderSettings::High();
                case PbrQualityPreset::Ultra:  return PbrRenderSettings::Ultra();
                case PbrQualityPreset::Custom: return settings;
            }

            return PbrRenderSettings::Medium();
        }

        [[nodiscard]] PbrRenderSettings SanitizePbrSettings(PbrRenderSettings settings) {
            const bool keep_visual_debug = settings.visual_debug;
            settings = ApplyPbrPreset(settings);
            settings.visual_debug = keep_visual_debug;
            settings.shadows.cascade_count = ClampAtLeastOne(settings.shadows.cascade_count,
                                                             static_cast<std::uint32_t>(kMaxPbrCascades));
            settings.shadows.cascades.split_lambda =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.cascades.split_lambda), 0.f, 1.f);
            settings.shadows.cascades.bounds_padding =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.cascades.bounds_padding), 1.f, 2.f);
            settings.shadows.cascades.caster_depth_padding =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.cascades.caster_depth_padding), 0.f, 8.f);
            settings.shadows.directional_shadow_resolution =
                    std::clamp(settings.shadows.directional_shadow_resolution, 128u, 8192u);
            settings.shadows.directional_shadow_distance =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.directional_shadow_distance), 1.f, 10000.f);
            settings.shadows.directional_shadow_bias =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.directional_shadow_bias), 0.f, 0.1f);
            settings.shadows.directional_shadow_normal_bias =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.directional_shadow_normal_bias), 0.f, 10.f);
            settings.shadows.directional_shadow_pcf_radius =
                    std::clamp(settings.shadows.directional_shadow_pcf_radius, 0u, 4u);
            settings.shadows.max_shadowed_point_lights =
                    std::min<std::uint32_t>(settings.shadows.max_shadowed_point_lights,
                                            static_cast<std::uint32_t>(kMaxPbrShadowedPointLights));
            settings.shadows.point_shadow_resolution =
                    std::clamp(settings.shadows.point_shadow_resolution, 64u, 4096u);
            settings.shadows.point_shadow_bias =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.point_shadow_bias), 0.f, 0.1f);
            settings.shadows.point_shadow_normal_bias =
                    std::clamp(PositiveFiniteOrZero(settings.shadows.point_shadow_normal_bias), 0.f, 10.f);
            settings.shadows.point_shadow_pcf_radius = std::clamp(settings.shadows.point_shadow_pcf_radius, 0u, 3u);

            settings.ibl.environment_cube_resolution =
                    std::clamp(settings.ibl.environment_cube_resolution, 16u, 2048u);
            settings.ibl.irradiance_resolution = std::clamp(settings.ibl.irradiance_resolution, 8u, 512u);
            settings.ibl.prefiltered_specular_resolution =
                    std::clamp(settings.ibl.prefiltered_specular_resolution, 16u, 2048u);
            settings.ibl.prefiltered_specular_mip_count =
                    ClampAtLeastOne(settings.ibl.prefiltered_specular_mip_count, 12u);
            settings.ibl.brdf_lut_resolution = std::clamp(settings.ibl.brdf_lut_resolution, 16u, 1024u);
            return settings;
        }

        [[nodiscard]] RenderDesc SanitizeRenderDesc(RenderDesc desc) {
            if (!IsColorFrameBufferFormat(desc.scene_color_format)) {
                desc.scene_color_format = FrameBufferFormat::RGBA16Float;
            }

            desc.post_process = SanitizePostProcess(desc.post_process);
            desc.pbr = SanitizePbrSettings(desc.pbr);
            return desc;
        }

        [[nodiscard]] DirectionalLightFrameData ResolveDirectionalLight(World &world) {
            constexpr float kDirectionEpsilon = 1.0e-8f;

            auto view = world.View<DirectionalLightComponent>();
            for (auto entity: view) {
                const DirectionalLightComponent &light = view.get<DirectionalLightComponent>(entity);
                if (!light.enabled || light.illuminance_lux <= 0.f) {
                    continue;
                }

                Math::Vec3 direction = light.direction;
                if (Math::LengthSquared(direction) <= kDirectionEpsilon) {
                    direction = {0.f, -1.f, 0.f};
                } else {
                    direction = Math::Normalize(direction);
                }

                return DirectionalLightFrameData{
                        .direction = direction,
                        .illuminance_lux = light.illuminance_lux,
                        .color = light.color,
                        .enabled = true,
                };
            }

            return {};
        }

        [[nodiscard]] std::uint32_t ResolvePointLights(
                World &world, std::unordered_map<entt::entity, Math::Mat4> &world_transform_cache,
                std::array<PointLightFrameData, kMaxPbrPointLights> &out_point_lights,
                std::uint32_t max_shadowed_point_lights) {
            constexpr float kMinPointLightRange = 0.001f;

            std::uint32_t light_count = 0u;
            std::uint32_t shadowed_light_count = 0u;
            auto view = world.View<PointLightComponent>();
            for (auto entity: view) {
                if (light_count >= kMaxPbrPointLights) {
                    break;
                }

                const PointLightComponent &light = view.get<PointLightComponent>(entity);
                const float intensity = PositiveFiniteOrZero(light.luminous_intensity_cd);
                const float range = PositiveFiniteOrZero(light.range);
                if (!light.enabled || intensity <= 0.f || range <= kMinPointLightRange) {
                    continue;
                }

                const TransformComponent *transform = world.TryGetComponent<TransformComponent>(entity);
                if (transform == nullptr) {
                    continue;
                }

                const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
                const Math::Mat4 world_matrix = hierarchy == nullptr || hierarchy->parent == entt::null
                                                        ? transform->WorldMatrix()
                                                        : ResolveCachedWorldMatrix(world, entity,
                                                                                   world_transform_cache);

                const bool casts_shadows = light.cast_shadows && shadowed_light_count < max_shadowed_point_lights;
                out_point_lights[light_count++] = PointLightFrameData{
                        .position = ExtractWorldPosition(world_matrix),
                        .range = range,
                        .color =
                                {PositiveFiniteOrZero(light.color.x), PositiveFiniteOrZero(light.color.y),
                                 PositiveFiniteOrZero(light.color.z)},
                        .luminous_intensity_cd = intensity,
                        .shadow_near_z = std::clamp(PositiveFiniteOrZero(light.shadow_near_z), 0.001f, range),
                        .shadow_bias = std::clamp(PositiveFiniteOrZero(light.shadow_bias), 0.f, 0.1f),
                        .shadow_normal_bias = std::clamp(PositiveFiniteOrZero(light.shadow_normal_bias), 0.f, 10.f),
                        .casts_shadows = casts_shadows,
                        .shadow_index = casts_shadows ? shadowed_light_count++ : 0u,
                };
            }

            return light_count;
        }

        [[nodiscard]] EnvironmentLightFrameData ResolveEnvironmentLight(World &world) {
            auto view = world.View<EnvironmentLightComponent>();
            for (auto entity: view) {
                const EnvironmentLightComponent &light = view.get<EnvironmentLightComponent>(entity);
                const float diffuse_intensity = PositiveFiniteOrZero(light.intensity);
                const float specular_intensity = PositiveFiniteOrZero(light.specular_intensity);
                if (!light.enabled || (diffuse_intensity <= 0.f && specular_intensity <= 0.f)) {
                    continue;
                }

                return EnvironmentLightFrameData{
                        .diffuse_irradiance =
                                {PositiveFiniteOrZero(light.diffuse_irradiance.x),
                                 PositiveFiniteOrZero(light.diffuse_irradiance.y),
                                 PositiveFiniteOrZero(light.diffuse_irradiance.z)},
                        .intensity = diffuse_intensity,
                        .specular_radiance =
                                {PositiveFiniteOrZero(light.specular_radiance.x),
                                 PositiveFiniteOrZero(light.specular_radiance.y),
                                 PositiveFiniteOrZero(light.specular_radiance.z)},
                        .specular_intensity = specular_intensity,
                        .enabled = true,
                };
            }

            return {};
        }

        struct ActiveReflectionProbe {
            TextureHandle environment_map{};
            Math::Vec3 position{0.f, 0.f, 0.f};
            float radius = 0.f;
            float intensity = 1.f;
            std::int32_t priority = 0;
            bool enabled = false;
        };

        struct PbrSkyboxParams {
            Math::Vec4 camera_right_tan_x{};
            Math::Vec4 camera_up_tan_y{};
            Math::Vec4 camera_forward_intensity{};
            Math::Vec4 fallback_horizon{};
            Math::Vec4 fallback_zenith{};
        };

        [[nodiscard]] bool ContainsProbeReference(const ReflectionProbeComponent &probe,
                                                  const Math::Vec3 &probe_position,
                                                  const Math::Vec3 &reference_position,
                                                  float radius) {
            const Math::Vec3 delta = reference_position - probe_position;
            const bool uses_box = probe.box_extent.x > 0.f || probe.box_extent.y > 0.f || probe.box_extent.z > 0.f;
            if (uses_box) {
                return std::fabs(delta.x) <= std::max(probe.box_extent.x, 0.f) &&
                       std::fabs(delta.y) <= std::max(probe.box_extent.y, 0.f) &&
                       std::fabs(delta.z) <= std::max(probe.box_extent.z, 0.f);
            }

            return Math::LengthSquared(delta) <= radius * radius;
        }

        [[nodiscard]] ActiveReflectionProbe ResolveActiveReflectionProbe(
                World &world, std::unordered_map<entt::entity, Math::Mat4> &world_transform_cache,
                const Math::Vec3 &reference_position) {
            ActiveReflectionProbe selected{};
            float selected_distance_sq = 0.f;

            auto view = world.View<ReflectionProbeComponent>();
            for (auto entity: view) {
                const ReflectionProbeComponent &probe = view.get<ReflectionProbeComponent>(entity);
                const float radius = std::max(PositiveFiniteOrZero(probe.radius), 0.001f);
                if (!probe.enabled || (!probe.environment_map.IsValid() && !probe.use_scene_environment) ||
                    PositiveFiniteOrZero(probe.intensity) <= 0.f) {
                    continue;
                }

                const TransformComponent *transform = world.TryGetComponent<TransformComponent>(entity);
                const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
                const Math::Mat4 world_matrix =
                        transform == nullptr
                                ? Math::Identity()
                                : (hierarchy == nullptr || hierarchy->parent == entt::null
                                           ? transform->WorldMatrix()
                                           : ResolveCachedWorldMatrix(world, entity, world_transform_cache));
                const Math::Vec3 probe_position = ExtractWorldPosition(world_matrix);
                if (!ContainsProbeReference(probe, probe_position, reference_position, radius)) {
                    continue;
                }

                const float distance_sq = Math::LengthSquared(reference_position - probe_position);
                const bool better_probe = !selected.enabled || probe.priority > selected.priority ||
                                          (probe.priority == selected.priority &&
                                           distance_sq < selected_distance_sq);
                if (!better_probe) {
                    continue;
                }

                selected = ActiveReflectionProbe{
                        .environment_map = probe.environment_map,
                        .position = probe_position,
                        .radius = radius,
                        .intensity = PositiveFiniteOrZero(probe.intensity),
                        .priority = probe.priority,
                        .enabled = true,
                };
                selected_distance_sq = distance_sq;
            }

            return selected;
        }
    } // namespace

    //clang-format off
    constexpr RenderPassStage kScenePassStages[] = {
            RenderPassStage::FrameSetup,         // Per-frame setup before any scene rendering.
            RenderPassStage::Shadow,             // Renders shadows maps and other light-space depth resources
            RenderPassStage::DepthPrePass,       // Fills scene depth before color rendering
            RenderPassStage::GBuffer,            // Writes deferred rendering geometry buffers
            RenderPassStage::Lighting,           // Computes lighting from scene/material buffers
            RenderPassStage::ForwardOpaque,      // Renders opaque forward geometry
            RenderPassStage::ForwardTransparent, // Renders transparent forward geometry after opaque
            RenderPassStage::PostProcess,        // Applies fullscreen effects after scene rendering
            RenderPassStage::Debug,              // Produces debug overlays or debug textures
    };
    //clang-format on

    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend,
                               std::unique_ptr<IModelImporter> model_importer) :
        backend_(std::move(backend)), model_importer_(std::move(model_importer)),
        models_(std::make_unique<ModelRegistry>()) {}

    RenderSystem::~RenderSystem() {
        if (initialized_ && backend_ != nullptr) {
            render_graph_.Clear(backend_.get());
            DestroyPbrResources();
            DestroySceneFrameBuffer();
        }
        DestroyAllModels();
    }

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_ = SanitizeRenderDesc(desc);
        surface_width_ = desc.width > 0 ? desc.width : 1;
        surface_height_ = desc.height > 0 ? desc.height : 1;

        default_camera_ = Camera{}.LookAt({0.f, 0.f, -5.f}, {0.f, 0.f, 0.f})
                                  .Perspective(60.f, static_cast<float>(surface_width_),
                                               static_cast<float>(surface_height_), 0.01f, 1000.f)
                                  .GetCameraData();

        initialized_ = backend_ != nullptr && backend_->Initialize(desc_, native_window);
        if (initialized_) {
            initialized_ = CreateSceneFrameBuffer();
            if (initialized_) {
                initialized_ = CreatePbrResources();
            }
            if (initialized_) {
                pbr_ibl_pass_ = render_graph_.AddPass(std::make_unique<PbrIblPass>(*this));
                pbr_shadow_pass_ = render_graph_.AddPass(std::make_unique<PbrShadowPass>(*this));
                default_scene_pass_ = render_graph_.AddPass(std::make_unique<DefaultSceneRenderPass>(*this));
                pbr_debug_pass_ = render_graph_.AddPass(std::make_unique<PbrDebugPass>(*this));
            }
        }

        return initialized_;
    }

    void RenderSystem::BeginImGuiFrame() const {
        if (!initialized_ || backend_ == nullptr || !desc_.enable_imgui) {
            return;
        }

        backend_->BeginImGuiFrame();
    }

    void RenderSystem::RenderFrame(World &world, const FrameClock &frame_clock, float delta_seconds) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }

        const auto frame_start = RenderCpuClock::now();
        const auto upload_start = RenderCpuClock::now();
        PumpModelUploads();
        const auto upload_end = RenderCpuClock::now();

        render_frame_resources_.Clear();
        debug_registry_.BeginFrame();
        RenderDebugStats &debug_stats = debug_registry_.Stats();
        debug_stats.model_upload_cpu_ms = CpuElapsedMilliseconds(upload_start, upload_end);

        backend_->BeginFrame();

        const RenderFrameTiming timing{
                .delta_seconds = delta_seconds,
                .total_seconds = frame_clock.TotalSeconds(),
                .frame_index = frame_clock.FrameIndex(),
        };

        // preserves the time snapshot to pass to all the render passes equally
        RenderPassContext pass_context{*backend_,      world,          frame_clock, timing, render_frame_resources_,
                                       surface_width_, surface_height_};

        const auto execute_stage = [&](RenderPassStage stage) {
            const auto stage_start = RenderCpuClock::now();
            render_graph_.Execute(stage, pass_context);
            AccumulateStageCpuTime(debug_stats, stage, CpuElapsedMilliseconds(stage_start, RenderCpuClock::now()));
        };

        for (const RenderPassStage &stage: kScenePassStages) {
            execute_stage(stage);
        }

        const auto composite_start = RenderCpuClock::now();
        backend_->CompositeFrameBuffer(scene_framebuffer_, desc_.post_process);
        debug_stats.composite_cpu_ms = CpuElapsedMilliseconds(composite_start, RenderCpuClock::now());

        execute_stage(RenderPassStage::UI);
        backend_->SetSwapChainFrameBuffer();

        if (desc_.enable_imgui) {
            const auto imgui_start = RenderCpuClock::now();
            backend_->RenderImGui();
            debug_stats.imgui_cpu_ms = CpuElapsedMilliseconds(imgui_start, RenderCpuClock::now());
        }

        execute_stage(RenderPassStage::Present);

        backend_->EndFrame();
        debug_stats.frame_cpu_ms = CpuElapsedMilliseconds(frame_start, RenderCpuClock::now());
    }

    MeshHandle RenderSystem::GetOrCreatePrimitive(PrimitiveType type) {
        const auto index = static_cast<std::size_t>(type);
        if (index >= primitive_cache_.size()) {
            return {};
        }

        MeshHandle &cached = primitive_cache_[index];
        if (cached.IsValid()) {
            return cached;
        }

        const MeshDesc desc = Primitives::MeshFor(type);
        if (!desc.IsValid()) {
            return {};
        }

        cached = CreateMesh(desc);
        return cached;
    }

    MeshHandle RenderSystem::CreateMesh(const MeshDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->UploadMesh(desc);
    }

    bool RenderSystem::SetPrimitiveRenderer(Node node, const PrimitiveRendererDesc &desc) {
        if (!node.IsValid()) {
            return false;
        }

        const MeshHandle mesh = GetOrCreatePrimitive(desc.type);
        const MaterialHandle material = desc.material.Resolve(*this);
        if (!mesh.IsValid() || !material.IsValid()) {
            return false;
        }

        MeshRendererComponent renderer{
                .mesh = mesh,
                .material = material,
                .visible = desc.visible,
                .cast_shadows = desc.cast_shadows,
                .mobility = desc.mobility,
                .topology = desc.topology,
        };

        if (MeshRendererComponent *existing = node.TryGetComponent<MeshRendererComponent>()) {
            *existing = renderer;
            return true;
        }

        node.AddComponent<MeshRendererComponent>(renderer);
        return true;
    }

    MaterialHandle RenderSystem::ResolveMaterial(const MaterialDesc &desc) {
        if (!initialized_ || backend_ == nullptr) {
            return {};
        }

        return backend_->ResolveMaterial(desc);
    }

    ShaderProgramHandle RenderSystem::CreateShaderProgram(const ShaderProgramDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->CreateShaderProgram(desc);
    }

    TextureHandle RenderSystem::LoadTexture2D(const TextureLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }
        return backend_->LoadTexture2D(desc);
    }

    TextureHandle RenderSystem::LoadTexture2DAsync(const TextureLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }
        return backend_->LoadTexture2DAsync(desc);
    }

    TextureLoadState RenderSystem::GetTextureLoadState(TextureHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return TextureLoadState::Invalid;
        }
        return backend_->GetTextureLoadState(handle);
    }

    void RenderSystem::DestroyTexture(TextureHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }
        backend_->DestroyTexture(handle);
    }

    ModelHandle RenderSystem::LoadModel(const ModelLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || model_importer_ == nullptr || models_ == nullptr ||
            !desc.IsValid()) {
            return {};
        }

        ModelLoadResult result;
        try {
            result = model_importer_->Load(desc);
        } catch (const std::exception &ex) {
            result.error_message = ex.what();
        }
        if (!result.IsSuccess()) {
            return {};
        }

        UploadedModelResources resources = BuildModelResources(result.asset, desc.material_pipeline);
        if (!resources.IsSuccess()) {
            return {};
        }

        ModelRegistry::Record record;
        record.state = ModelLoadState::Ready;
        record.meshes = std::move(resources.meshes);
        record.mesh_names = std::move(resources.mesh_names);
        record.materials = std::move(resources.materials);
        record.mesh_material_indices = std::move(resources.mesh_material_indices);
        record.nodes = std::move(resources.nodes);
        record.material_pipeline = desc.material_pipeline;

        std::lock_guard lock{models_->mutex};
        const uint32_t id = models_->next_model_id++;
        record.generation = models_->model_generation++;
        const uint32_t generation = record.generation;
        models_->records[id] = std::move(record);
        return ModelHandle{.id = id, .generation = generation};
    }

    ModelHandle RenderSystem::LoadModelAsync(const ModelLoadDesc &desc) { return StartModelLoadAsync(desc).handle; }

    Future<ModelHandle> RenderSystem::LoadModelAsyncFuture(const ModelLoadDesc &desc) {
        return StartModelLoadAsync(desc).future;
    }

    RenderSystem::AsyncModelLoadRequest RenderSystem::StartModelLoadAsync(const ModelLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || model_importer_ == nullptr || models_ == nullptr ||
            !desc.IsValid()) {
            return AsyncModelLoadRequest{
                    .handle = {},
                    .future = Future<ModelHandle>::Failed("Invalid asynchronous model load request"),
            };
        }

        ModelHandle handle;
        Future<ModelHandle> future;
        {
            std::lock_guard lock{models_->mutex};
            handle = ModelHandle{
                    .id = models_->next_model_id++,
                    .generation = models_->model_generation++,
            };

            ModelRegistry::Record record;
            record.generation = handle.generation;
            record.state = ModelLoadState::Pending;
            record.material_pipeline = desc.material_pipeline;
            future = record.completion.GetFuture();
            models_->records[handle.id] = std::move(record);
        }

        EnsureModelLoadWorker();
        {
            std::lock_guard lock{models_->load_queue_mutex};
            models_->load_queue.push_back(ModelRegistry::LoadTask{
                    .handle = handle,
                    .desc = desc,
            });
        }
        models_->load_event.notify_one();

        return AsyncModelLoadRequest{
                .handle = handle,
                .future = future,
        };
    }

    void RenderSystem::EnsureModelLoadWorker() {
        if (models_ == nullptr || model_importer_ == nullptr) {
            return;
        }

        std::lock_guard lock{models_->load_queue_mutex};
        if (models_->load_worker_started) {
            return;
        }

        ModelRegistry *registry = models_.get();
        IModelImporter *importer = model_importer_.get();
        models_->load_worker_started = true;
        models_->load_worker = std::jthread{[registry, importer](std::stop_token stop_token) {
            while (!stop_token.stop_requested()) {
                ModelRegistry::LoadTask task;
                {
                    std::unique_lock queue_lock{registry->load_queue_mutex};
                    const bool has_task = registry->load_event.wait(
                            queue_lock, stop_token, [registry] { return !registry->load_queue.empty(); });

                    if (!has_task || stop_token.stop_requested()) {
                        return;
                    }

                    task = std::move(registry->load_queue.front());
                    registry->load_queue.pop_front();
                }

                ModelLoadResult result;
                try {
                    result = importer->Load(task.desc);
                } catch (const std::exception &ex) {
                    result.error_message = ex.what();
                }

                if (stop_token.stop_requested()) {
                    return;
                }

                std::lock_guard record_lock{registry->mutex};
                const auto it = registry->records.find(task.handle.id);
                if (it == registry->records.end() || it.value().generation != task.handle.generation) {
                    continue;
                }

                it.value().decoded_result = std::make_unique<ModelLoadResult>(std::move(result));
            }
        }};
    }

    void RenderSystem::StopModelLoadWorker() {
        if (models_ == nullptr || !models_->load_worker_started) {
            return;
        }

        std::jthread worker;
        {
            std::lock_guard lock{models_->load_queue_mutex};
            models_->load_worker.request_stop();
            worker = std::move(models_->load_worker);
            models_->load_queue.clear();
            models_->load_worker_started = false;
        }
        models_->load_event.notify_all();
    }

    void RenderSystem::RemoveQueuedModelLoad(ModelHandle handle) {
        if (models_ == nullptr || !handle.IsValid()) {
            return;
        }

        std::lock_guard lock{models_->load_queue_mutex};
        for (auto it = models_->load_queue.begin(); it != models_->load_queue.end();) {
            if (it->handle == handle) {
                it = models_->load_queue.erase(it);
                continue;
            }

            ++it;
        }
    }

    ModelLoadState RenderSystem::GetModelLoadState(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return ModelLoadState::Invalid;
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation) {
            return ModelLoadState::Invalid;
        }

        return it.value().state;
    }

    std::size_t RenderSystem::GetModelMeshCount(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return 0;
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready) {
            return 0;
        }

        return it.value().meshes.size();
    }

    MeshHandle RenderSystem::GetModelMesh(ModelHandle handle, std::size_t mesh_index) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready || mesh_index >= it.value().meshes.size()) {
            return {};
        }

        return it.value().meshes[mesh_index];
    }

    std::size_t RenderSystem::GetModelMaterialCount(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return 0;
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready) {
            return 0;
        }

        return it.value().materials.size();
    }

    MaterialHandle RenderSystem::GetModelMaterial(ModelHandle handle, std::size_t material_index) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready || material_index >= it.value().materials.size()) {
            return {};
        }

        return it.value().materials[material_index];
    }

    MaterialHandle RenderSystem::GetModelMeshMaterial(ModelHandle handle, std::size_t mesh_index) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready || mesh_index >= it.value().mesh_material_indices.size()) {
            return {};
        }

        const std::uint32_t material_index = it.value().mesh_material_indices[mesh_index];
        if (material_index >= it.value().materials.size()) {
            return {};
        }

        return it.value().materials[material_index];
    }

    ModelInstance RenderSystem::InstantiateModel(World &world, ModelHandle handle, Node parent,
                                                 const ModelInstantiationDesc &desc) const {
        ModelInstance instance;
        if (!handle.IsValid() || models_ == nullptr || (parent.IsValid() && parent.OwnerWorld() != &world)) {
            return instance;
        }

        std::vector<MeshHandle> meshes;
        std::vector<std::string> mesh_names;
        std::vector<MaterialHandle> materials;
        std::vector<std::uint32_t> mesh_material_indices;
        std::vector<ModelNodeAsset> nodes;
        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->records.find(handle.id);
            if (it == models_->records.end() || it.value().generation != handle.generation ||
                it.value().state != ModelLoadState::Ready) {
                return instance;
            }

            meshes = it.value().meshes;
            mesh_names = it.value().mesh_names;
            materials = it.value().materials;
            mesh_material_indices = it.value().mesh_material_indices;
            nodes = it.value().nodes;
        }

        const std::string root_name = desc.root_name.empty() ? "Model" : desc.root_name;
        instance.root = world.CreateNode(root_name);
        if (parent.IsValid() && !instance.root.SetParent(parent)) {
            instance.root.Destroy();
            instance = {};
            return instance;
        }

        std::vector<Node> created_nodes;
        created_nodes.resize(nodes.size());
        instance.nodes.reserve(nodes.size());
        instance.mesh_nodes.reserve(meshes.size());

        for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index) {
            const ModelNodeAsset &node_asset = nodes[node_index];
            Node node = world.CreateNode(MakeModelNodeName(node_asset, node_index));
            node.SetLocalMatrix(node_asset.local_transform);

            const bool has_valid_parent =
                    node_asset.parent_index < created_nodes.size() && created_nodes[node_asset.parent_index].IsValid();
            node.SetParent(has_valid_parent ? created_nodes[node_asset.parent_index] : instance.root);

            created_nodes[node_index] = node;
            instance.nodes.push_back(node);
        }

        if (created_nodes.empty()) {
            created_nodes.push_back(instance.root);
        }

        for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index) {
            const Node parent_node = created_nodes[node_index].IsValid() ? created_nodes[node_index] : instance.root;
            for (const std::uint32_t mesh_index: nodes[node_index].mesh_indices) {
                if (mesh_index >= meshes.size() || mesh_index >= mesh_material_indices.size()) {
                    continue;
                }

                const std::uint32_t material_index = mesh_material_indices[mesh_index];
                if (material_index >= materials.size()) {
                    continue;
                }

                Node mesh_node = world.CreateNode(MakeModelMeshNodeName(mesh_names[mesh_index], mesh_index));
                mesh_node.SetParent(parent_node);
                mesh_node.AddComponent<MeshRendererComponent>(MeshRendererComponent{
                        .mesh = meshes[mesh_index],
                        .material = materials[material_index],
                        .visible = desc.visible,
                });
                instance.mesh_nodes.push_back(mesh_node);
            }
        }

        if (nodes.empty()) {
            for (std::size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
                if (mesh_index >= mesh_material_indices.size()) {
                    continue;
                }

                const std::uint32_t material_index = mesh_material_indices[mesh_index];
                if (material_index >= materials.size()) {
                    continue;
                }

                Node mesh_node = world.CreateNode(MakeModelMeshNodeName(mesh_names[mesh_index], mesh_index));
                mesh_node.SetParent(instance.root);
                mesh_node.AddComponent<MeshRendererComponent>(MeshRendererComponent{
                        .mesh = meshes[mesh_index],
                        .material = materials[material_index],
                        .visible = desc.visible,
                });
                instance.mesh_nodes.push_back(mesh_node);
            }
        }

        return instance;
    }

    std::string RenderSystem::GetModelLoadError(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation) {
            return {};
        }

        return it.value().error_message;
    }

    void RenderSystem::DestroyModel(ModelHandle handle) {
        if (!handle.IsValid() || models_ == nullptr) {
            return;
        }

        ModelRegistry::Record record;
        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->records.find(handle.id);
            if (it == models_->records.end() || it.value().generation != handle.generation) {
                return;
            }

            record = std::move(it.value());
            models_->records.erase(it);
        }

        if (record.state == ModelLoadState::Pending) {
            RemoveQueuedModelLoad(handle);
            record.completion.Cancel("Model load was cancelled");
        }

        if (backend_ != nullptr) {
            for (MeshHandle mesh: record.meshes) {
                backend_->DestroyMesh(mesh);
            }
        }
    }

    void RenderSystem::DestroyShaderProgram(ShaderProgramHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyShaderProgram(handle);
    }

    void RenderSystem::DestroyMesh(MeshHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyMesh(handle);

        for (MeshHandle &primitive: primitive_cache_) {
            if (primitive == handle) {
                primitive = {};
            }
        }
    }

    FrameBufferHandle RenderSystem::CreateFrameBuffer(const FrameBufferDesc &desc) const {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->CreateFrameBuffer(desc);
    }

    void RenderSystem::DestroyFrameBuffer(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyFrameBuffer(handle);
    }

    void RenderSystem::SetFrameBuffer(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->SetFrameBuffer(handle);
    }

    void RenderSystem::SetSwapChainFrameBuffer() const {
        if (backend_ == nullptr) {
            return;
        }

        backend_->SetSwapChainFrameBuffer();
    }

    void RenderSystem::Clear(const RenderClearColor &clear_color) const {
        if (backend_ == nullptr) {
            return;
        }

        backend_->Clear(clear_color);
    }

    FrameBufferColorView RenderSystem::GetFrameBufferColorView(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return {};
        }

        return backend_->GetFrameBufferColorView(handle);
    }

    FrameBufferDepthView RenderSystem::GetFrameBufferDepthView(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return {};
        }

        return backend_->GetFrameBufferDepthView(handle);
    }

    RenderPassHandle RenderSystem::AddRenderPass(std::unique_ptr<IRenderPass> pass) {
        return render_graph_.AddPass(std::move(pass));
    }

    void RenderSystem::RemoveRenderPass(RenderPassHandle handle) { render_graph_.RemovePass(handle, backend_.get()); }

    void RenderSystem::SetCamera(const Camera &camera) {
        manual_camera_override_ = camera.GetCameraData();
        has_manual_camera_override_ = true;
    }

    void RenderSystem::SetCamera(const CameraData &camera_data) {
        manual_camera_override_ = camera_data;
        has_manual_camera_override_ = true;
    }

    void RenderSystem::ClearCameraOverride() { has_manual_camera_override_ = false; }

    void RenderSystem::SetPostProcess(PostProcessDesc desc) { desc_.post_process = SanitizePostProcess(desc); }

    const PostProcessDesc &RenderSystem::GetPostProcess() const { return desc_.post_process; }

    void RenderSystem::Resize(int width, int height) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }
        DestroySceneFrameBuffer();
        surface_width_ = width > 0 ? width : 1;
        surface_height_ = height > 0 ? height : 1;
        backend_->Resize(surface_width_, surface_height_);
        initialized_ = CreateSceneFrameBuffer();
    }

    void RenderSystem::Shutdown() {
        default_scene_pass_ = {};
        pbr_shadow_pass_ = {};
        pbr_ibl_pass_ = {};
        pbr_debug_pass_ = {};
        render_graph_.Clear(backend_.get());

        DestroyAllModels();
        DestroyPbrResources();
        DestroySceneFrameBuffer();

        if (backend_ != nullptr) {
            backend_->Shutdown();
        }

        primitive_cache_.fill({});
        pbr_shadow_frame_data_ = {};
        pbr_global_resources_ = {};
        initialized_ = false;
    }

    bool RenderSystem::IsInitialized() const { return initialized_; }

    std::string_view RenderSystem::LastError() const {
        if (backend_ == nullptr) {
            return "Render backend is not available";
        }

        return backend_->LastError();
    }

    IRenderContext &RenderSystem::Context() { return *this; }

    RenderGraph &RenderSystem::Graph() { return render_graph_; }

    void RenderSystem::SetDebugView(RenderDebugView view) {
        if (!view.IsValid()) {
            return;
        }

        const std::string name = view.name;
        debug_registry_.RegisterView(std::move(view));
        debug_registry_.Select(name);
    }

    bool RenderSystem::SetDebugView(std::string_view name) {
        if (debug_registry_.Find(name) == nullptr) {
            return false;
        }

        debug_registry_.Select(name);
        return true;
    }

    void RenderSystem::ClearDebugView() { debug_registry_.ClearSelection(); }

    std::span<const RenderDebugView> RenderSystem::GetAvailableDebugViews() const {
        return debug_registry_.Views();
    }

    const RenderDebugStats &RenderSystem::GetDebugStats() const { return debug_registry_.Stats(); }

    bool RenderSystem::CreatePbrResources() {
        if (backend_ == nullptr) {
            return false;
        }

        if (!CreatePbrShadowResources()) {
            DestroyPbrResources();
            return false;
        }

        if (!CreatePbrIblResources()) {
            DestroyPbrResources();
            return false;
        }

        pbr_shadow_depth_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_ShadowDepth",
                .vertex_shader_source = BuiltinShaders::kPbrShadowDepthVS,
                .pixel_shader_source = {},
                .bindings = {},
                .has_color_target = false,
                .depth_format = FrameBufferFormat::Depth32Float,
                .depth_test = true,
        });
        pbr_skybox_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_EnvironmentSkybox",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrSkyboxPS,
                .bindings = {ShaderBindingDesc::Texture("g_SkyboxCube", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_SkyboxSampler"),
                             ShaderBindingDesc::UniformBuffer("Skybox", sizeof(PbrSkyboxParams),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
                .color_format = desc_.scene_color_format,
                .depth_format = FrameBufferFormat::Depth32Float,
        });
        pbr_skybox_fallback_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_EnvironmentSkyboxFallback",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrSkyboxFallbackPS,
                .bindings = {ShaderBindingDesc::UniformBuffer("Skybox", sizeof(PbrSkyboxParams),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
                .color_format = desc_.scene_color_format,
                .depth_format = FrameBufferFormat::Depth32Float,
        });

        pbr_debug_texture_2d_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_DebugTexture2D",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrDebugTexture2DPS,
                .bindings = {ShaderBindingDesc::Texture("g_DebugTexture", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_DebugSampler"),
                             ShaderBindingDesc::UniformBuffer("DebugTexture", sizeof(Math::Vec4),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
        });
        pbr_debug_texture_array_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_DebugTextureArray",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrDebugTexture2DArrayPS,
                .bindings = {ShaderBindingDesc::Texture("g_DebugTexture", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_DebugSampler"),
                             ShaderBindingDesc::UniformBuffer("DebugTexture", sizeof(Math::Vec4),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
        });
        pbr_debug_texture_cube_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_DebugTextureCube",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrDebugTextureCubePS,
                .bindings = {ShaderBindingDesc::Texture("g_DebugTexture", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_DebugSampler"),
                             ShaderBindingDesc::UniformBuffer("DebugTexture", sizeof(Math::Vec4),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
        });

        const bool programs_ready = pbr_shadow_depth_program_.IsValid() &&
                                    pbr_skybox_program_.IsValid() &&
                                    pbr_skybox_fallback_program_.IsValid() &&
                                    pbr_debug_texture_2d_program_.IsValid() &&
                                    pbr_debug_texture_array_program_.IsValid() &&
                                    pbr_debug_texture_cube_program_.IsValid();
        if (!programs_ready) {
            DestroyPbrResources();
        }
        return programs_ready;
    }

    void RenderSystem::DestroyPbrResources() {
        if (backend_ == nullptr) {
            return;
        }

        if (pbr_shadow_depth_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_shadow_depth_program_);
        }
        if (pbr_debug_texture_2d_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_debug_texture_2d_program_);
        }
        if (pbr_equirect_to_cube_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_equirect_to_cube_program_);
        }
        if (pbr_irradiance_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_irradiance_program_);
        }
        if (pbr_prefiltered_specular_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_prefiltered_specular_program_);
        }
        if (pbr_brdf_lut_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_brdf_lut_program_);
        }
        if (pbr_skybox_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_skybox_program_);
        }
        if (pbr_skybox_fallback_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_skybox_fallback_program_);
        }
        if (pbr_debug_texture_array_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_debug_texture_array_program_);
        }
        if (pbr_debug_texture_cube_program_.IsValid()) {
            backend_->DestroyShaderProgram(pbr_debug_texture_cube_program_);
        }
        pbr_shadow_depth_program_ = {};
        pbr_equirect_to_cube_program_ = {};
        pbr_irradiance_program_ = {};
        pbr_prefiltered_specular_program_ = {};
        pbr_brdf_lut_program_ = {};
        pbr_skybox_program_ = {};
        pbr_skybox_fallback_program_ = {};
        pbr_debug_texture_2d_program_ = {};
        pbr_debug_texture_array_program_ = {};
        pbr_debug_texture_cube_program_ = {};

        DestroyPbrShadowResources();
        DestroyPbrIblResources();
        pbr_global_resources_ = {};
        backend_->SetPbrGlobalResources(pbr_global_resources_);
    }

    bool RenderSystem::CreatePbrShadowResources() {
        const PbrShadowSettings &settings = desc_.pbr.shadows;
        const std::uint32_t cascade_count =
                settings.directional_shadows ? settings.cascade_count : 1u;
        const std::uint32_t directional_resolution =
                settings.directional_shadows ? settings.directional_shadow_resolution : 128u;
        const std::uint32_t shadowed_point_lights =
                settings.point_shadows ? settings.max_shadowed_point_lights : 0u;
        const std::uint32_t point_light_capacity = std::max<std::uint32_t>(shadowed_point_lights, 1u);
        const std::uint32_t point_resolution = settings.point_shadows ? settings.point_shadow_resolution : 64u;

        pbr_shadow_resources_.directional_texture = backend_->CreateTexture(TextureDesc{
                .debug_name = "PBR_DirectionalShadowArray",
                .width = static_cast<int>(directional_resolution),
                .height = static_cast<int>(directional_resolution),
                .mip_levels = 1u,
                .array_size = cascade_count,
                .dimension = TextureDimension::Texture2DArray,
                .format = TextureFormat::Depth32Float,
                .usage = TextureUsage::ShaderResource | TextureUsage::DepthStencil,
        });
        if (!pbr_shadow_resources_.directional_texture.IsValid()) {
            return false;
        }

        pbr_shadow_resources_.directional_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_shadow_resources_.directional_texture,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::Texture2DArray,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = cascade_count,
        });
        if (!pbr_shadow_resources_.directional_srv.IsValid()) {
            return false;
        }

        for (std::uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
            pbr_shadow_resources_.directional_dsvs[cascade] = backend_->CreateTextureView(TextureViewDesc{
                    .texture = pbr_shadow_resources_.directional_texture,
                    .type = TextureViewType::DepthStencil,
                    .dimension = TextureDimension::Texture2DArray,
                    .mip_level = 0u,
                    .mip_count = 1u,
                    .array_slice = cascade,
                    .array_slice_count = 1u,
            });
            if (!pbr_shadow_resources_.directional_dsvs[cascade].IsValid()) {
                return false;
            }
        }

        const std::uint32_t point_slice_count =
                point_light_capacity * static_cast<std::uint32_t>(kPbrPointShadowFaceCount);
        pbr_shadow_resources_.point_texture = backend_->CreateTexture(TextureDesc{
                .debug_name = "PBR_PointShadowFaceArray",
                .width = static_cast<int>(point_resolution),
                .height = static_cast<int>(point_resolution),
                .mip_levels = 1u,
                .array_size = point_slice_count,
                .dimension = TextureDimension::Texture2DArray,
                .format = TextureFormat::Depth32Float,
                .usage = TextureUsage::ShaderResource | TextureUsage::DepthStencil,
        });
        if (!pbr_shadow_resources_.point_texture.IsValid()) {
            return false;
        }

        pbr_shadow_resources_.point_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_shadow_resources_.point_texture,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::Texture2DArray,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = point_slice_count,
        });
        if (!pbr_shadow_resources_.point_srv.IsValid()) {
            return false;
        }

        for (std::uint32_t slice = 0; slice < point_slice_count; ++slice) {
            pbr_shadow_resources_.point_dsvs[slice] = backend_->CreateTextureView(TextureViewDesc{
                    .texture = pbr_shadow_resources_.point_texture,
                    .type = TextureViewType::DepthStencil,
                    .dimension = TextureDimension::Texture2DArray,
                    .mip_level = 0u,
                    .mip_count = 1u,
                    .array_slice = slice,
                    .array_slice_count = 1u,
            });
            if (!pbr_shadow_resources_.point_dsvs[slice].IsValid()) {
                return false;
            }
        }

        pbr_shadow_resources_.cascade_count = cascade_count;
        pbr_shadow_resources_.max_point_lights = shadowed_point_lights;
        pbr_shadow_resources_.directional_resolution = directional_resolution;
        pbr_shadow_resources_.point_resolution = point_resolution;
        pbr_global_resources_.directional_shadow_map = pbr_shadow_resources_.directional_srv;
        pbr_global_resources_.point_shadow_map = pbr_shadow_resources_.point_srv;
        return true;
    }

    void RenderSystem::DestroyPbrShadowResources() {
        for (TextureViewHandle view: pbr_shadow_resources_.directional_dsvs) {
            if (view.IsValid()) {
                backend_->DestroyTextureView(view);
            }
        }
        for (TextureViewHandle view: pbr_shadow_resources_.point_dsvs) {
            if (view.IsValid()) {
                backend_->DestroyTextureView(view);
            }
        }
        if (pbr_shadow_resources_.directional_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_shadow_resources_.directional_srv);
        }
        if (pbr_shadow_resources_.point_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_shadow_resources_.point_srv);
        }
        if (pbr_shadow_resources_.directional_texture.IsValid()) {
            backend_->DestroyTexture(pbr_shadow_resources_.directional_texture);
        }
        if (pbr_shadow_resources_.point_texture.IsValid()) {
            backend_->DestroyTexture(pbr_shadow_resources_.point_texture);
        }
        pbr_shadow_resources_ = {};
    }

    bool RenderSystem::CreatePbrIblResources() {
        const PbrIblSettings &settings = desc_.pbr.ibl;
        pbr_ibl_resources_.environment_resolution = settings.enabled ? settings.environment_cube_resolution : 16u;
        pbr_ibl_resources_.irradiance_resolution = settings.enabled ? settings.irradiance_resolution : 8u;
        pbr_ibl_resources_.prefiltered_resolution =
                settings.enabled ? settings.prefiltered_specular_resolution : 16u;
        pbr_ibl_resources_.prefiltered_mip_count =
                settings.enabled ? settings.prefiltered_specular_mip_count : 1u;
        pbr_ibl_resources_.brdf_lut_resolution = settings.enabled ? settings.brdf_lut_resolution : 16u;
        return true;
    }

    bool RenderSystem::EnsurePbrIblGenerationResources() {
        if (pbr_ibl_resources_.environment_cube_texture.IsValid() &&
            pbr_ibl_resources_.irradiance_texture.IsValid() &&
            pbr_ibl_resources_.prefiltered_specular_texture.IsValid() &&
            pbr_ibl_resources_.brdf_lut_texture.IsValid() &&
            pbr_ibl_resources_.environment_cube_srv.IsValid() &&
            pbr_ibl_resources_.irradiance_srv.IsValid() &&
            pbr_ibl_resources_.prefiltered_specular_srv.IsValid() &&
            pbr_ibl_resources_.brdf_lut_srv.IsValid() &&
            pbr_ibl_resources_.brdf_lut_rtv.IsValid()) {
            UseRuntimePbrIblResources();
            return true;
        }

        DestroyPbrRuntimeIblResources();
        const auto fail = [this]() {
            DestroyPbrRuntimeIblResources();
            return false;
        };

        const std::uint32_t environment_resolution = pbr_ibl_resources_.environment_resolution;
        const std::uint32_t irradiance_resolution = pbr_ibl_resources_.irradiance_resolution;
        const std::uint32_t prefiltered_resolution = pbr_ibl_resources_.prefiltered_resolution;
        const std::uint32_t prefiltered_mips = pbr_ibl_resources_.prefiltered_mip_count;
        const std::uint32_t brdf_resolution = pbr_ibl_resources_.brdf_lut_resolution;

        pbr_ibl_resources_.environment_cube_texture = backend_->CreateTexture(TextureDesc{
                .debug_name = "PBR_EnvironmentCube",
                .width = static_cast<int>(environment_resolution),
                .height = static_cast<int>(environment_resolution),
                .mip_levels = 1u,
                .array_size = 6u,
                .dimension = TextureDimension::TextureCube,
                .format = TextureFormat::RGBA16Float,
                .usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget,
        });
        pbr_ibl_resources_.irradiance_texture = backend_->CreateTexture(TextureDesc{
                .debug_name = "PBR_IrradianceCube",
                .width = static_cast<int>(irradiance_resolution),
                .height = static_cast<int>(irradiance_resolution),
                .mip_levels = 1u,
                .array_size = 6u,
                .dimension = TextureDimension::TextureCube,
                .format = TextureFormat::RGBA16Float,
                .usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget,
        });
        pbr_ibl_resources_.prefiltered_specular_texture = backend_->CreateTexture(TextureDesc{
                .debug_name = "PBR_PrefilteredSpecularCube",
                .width = static_cast<int>(prefiltered_resolution),
                .height = static_cast<int>(prefiltered_resolution),
                .mip_levels = prefiltered_mips,
                .array_size = 6u,
                .dimension = TextureDimension::TextureCube,
                .format = TextureFormat::RGBA16Float,
                .usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget,
        });
        pbr_ibl_resources_.brdf_lut_texture = backend_->CreateTexture(TextureDesc{
                .debug_name = "PBR_BrdfLut",
                .width = static_cast<int>(brdf_resolution),
                .height = static_cast<int>(brdf_resolution),
                .mip_levels = 1u,
                .array_size = 1u,
                .dimension = TextureDimension::Texture2D,
                .format = TextureFormat::RG16Float,
                .usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget,
        });

        if (!pbr_ibl_resources_.environment_cube_texture.IsValid() ||
            !pbr_ibl_resources_.irradiance_texture.IsValid() ||
            !pbr_ibl_resources_.prefiltered_specular_texture.IsValid() ||
            !pbr_ibl_resources_.brdf_lut_texture.IsValid()) {
            return fail();
        }

        pbr_ibl_resources_.environment_cube_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_ibl_resources_.environment_cube_texture,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::TextureCube,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 6u,
        });
        pbr_ibl_resources_.irradiance_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_ibl_resources_.irradiance_texture,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::TextureCube,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 6u,
        });
        pbr_ibl_resources_.prefiltered_specular_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_ibl_resources_.prefiltered_specular_texture,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::TextureCube,
                .mip_level = 0u,
                .mip_count = prefiltered_mips,
                .array_slice = 0u,
                .array_slice_count = 6u,
        });
        pbr_ibl_resources_.brdf_lut_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_ibl_resources_.brdf_lut_texture,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::Texture2D,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 1u,
        });
        pbr_ibl_resources_.brdf_lut_rtv = backend_->CreateTextureView(TextureViewDesc{
                .texture = pbr_ibl_resources_.brdf_lut_texture,
                .type = TextureViewType::RenderTarget,
                .dimension = TextureDimension::Texture2D,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 1u,
        });

        if (!pbr_ibl_resources_.environment_cube_srv.IsValid() ||
            !pbr_ibl_resources_.irradiance_srv.IsValid() ||
            !pbr_ibl_resources_.prefiltered_specular_srv.IsValid() ||
            !pbr_ibl_resources_.brdf_lut_srv.IsValid() || !pbr_ibl_resources_.brdf_lut_rtv.IsValid()) {
            return fail();
        }

        for (std::uint32_t face = 0u; face < 6u; ++face) {
            pbr_ibl_resources_.environment_cube_rtvs[face] = backend_->CreateTextureView(TextureViewDesc{
                    .texture = pbr_ibl_resources_.environment_cube_texture,
                    .type = TextureViewType::RenderTarget,
                    .dimension = TextureDimension::Texture2DArray,
                    .mip_level = 0u,
                    .mip_count = 1u,
                    .array_slice = face,
                    .array_slice_count = 1u,
            });
            pbr_ibl_resources_.irradiance_rtvs[face] = backend_->CreateTextureView(TextureViewDesc{
                    .texture = pbr_ibl_resources_.irradiance_texture,
                    .type = TextureViewType::RenderTarget,
                    .dimension = TextureDimension::Texture2DArray,
                    .mip_level = 0u,
                    .mip_count = 1u,
                    .array_slice = face,
                    .array_slice_count = 1u,
            });
            if (!pbr_ibl_resources_.environment_cube_rtvs[face].IsValid() ||
                !pbr_ibl_resources_.irradiance_rtvs[face].IsValid()) {
                return fail();
            }
        }

        pbr_ibl_resources_.prefiltered_specular_rtvs.reserve(prefiltered_mips * 6u);
        for (std::uint32_t mip = 0u; mip < prefiltered_mips; ++mip) {
            for (std::uint32_t face = 0u; face < 6u; ++face) {
                TextureViewHandle view = backend_->CreateTextureView(TextureViewDesc{
                        .texture = pbr_ibl_resources_.prefiltered_specular_texture,
                        .type = TextureViewType::RenderTarget,
                        .dimension = TextureDimension::Texture2DArray,
                        .mip_level = mip,
                        .mip_count = 1u,
                        .array_slice = face,
                        .array_slice_count = 1u,
                });
                if (!view.IsValid()) {
                    return fail();
                }
                pbr_ibl_resources_.prefiltered_specular_rtvs.push_back(view);
            }
        }

        pbr_ibl_resources_.environment_resolution = environment_resolution;
        pbr_ibl_resources_.irradiance_resolution = irradiance_resolution;
        pbr_ibl_resources_.prefiltered_resolution = prefiltered_resolution;
        pbr_ibl_resources_.prefiltered_mip_count = prefiltered_mips;
        pbr_ibl_resources_.brdf_lut_resolution = brdf_resolution;
        UseRuntimePbrIblResources();
        return true;
    }

    bool RenderSystem::EnsurePbrIblGenerationPrograms() {
        if (pbr_equirect_to_cube_program_.IsValid() && pbr_irradiance_program_.IsValid() &&
            pbr_prefiltered_specular_program_.IsValid() && pbr_brdf_lut_program_.IsValid()) {
            return true;
        }

        pbr_equirect_to_cube_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_EquirectToCube",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrEquirectToCubePS,
                .bindings = {ShaderBindingDesc::Texture("g_EquirectangularTexture", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_IblSampler"),
                             ShaderBindingDesc::UniformBuffer("IblGenerate", sizeof(Math::Vec4),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
                .color_format = FrameBufferFormat::RGBA16Float,
        });
        pbr_irradiance_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_IrradianceCube",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrIrradianceCubePS,
                .bindings = {ShaderBindingDesc::Texture("g_EnvironmentCube", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_IblSampler"),
                             ShaderBindingDesc::UniformBuffer("IblGenerate", sizeof(Math::Vec4),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
                .color_format = FrameBufferFormat::RGBA16Float,
        });
        pbr_prefiltered_specular_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_PrefilteredSpecular",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrPrefilteredSpecularCubePS,
                .bindings = {ShaderBindingDesc::Texture("g_EnvironmentCube", ShaderBindingScope::Pass,
                                                        ShaderStage::Pixel, "g_IblSampler"),
                             ShaderBindingDesc::UniformBuffer("IblGenerate", sizeof(Math::Vec4),
                                                              ShaderBindingScope::Pass, ShaderStage::Pixel)},
                .color_format = FrameBufferFormat::RGBA16Float,
        });
        pbr_brdf_lut_program_ = backend_->CreateShaderProgram(ShaderProgramDesc{
                .debug_name = "PBR_BrdfLut",
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kPbrBrdfLutPS,
                .bindings = {},
                .color_format = FrameBufferFormat::RGBA16Float,
        });

        return pbr_equirect_to_cube_program_.IsValid() && pbr_irradiance_program_.IsValid() &&
               pbr_prefiltered_specular_program_.IsValid() && pbr_brdf_lut_program_.IsValid();
    }

    void RenderSystem::SetActivePbrIblResources(TextureViewHandle environment_cube, TextureViewHandle irradiance,
                                                TextureViewHandle prefiltered_specular, TextureViewHandle brdf_lut) {
        pbr_ibl_resources_.active_environment_cube_srv = environment_cube;
        pbr_ibl_resources_.active_irradiance_srv = irradiance;
        pbr_ibl_resources_.active_prefiltered_specular_srv = prefiltered_specular;
        pbr_ibl_resources_.active_brdf_lut_srv = brdf_lut;
        pbr_global_resources_.irradiance_map = irradiance;
        pbr_global_resources_.prefiltered_specular_map = prefiltered_specular;
        pbr_global_resources_.brdf_lut = brdf_lut;
    }

    void RenderSystem::UseRuntimePbrIblResources() {
        pbr_ibl_resources_.using_precomputed_cache = false;
        SetActivePbrIblResources(pbr_ibl_resources_.environment_cube_srv, pbr_ibl_resources_.irradiance_srv,
                                 pbr_ibl_resources_.prefiltered_specular_srv, pbr_ibl_resources_.brdf_lut_srv);
    }

    void RenderSystem::DestroyPbrPrecomputedIblResources() {
        if (pbr_ibl_resources_.precomputed_environment_cube_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.precomputed_environment_cube_srv);
        }
        if (pbr_ibl_resources_.precomputed_irradiance_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.precomputed_irradiance_srv);
        }
        if (pbr_ibl_resources_.precomputed_prefiltered_specular_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.precomputed_prefiltered_specular_srv);
        }
        if (pbr_ibl_resources_.precomputed_brdf_lut_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.precomputed_brdf_lut_srv);
        }
        if (pbr_ibl_resources_.precomputed_environment_cube_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.precomputed_environment_cube_texture);
        }
        if (pbr_ibl_resources_.precomputed_irradiance_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.precomputed_irradiance_texture);
        }
        if (pbr_ibl_resources_.precomputed_prefiltered_specular_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.precomputed_prefiltered_specular_texture);
        }
        if (pbr_ibl_resources_.precomputed_brdf_lut_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.precomputed_brdf_lut_texture);
        }

        pbr_ibl_resources_.precomputed_environment_cube_texture = {};
        pbr_ibl_resources_.precomputed_environment_cube_srv = {};
        pbr_ibl_resources_.precomputed_irradiance_texture = {};
        pbr_ibl_resources_.precomputed_irradiance_srv = {};
        pbr_ibl_resources_.precomputed_prefiltered_specular_texture = {};
        pbr_ibl_resources_.precomputed_prefiltered_specular_srv = {};
        pbr_ibl_resources_.precomputed_brdf_lut_texture = {};
        pbr_ibl_resources_.precomputed_brdf_lut_srv = {};
        UseRuntimePbrIblResources();
    }

    bool RenderSystem::LoadPbrPrecomputedIbl(const PbrPrecomputedIblPaths &paths) {
        if (backend_ == nullptr || !paths.IsComplete()) {
            return false;
        }

        DestroyPbrPrecomputedIblResources();

        TextureHandle environment_cube = backend_->LoadTexture2D(TextureLoadDesc{
                .path = paths.environment_cube_path,
                .format = TextureFormat::Auto,
                .generate_mipmaps = false,
                .flip_vertically = false,
                .premultiply_alpha = false,
        });
        TextureHandle irradiance = backend_->LoadTexture2D(TextureLoadDesc{
                .path = paths.irradiance_cube_path,
                .format = TextureFormat::Auto,
                .generate_mipmaps = false,
                .flip_vertically = false,
                .premultiply_alpha = false,
        });
        TextureHandle prefiltered_specular = backend_->LoadTexture2D(TextureLoadDesc{
                .path = paths.prefiltered_specular_cube_path,
                .format = TextureFormat::Auto,
                .generate_mipmaps = false,
                .flip_vertically = false,
                .premultiply_alpha = false,
        });
        TextureHandle brdf_lut = backend_->LoadTexture2D(TextureLoadDesc{
                .path = paths.brdf_lut_path,
                .format = TextureFormat::Auto,
                .generate_mipmaps = false,
                .flip_vertically = false,
                .premultiply_alpha = false,
        });

        if (!environment_cube.IsValid() || !irradiance.IsValid() || !prefiltered_specular.IsValid() ||
            !brdf_lut.IsValid()) {
            if (environment_cube.IsValid()) {
                backend_->DestroyTexture(environment_cube);
            }
            if (irradiance.IsValid()) {
                backend_->DestroyTexture(irradiance);
            }
            if (prefiltered_specular.IsValid()) {
                backend_->DestroyTexture(prefiltered_specular);
            }
            if (brdf_lut.IsValid()) {
                backend_->DestroyTexture(brdf_lut);
            }
            UseRuntimePbrIblResources();
            return false;
        }

        TextureViewHandle environment_cube_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = environment_cube,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::TextureCube,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 6u,
        });
        TextureViewHandle irradiance_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = irradiance,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::TextureCube,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 6u,
        });
        TextureViewHandle prefiltered_specular_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = prefiltered_specular,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::TextureCube,
                .mip_level = 0u,
                .mip_count = pbr_ibl_resources_.prefiltered_mip_count,
                .array_slice = 0u,
                .array_slice_count = 6u,
        });
        TextureViewHandle brdf_lut_srv = backend_->CreateTextureView(TextureViewDesc{
                .texture = brdf_lut,
                .type = TextureViewType::ShaderResource,
                .dimension = TextureDimension::Texture2D,
                .mip_level = 0u,
                .mip_count = 1u,
                .array_slice = 0u,
                .array_slice_count = 1u,
        });

        if (!environment_cube_srv.IsValid() || !irradiance_srv.IsValid() || !prefiltered_specular_srv.IsValid() ||
            !brdf_lut_srv.IsValid()) {
            if (environment_cube_srv.IsValid()) {
                backend_->DestroyTextureView(environment_cube_srv);
            }
            if (irradiance_srv.IsValid()) {
                backend_->DestroyTextureView(irradiance_srv);
            }
            if (prefiltered_specular_srv.IsValid()) {
                backend_->DestroyTextureView(prefiltered_specular_srv);
            }
            if (brdf_lut_srv.IsValid()) {
                backend_->DestroyTextureView(brdf_lut_srv);
            }
            backend_->DestroyTexture(environment_cube);
            backend_->DestroyTexture(irradiance);
            backend_->DestroyTexture(prefiltered_specular);
            backend_->DestroyTexture(brdf_lut);
            UseRuntimePbrIblResources();
            return false;
        }

        pbr_ibl_resources_.precomputed_environment_cube_texture = environment_cube;
        pbr_ibl_resources_.precomputed_environment_cube_srv = environment_cube_srv;
        pbr_ibl_resources_.precomputed_irradiance_texture = irradiance;
        pbr_ibl_resources_.precomputed_irradiance_srv = irradiance_srv;
        pbr_ibl_resources_.precomputed_prefiltered_specular_texture = prefiltered_specular;
        pbr_ibl_resources_.precomputed_prefiltered_specular_srv = prefiltered_specular_srv;
        pbr_ibl_resources_.precomputed_brdf_lut_texture = brdf_lut;
        pbr_ibl_resources_.precomputed_brdf_lut_srv = brdf_lut_srv;
        pbr_ibl_resources_.using_precomputed_cache = true;
        SetActivePbrIblResources(environment_cube_srv, irradiance_srv, prefiltered_specular_srv, brdf_lut_srv);
        Log::Info("Render", "Loaded precomputed PBR IBL cache '{}'", ResolvePrecomputedIblManifestPath(paths));
        return true;
    }

    bool RenderSystem::SaveGeneratedPbrIblCache() {
        if (backend_ == nullptr || !pbr_ibl_resources_.save_generated_precomputed_cache ||
            !pbr_ibl_resources_.pending_precomputed_bake_paths.IsComplete() ||
            pbr_ibl_resources_.pending_precomputed_bake_source_key.empty()) {
            return false;
        }

        const PbrPrecomputedIblPaths &paths = pbr_ibl_resources_.pending_precomputed_bake_paths;
        const bool saved = backend_->SaveTextureAsDds(pbr_ibl_resources_.environment_cube_texture,
                                                      paths.environment_cube_path) &&
                           backend_->SaveTextureAsDds(pbr_ibl_resources_.irradiance_texture,
                                                      paths.irradiance_cube_path) &&
                           backend_->SaveTextureAsDds(pbr_ibl_resources_.prefiltered_specular_texture,
                                                      paths.prefiltered_specular_cube_path) &&
                           backend_->SaveTextureAsDds(pbr_ibl_resources_.brdf_lut_texture, paths.brdf_lut_path);
        if (!saved) {
            Log::Warn("Render", "Failed to persist generated PBR IBL cache '{}': {}",
                      ResolvePrecomputedIblManifestPath(paths), backend_->LastError());
            return false;
        }

        if (!WritePbrIblManifest(paths, pbr_ibl_resources_.pending_precomputed_bake_source_key,
                                 pbr_ibl_resources_.environment_resolution,
                                 pbr_ibl_resources_.irradiance_resolution,
                                 pbr_ibl_resources_.prefiltered_resolution,
                                 pbr_ibl_resources_.prefiltered_mip_count,
                                 pbr_ibl_resources_.brdf_lut_resolution)) {
            Log::Warn("Render", "Failed to write generated PBR IBL cache manifest '{}'",
                      ResolvePrecomputedIblManifestPath(paths));
            return false;
        }

        Log::Info("Render", "Persisted generated PBR IBL cache '{}'", ResolvePrecomputedIblManifestPath(paths));
        return true;
    }

    void RenderSystem::DestroyPbrRuntimeIblResources() {
        for (TextureViewHandle view: pbr_ibl_resources_.environment_cube_rtvs) {
            if (view.IsValid()) {
                backend_->DestroyTextureView(view);
            }
        }
        for (TextureViewHandle view: pbr_ibl_resources_.irradiance_rtvs) {
            if (view.IsValid()) {
                backend_->DestroyTextureView(view);
            }
        }
        for (TextureViewHandle view: pbr_ibl_resources_.prefiltered_specular_rtvs) {
            if (view.IsValid()) {
                backend_->DestroyTextureView(view);
            }
        }
        if (pbr_ibl_resources_.environment_cube_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.environment_cube_srv);
        }
        if (pbr_ibl_resources_.irradiance_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.irradiance_srv);
        }
        if (pbr_ibl_resources_.prefiltered_specular_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.prefiltered_specular_srv);
        }
        if (pbr_ibl_resources_.brdf_lut_srv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.brdf_lut_srv);
        }
        if (pbr_ibl_resources_.brdf_lut_rtv.IsValid()) {
            backend_->DestroyTextureView(pbr_ibl_resources_.brdf_lut_rtv);
        }
        if (pbr_ibl_resources_.environment_cube_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.environment_cube_texture);
        }
        if (pbr_ibl_resources_.irradiance_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.irradiance_texture);
        }
        if (pbr_ibl_resources_.prefiltered_specular_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.prefiltered_specular_texture);
        }
        if (pbr_ibl_resources_.brdf_lut_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.brdf_lut_texture);
        }
        pbr_ibl_resources_.environment_cube_texture = {};
        pbr_ibl_resources_.environment_cube_srv = {};
        pbr_ibl_resources_.environment_cube_rtvs = {};
        pbr_ibl_resources_.irradiance_texture = {};
        pbr_ibl_resources_.irradiance_srv = {};
        pbr_ibl_resources_.irradiance_rtvs = {};
        pbr_ibl_resources_.prefiltered_specular_texture = {};
        pbr_ibl_resources_.prefiltered_specular_srv = {};
        pbr_ibl_resources_.prefiltered_specular_rtvs.clear();
        pbr_ibl_resources_.brdf_lut_texture = {};
        pbr_ibl_resources_.brdf_lut_srv = {};
        pbr_ibl_resources_.brdf_lut_rtv = {};
    }

    void RenderSystem::DestroyPbrIblResources() {
        DestroyPbrPrecomputedIblResources();
        DestroyPbrRuntimeIblResources();
        if (pbr_ibl_resources_.owns_source_texture && pbr_ibl_resources_.source_equirectangular_texture.IsValid()) {
            backend_->DestroyTexture(pbr_ibl_resources_.source_equirectangular_texture);
        }
        pbr_ibl_resources_ = {};
    }

    void RenderSystem::GatherShadowCasters(World &world, ShadowCasterFilter filter) {
        auto group = world.Registry().group<TransformComponent, MeshRendererComponent>();
        shadow_accumulator_.Reserve(group.size());
        shadow_accumulator_.Clear();
        world_transform_cache_.clear();
        world_transform_cache_.reserve(group.size());

        for (auto [entity, transform, renderer]: group.each()) {
            if (!renderer.visible || !renderer.cast_shadows || !renderer.mesh.IsValid()) {
                continue;
            }
            if (filter == ShadowCasterFilter::StaticOnly && !IsStaticBakeCandidate(renderer)) {
                continue;
            }
            if (filter == ShadowCasterFilter::DynamicOnly && renderer.mobility == RenderMobility::Static) {
                continue;
            }

            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            const Math::Mat4 world_matrix = hierarchy == nullptr || hierarchy->parent == entt::null
                                                    ? transform.WorldMatrix()
                                                    : ResolveCachedWorldMatrix(world, entity, world_transform_cache_);
            shadow_accumulator_.Add(renderer.mesh, world_matrix);
        }
    }

    void RenderSystem::ExecutePbrIblPass(RenderPassContext &context) {
        if (backend_ == nullptr) {
            return;
        }

        if (!desc_.pbr.ibl.enabled) {
            pbr_ibl_resources_.generated = false;
            pbr_ibl_resources_.generation_pending = false;
            pbr_ibl_resources_.save_generated_precomputed_cache = false;
            backend_->SetPbrGlobalResources(pbr_global_resources_);
            return;
        }

        World &world = context.GetWorld();
        const CameraData active_camera =
                has_manual_camera_override_ ? manual_camera_override_ : ResolveWorldCamera(world);
        std::unordered_map<entt::entity, Math::Mat4> probe_transform_cache;
        const ActiveReflectionProbe active_probe =
                ResolveActiveReflectionProbe(world, probe_transform_cache, ExtractCameraPosition(active_camera));

        std::string runtime_source_key;
        std::string runtime_source_path;
        TextureHandle external_source{};
        PbrPrecomputedIblPaths precomputed_paths{};
        bool use_precomputed_ibl = false;
        bool bake_precomputed_ibl = false;
        bool allow_precomputed_ibl_bake = false;
        const auto consider_environment_cache = [&](const EnvironmentLightComponent &environment,
                                                    TextureHandle source_override) {
            if (!environment.enabled) {
                return false;
            }
            if (source_override.IsValid() && environment.environment_map.IsValid() &&
                environment.environment_map != source_override) {
                return false;
            }

            PbrPrecomputedIblPaths candidate_precomputed{
                    .environment_cube_path = environment.precomputed_environment_cube_path,
                    .irradiance_cube_path = environment.precomputed_irradiance_cube_path,
                    .prefiltered_specular_cube_path = environment.precomputed_prefiltered_specular_cube_path,
                    .brdf_lut_path = environment.precomputed_brdf_lut_path,
                    .manifest_path = environment.precomputed_ibl_manifest_path,
            };
            std::string candidate_source_key;
            std::string candidate_source_path;
            TextureHandle candidate_external_source = source_override;

            if (!environment.hdr_equirectangular_path.empty()) {
                candidate_source_path = environment.hdr_equirectangular_path;
                candidate_source_key = MakePathIblSourceKey(candidate_source_path);
                if (!candidate_external_source.IsValid() && environment.environment_map.IsValid()) {
                    candidate_external_source = environment.environment_map;
                }
            } else if (candidate_external_source.IsValid()) {
                candidate_source_key = MakeHandleIblSourceKey("handle", candidate_external_source);
            } else if (environment.environment_map.IsValid()) {
                candidate_source_key = MakeHandleIblSourceKey("handle", environment.environment_map);
                candidate_external_source = environment.environment_map;
            }

            bool candidate_use_precomputed = false;
            bool candidate_bake_precomputed = false;
            bool candidate_allow_bake = false;
            if (candidate_precomputed.IsComplete()) {
                candidate_allow_bake = environment.bake_precomputed_ibl_if_missing;
                const bool require_manifest = environment.bake_precomputed_ibl_if_missing &&
                                              !candidate_source_key.empty();
                if (HasCurrentPrecomputedIbl(candidate_precomputed, candidate_source_key, require_manifest,
                                             pbr_ibl_resources_.environment_resolution,
                                             pbr_ibl_resources_.irradiance_resolution,
                                             pbr_ibl_resources_.prefiltered_resolution,
                                             pbr_ibl_resources_.prefiltered_mip_count,
                                             pbr_ibl_resources_.brdf_lut_resolution)) {
                    candidate_use_precomputed = true;
                } else if (environment.bake_precomputed_ibl_if_missing && !candidate_source_key.empty()) {
                    candidate_bake_precomputed = true;
                }
            }

            if (!candidate_use_precomputed && candidate_source_key.empty()) {
                return false;
            }

            runtime_source_key = std::move(candidate_source_key);
            runtime_source_path = std::move(candidate_source_path);
            external_source = candidate_external_source;
            precomputed_paths = std::move(candidate_precomputed);
            use_precomputed_ibl = candidate_use_precomputed;
            bake_precomputed_ibl = candidate_bake_precomputed;
            allow_precomputed_ibl_bake = candidate_allow_bake;
            return true;
        };

        if (active_probe.enabled) {
            if (active_probe.environment_map.IsValid()) {
                runtime_source_key = MakeHandleIblSourceKey("probe", active_probe.environment_map);
                external_source = active_probe.environment_map;
            }

            auto environment_view = world.View<EnvironmentLightComponent>();
            for (auto entity: environment_view) {
                const EnvironmentLightComponent &environment = environment_view.get<EnvironmentLightComponent>(entity);
                if (consider_environment_cache(environment, active_probe.environment_map)) {
                    break;
                }
            }
        } else {
            auto environment_view = world.View<EnvironmentLightComponent>();
            for (auto entity: environment_view) {
                const EnvironmentLightComponent &environment = environment_view.get<EnvironmentLightComponent>(entity);
                if (consider_environment_cache(environment, {})) {
                    break;
                }
            }
        }

        std::string source_key = use_precomputed_ibl ? MakePrecomputedIblKey(precomputed_paths) : runtime_source_key;
        if (source_key != pbr_ibl_resources_.source_key) {
            if (pbr_ibl_resources_.owns_source_texture && pbr_ibl_resources_.source_equirectangular_texture.IsValid()) {
                backend_->DestroyTexture(pbr_ibl_resources_.source_equirectangular_texture);
            }
            DestroyPbrPrecomputedIblResources();
            pbr_ibl_resources_.source_equirectangular_texture = {};
            pbr_ibl_resources_.source_key = source_key;
            pbr_ibl_resources_.source_path = runtime_source_path;
            pbr_ibl_resources_.pending_precomputed_bake_paths = {};
            pbr_ibl_resources_.pending_precomputed_bake_source_key = {};
            pbr_ibl_resources_.save_generated_precomputed_cache = false;
            pbr_ibl_resources_.owns_source_texture = false;
            pbr_ibl_resources_.generated = false;
            pbr_ibl_resources_.generation_pending = !source_key.empty();

            if (use_precomputed_ibl && LoadPbrPrecomputedIbl(precomputed_paths)) {
                pbr_ibl_resources_.generated = true;
                pbr_ibl_resources_.generation_pending = false;
            } else if (!runtime_source_key.empty()) {
                pbr_ibl_resources_.source_key = runtime_source_key;
                pbr_ibl_resources_.source_path = runtime_source_path;
                pbr_ibl_resources_.generation_pending = true;
                if ((bake_precomputed_ibl || (use_precomputed_ibl && allow_precomputed_ibl_bake)) &&
                    precomputed_paths.IsComplete()) {
                    pbr_ibl_resources_.pending_precomputed_bake_paths = std::move(precomputed_paths);
                    pbr_ibl_resources_.pending_precomputed_bake_source_key = runtime_source_key;
                    pbr_ibl_resources_.save_generated_precomputed_cache = true;
                }
                if (external_source.IsValid()) {
                    pbr_ibl_resources_.source_equirectangular_texture = external_source;
                } else if (!runtime_source_path.empty()) {
                    pbr_ibl_resources_.source_equirectangular_texture = backend_->LoadTexture2DAsync(TextureLoadDesc{
                            .path = runtime_source_path,
                            .format = TextureFormat::Auto,
                            .generate_mipmaps = false,
                            .flip_vertically = false,
                            .premultiply_alpha = false,
                    });
                    pbr_ibl_resources_.owns_source_texture =
                            pbr_ibl_resources_.source_equirectangular_texture.IsValid();
                } else {
                    pbr_ibl_resources_.generation_pending = false;
                }
            } else if (use_precomputed_ibl) {
                pbr_ibl_resources_.source_key = {};
                pbr_ibl_resources_.generation_pending = false;
            }
        }

        if (pbr_ibl_resources_.generation_pending &&
            pbr_ibl_resources_.source_equirectangular_texture.IsValid() &&
            backend_->GetTextureLoadState(pbr_ibl_resources_.source_equirectangular_texture) ==
                    TextureLoadState::Ready) {
            if (!EnsurePbrIblGenerationResources() || !EnsurePbrIblGenerationPrograms()) {
                pbr_ibl_resources_.generation_pending = false;
                pbr_ibl_resources_.save_generated_precomputed_cache = false;
                Log::Warn("Render", "Failed to create PBR IBL generation resources; environment lighting disabled.");
                backend_->SetPbrGlobalResources(pbr_global_resources_);
                return;
            }

            for (std::uint32_t face = 0u; face < 6u; ++face) {
                const Math::Vec4 params{static_cast<float>(face), 0.f, 0.f,
                                        static_cast<float>(pbr_ibl_resources_.environment_resolution)};
                context.SetRenderTargets(pbr_ibl_resources_.environment_cube_rtvs[face], {});
                context.Clear(RenderClearColor{});
                context.UseShaderProgram(pbr_equirect_to_cube_program_);
                context.BindTexture("g_EquirectangularTexture", pbr_ibl_resources_.source_equirectangular_texture);
                context.BindUniform("IblGenerate", params);
                context.DrawFullscreenTriangle();
            }

            for (std::uint32_t face = 0u; face < 6u; ++face) {
                const Math::Vec4 params{static_cast<float>(face), 0.f, 0.f,
                                        static_cast<float>(pbr_ibl_resources_.irradiance_resolution)};
                context.SetRenderTargets(pbr_ibl_resources_.irradiance_rtvs[face], {});
                context.Clear(RenderClearColor{});
                context.UseShaderProgram(pbr_irradiance_program_);
                context.BindTexture("g_EnvironmentCube", pbr_ibl_resources_.environment_cube_srv);
                context.BindUniform("IblGenerate", params);
                context.DrawFullscreenTriangle();
            }

            for (std::uint32_t mip = 0u; mip < pbr_ibl_resources_.prefiltered_mip_count; ++mip) {
                const float roughness = pbr_ibl_resources_.prefiltered_mip_count > 1u
                                                ? static_cast<float>(mip) /
                                                          static_cast<float>(pbr_ibl_resources_.prefiltered_mip_count -
                                                                             1u)
                                                : 0.f;
                for (std::uint32_t face = 0u; face < 6u; ++face) {
                    const std::uint32_t view_index = mip * 6u + face;
                    const Math::Vec4 params{static_cast<float>(face), static_cast<float>(mip), roughness,
                                            static_cast<float>(pbr_ibl_resources_.prefiltered_resolution)};
                    context.SetRenderTargets(pbr_ibl_resources_.prefiltered_specular_rtvs[view_index], {});
                    context.Clear(RenderClearColor{});
                    context.UseShaderProgram(pbr_prefiltered_specular_program_);
                    context.BindTexture("g_EnvironmentCube", pbr_ibl_resources_.environment_cube_srv);
                    context.BindUniform("IblGenerate", params);
                    context.DrawFullscreenTriangle();
                }
            }

            context.SetRenderTargets(pbr_ibl_resources_.brdf_lut_rtv, {});
            context.Clear(RenderClearColor{});
            context.UseShaderProgram(pbr_brdf_lut_program_);
            context.BindUniform("IblGenerate", Math::Vec4(static_cast<float>(pbr_ibl_resources_.brdf_lut_resolution),
                                                          0.f, 0.f, 0.f));
            context.DrawFullscreenTriangle();

            pbr_ibl_resources_.generation_pending = false;
            pbr_ibl_resources_.generated = true;
            debug_registry_.Stats().ibl_generated_this_frame = true;
            if (pbr_ibl_resources_.save_generated_precomputed_cache) {
                (void) SaveGeneratedPbrIblCache();
                pbr_ibl_resources_.save_generated_precomputed_cache = false;
                pbr_ibl_resources_.pending_precomputed_bake_paths = {};
                pbr_ibl_resources_.pending_precomputed_bake_source_key = {};
            }
        }

        backend_->SetPbrGlobalResources(pbr_global_resources_);
        RenderDebugStats &stats = debug_registry_.Stats();
        stats.estimated_ibl_bytes =
                TextureBytes(pbr_ibl_resources_.environment_resolution, pbr_ibl_resources_.environment_resolution, 6u,
                             8u) +
                TextureBytes(pbr_ibl_resources_.irradiance_resolution, pbr_ibl_resources_.irradiance_resolution, 6u,
                             8u) +
                TextureBytes(pbr_ibl_resources_.prefiltered_resolution, pbr_ibl_resources_.prefiltered_resolution,
                             6u * pbr_ibl_resources_.prefiltered_mip_count, 8u) +
                TextureBytes(pbr_ibl_resources_.brdf_lut_resolution, pbr_ibl_resources_.brdf_lut_resolution, 1u, 4u);

        if (desc_.pbr.visual_debug) {
            for (std::uint32_t face = 0; face < 6u; ++face) {
                debug_registry_.RegisterView(RenderDebugView{
                        .name = "ibl/environment/face-" + std::to_string(face),
                        .kind = RenderDebugViewKind::TextureCubeFace,
                        .texture_view = pbr_ibl_resources_.active_environment_cube_srv,
                        .cube_face = face,
                });
                debug_registry_.RegisterView(RenderDebugView{
                        .name = "ibl/irradiance/face-" + std::to_string(face),
                        .kind = RenderDebugViewKind::TextureCubeFace,
                        .texture_view = pbr_ibl_resources_.active_irradiance_srv,
                        .cube_face = face,
                });
                debug_registry_.RegisterView(RenderDebugView{
                        .name = "ibl/prefiltered-specular/face-" + std::to_string(face),
                        .kind = RenderDebugViewKind::TextureCubeFace,
                        .texture_view = pbr_ibl_resources_.active_prefiltered_specular_srv,
                        .cube_face = face,
                });
            }

            for (std::uint32_t mip = 0; mip < pbr_ibl_resources_.prefiltered_mip_count; ++mip) {
                debug_registry_.RegisterView(RenderDebugView{
                        .name = "ibl/prefiltered-specular/mip-" + std::to_string(mip),
                        .kind = RenderDebugViewKind::TextureCubeFace,
                        .texture_view = pbr_ibl_resources_.active_prefiltered_specular_srv,
                        .cube_face = 0u,
                        .mip_level = mip,
                });
            }

            debug_registry_.RegisterView(RenderDebugView{
                    .name = "ibl/brdf-lut",
                    .kind = RenderDebugViewKind::Texture2D,
                    .texture_view = pbr_ibl_resources_.active_brdf_lut_srv,
            });
            debug_registry_.RegisterView(RenderDebugView{
                    .name = "probes/reflection-influence",
                    .kind = RenderDebugViewKind::Overlay,
            });
            debug_registry_.RegisterView(RenderDebugView{
                    .name = "resources/selected-active",
                    .kind = RenderDebugViewKind::TextStats,
            });
        }
    }

    void RenderSystem::ExecutePbrShadowPass(RenderPassContext &context) {
        if (!pbr_shadow_depth_program_.IsValid()) {
            return;
        }

        pbr_shadow_frame_data_ = {};
        pbr_shadow_frame_data_.directional_shadow_params =
                Math::Vec4(0.f, static_cast<float>(pbr_shadow_resources_.directional_resolution),
                           desc_.pbr.shadows.directional_shadow_bias,
                           desc_.pbr.shadows.directional_shadow_normal_bias);
        pbr_shadow_frame_data_.point_shadow_params =
                Math::Vec4(0.f, static_cast<float>(pbr_shadow_resources_.point_resolution),
                           static_cast<float>(desc_.pbr.shadows.point_shadow_pcf_radius), 0.f);

        World &world = context.GetWorld();
        GatherShadowCasters(world);

        const CameraData active_camera =
                has_manual_camera_override_ ? manual_camera_override_ : ResolveWorldCamera(world);
        const Math::Vec3 camera_position = ExtractCameraPosition(active_camera);
        const Math::Vec3 camera_right = ExtractCameraRight(active_camera);
        const Math::Vec3 camera_up = ExtractCameraUp(active_camera);
        const Math::Vec3 camera_forward = ExtractCameraForward(active_camera);
        const float camera_near = ExtractProjectionNear(active_camera.projection);
        const float camera_far = ExtractProjectionFar(active_camera.projection, camera_near);
        const float directional_shadow_distance =
                std::max(std::min(desc_.pbr.shadows.directional_shadow_distance, camera_far), camera_near + 1.f);

        auto directional_view = world.View<DirectionalLightComponent>();
        for (auto entity: directional_view) {
            const DirectionalLightComponent &light = directional_view.get<DirectionalLightComponent>(entity);
            if (!desc_.pbr.shadows.directional_shadows || !light.enabled || !light.cast_shadows ||
                light.illuminance_lux <= 0.f) {
                continue;
            }

            Math::Vec3 light_direction = Math::LengthSquared(light.direction) <= 1.0e-8f
                                                 ? Math::Vec3{0.f, -1.f, 0.f}
                                                 : Math::Normalize(light.direction);
            const std::uint32_t cascade_count = pbr_shadow_resources_.cascade_count;
            const float split_lambda = desc_.pbr.shadows.cascades.split_lambda;
            const float split_range = std::max(directional_shadow_distance - camera_near, 1.f);
            const float split_ratio = std::max(directional_shadow_distance / std::max(camera_near, 0.001f), 1.f);
            float previous_split = camera_near;
            for (std::uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
                const float cascade_ratio = static_cast<float>(cascade + 1u) / static_cast<float>(cascade_count);
                const float linear_split = camera_near + split_range * cascade_ratio;
                const float logarithmic_split = camera_near * std::pow(split_ratio, cascade_ratio);
                float cascade_far = linear_split * (1.f - split_lambda) + logarithmic_split * split_lambda;
                if (cascade + 1u == cascade_count) {
                    cascade_far = directional_shadow_distance;
                }
                cascade_far = std::max(cascade_far, previous_split + 0.01f);

                const DirectionalShadowCascadeData cascade_data = BuildDirectionalShadowCascade(
                        active_camera, camera_position, camera_right, camera_up, camera_forward, light_direction,
                        previous_split, cascade_far, pbr_shadow_resources_.directional_resolution,
                        desc_.pbr.shadows.cascades);
                Math::ValuePtr(pbr_shadow_frame_data_.directional_shadow_splits)[cascade] =
                        cascade_data.split_depth;
                pbr_shadow_frame_data_.directional_shadow_view_proj[cascade] = cascade_data.view_proj;

                CameraData shadow_camera{
                        .view = Math::Identity(),
                        .projection = pbr_shadow_frame_data_.directional_shadow_view_proj[cascade],
                };
                context.SetPerFrameProps(PerFrameProps{.camera = shadow_camera});
                context.SetRenderTargets({}, pbr_shadow_resources_.directional_dsvs[cascade]);
                context.Clear(RenderClearColor{});
                context.UseShaderProgram(pbr_shadow_depth_program_);

                for (const GeometryBatch &batch: shadow_accumulator_.Batches()) {
                    context.SubmitGeometryBatch(batch);
                    debug_registry_.Stats().directional_shadow_draws +=
                            static_cast<std::uint32_t>(batch.InstanceCount());
                }

                if (desc_.pbr.visual_debug) {
                    debug_registry_.RegisterView(RenderDebugView{
                            .name = "shadows/directional/cascade-" + std::to_string(cascade),
                            .kind = RenderDebugViewKind::Texture2DArraySlice,
                            .texture_view = pbr_shadow_resources_.directional_srv,
                            .array_slice = cascade,
                    });
                }
                previous_split = cascade_far;
            }

            pbr_shadow_frame_data_.directional_shadow_params =
                    Math::Vec4(static_cast<float>(cascade_count),
                               static_cast<float>(pbr_shadow_resources_.directional_resolution),
                               std::clamp(PositiveFiniteOrZero(light.shadow_bias), 0.f, 0.1f),
                               std::clamp(PositiveFiniteOrZero(light.shadow_normal_bias), 0.f, 10.f));
            pbr_shadow_frame_data_.directional_shadow_extra =
                    Math::Vec4(std::clamp(PositiveFiniteOrZero(light.shadow_strength), 0.f, 1.f),
                               static_cast<float>(desc_.pbr.shadows.directional_shadow_pcf_radius), 0.f, 0.f);
            debug_registry_.Stats().shadow_cascade_count = cascade_count;
            break;
        }

        std::array<PointLightFrameData, kMaxPbrPointLights> point_lights{};
        const std::uint32_t point_light_count =
                ResolvePointLights(world, world_transform_cache_, point_lights, pbr_shadow_resources_.max_point_lights);
        std::uint32_t shadowed_point_count = 0u;
        for (std::uint32_t light_index = 0u; light_index < point_light_count; ++light_index) {
            const PointLightFrameData &light = point_lights[light_index];
            if (!desc_.pbr.shadows.point_shadows || !light.casts_shadows) {
                continue;
            }

            const Math::Mat4 projection = Math::PerspectiveLH(Math::HalfPi, 1.f, light.shadow_near_z, light.range);
            for (std::uint32_t face = 0u; face < static_cast<std::uint32_t>(kPbrPointShadowFaceCount); ++face) {
                const std::uint32_t slice = light.shadow_index * static_cast<std::uint32_t>(kPbrPointShadowFaceCount) +
                                            face;
                if (slice >= pbr_shadow_resources_.point_dsvs.size() ||
                    !pbr_shadow_resources_.point_dsvs[slice].IsValid()) {
                    continue;
                }

                const Math::Vec3 direction = PointShadowFaceDirection(face);
                const Math::Mat4 view = Math::LookAtLH(light.position, light.position + direction,
                                                       PointShadowFaceUp(face));
                pbr_shadow_frame_data_.point_shadow_view_proj[slice] = projection * view;

                CameraData shadow_camera{.view = view, .projection = projection};
                context.SetPerFrameProps(PerFrameProps{.camera = shadow_camera});
                context.SetRenderTargets({}, pbr_shadow_resources_.point_dsvs[slice]);
                context.Clear(RenderClearColor{});
                context.UseShaderProgram(pbr_shadow_depth_program_);

                for (const GeometryBatch &batch: shadow_accumulator_.Batches()) {
                    context.SubmitGeometryBatch(batch);
                    debug_registry_.Stats().point_shadow_draws += static_cast<std::uint32_t>(batch.InstanceCount());
                }

                if (desc_.pbr.visual_debug) {
                    debug_registry_.RegisterView(RenderDebugView{
                            .name = "shadows/point/light-" + std::to_string(light.shadow_index) + "/face-" +
                                    std::to_string(face),
                            .kind = RenderDebugViewKind::Texture2DArraySlice,
                            .texture_view = pbr_shadow_resources_.point_srv,
                            .array_slice = slice,
                    });
                }
            }

            shadowed_point_count = std::max(shadowed_point_count, light.shadow_index + 1u);
        }
        pbr_shadow_frame_data_.point_shadow_params =
                Math::Vec4(static_cast<float>(shadowed_point_count),
                           static_cast<float>(pbr_shadow_resources_.point_resolution),
                           static_cast<float>(desc_.pbr.shadows.point_shadow_pcf_radius), 0.f);
        debug_registry_.Stats().shadowed_point_light_count = shadowed_point_count;
        debug_registry_.Stats().estimated_shadow_bytes =
                TextureBytes(pbr_shadow_resources_.directional_resolution,
                             pbr_shadow_resources_.directional_resolution, pbr_shadow_resources_.cascade_count, 4u) +
                TextureBytes(pbr_shadow_resources_.point_resolution, pbr_shadow_resources_.point_resolution,
                             std::max<std::uint32_t>(pbr_shadow_resources_.max_point_lights, 1u) *
                                     static_cast<std::uint32_t>(kPbrPointShadowFaceCount),
                             4u);

        backend_->SetPbrGlobalResources(pbr_global_resources_);
        if (desc_.pbr.visual_debug) {
            debug_registry_.RegisterView(RenderDebugView{.name = "lighting/cascade-index",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "lighting/shadow-factor",
                                                         .kind = RenderDebugViewKind::ScalarHeatmap});
        }
    }

    void RenderSystem::ExecutePbrDebugPass(RenderPassContext &context) {
        const RenderDebugView *selected = debug_registry_.SelectedView();
        if (selected == nullptr) {
            return;
        }

        if (selected->depth_view.IsValid()) {
            context.RenderDepthToColor(selected->depth_view, scene_framebuffer_);
            return;
        }

        ShaderProgramHandle program{};
        if (selected->kind == RenderDebugViewKind::Texture2D) {
            program = pbr_debug_texture_2d_program_;
        } else if (selected->kind == RenderDebugViewKind::Texture2DArraySlice ||
                   selected->kind == RenderDebugViewKind::ScalarHeatmap) {
            program = pbr_debug_texture_array_program_;
        } else if (selected->kind == RenderDebugViewKind::TextureCubeFace) {
            program = pbr_debug_texture_cube_program_;
        }

        if (!program.IsValid() || !selected->texture_view.IsValid()) {
            return;
        }

        const Math::Vec4 params{static_cast<float>(selected->array_slice), static_cast<float>(selected->cube_face),
                                static_cast<float>(selected->mip_level), selected->max_value};
        context.SetFrameBuffer(scene_framebuffer_);
        context.UseShaderProgram(program);
        context.BindTexture("g_DebugTexture", selected->texture_view);
        context.BindUniform("DebugTexture", params);
        context.DrawFullscreenTriangle();
    }

    void RenderSystem::DrawPbrSkybox(RenderPassContext &context, const CameraData &camera, float intensity) {
        if (!desc_.pbr.ibl.enabled) {
            return;
        }

        const float skybox_intensity = PositiveFiniteOrZero(intensity);
        if (skybox_intensity <= 0.f) {
            return;
        }

        const bool has_generated_environment =
                pbr_ibl_resources_.generated && pbr_ibl_resources_.active_environment_cube_srv.IsValid();
        if (has_generated_environment && !pbr_skybox_program_.IsValid()) {
            return;
        }
        if (!has_generated_environment && (!pbr_skybox_fallback_program_.IsValid() ||
                                           (!pbr_ibl_resources_.generation_pending &&
                                            pbr_ibl_resources_.source_key.empty()))) {
            return;
        }

        const bool perspective = IsPerspectiveProjection(camera.projection);
        const float tan_half_x = perspective ? ProjectionHalfWidthAtDepth(camera.projection, 1.f) : 0.f;
        const float tan_half_y = perspective ? ProjectionHalfHeightAtDepth(camera.projection, 1.f) : 0.f;
        const Math::Vec3 right = ExtractCameraRight(camera);
        const Math::Vec3 up = ExtractCameraUp(camera);
        const Math::Vec3 forward = ExtractCameraForward(camera);
        const float exposure = std::max(PositiveFiniteOrZero(desc_.post_process.exposure), 1.0e-5f);
        const float fallback_scale = std::min(1.0f / exposure, 65504.0f);
        const PbrSkyboxParams params{
                .camera_right_tan_x = Math::Vec4(right.x, right.y, right.z, tan_half_x),
                .camera_up_tan_y = Math::Vec4(up.x, up.y, up.z, tan_half_y),
                .camera_forward_intensity = Math::Vec4(forward.x, forward.y, forward.z, skybox_intensity),
                .fallback_horizon = Math::Vec4(0.08f * fallback_scale, 0.10f * fallback_scale,
                                               0.12f * fallback_scale, 1.f),
                .fallback_zenith = Math::Vec4(0.16f * fallback_scale, 0.24f * fallback_scale,
                                              0.36f * fallback_scale, 1.f),
        };

        context.SetFrameBuffer(scene_framebuffer_);
        context.UseShaderProgram(has_generated_environment ? pbr_skybox_program_ : pbr_skybox_fallback_program_);
        if (has_generated_environment) {
            context.BindTexture("g_SkyboxCube", pbr_ibl_resources_.active_environment_cube_srv);
        }
        context.BindUniform("Skybox", params);
        context.DrawFullscreenTriangle();
    }

    void RenderSystem::ExecuteDefaultScenePass(RenderPassContext &context) {
        World &world = context.GetWorld();
        auto group = world.Registry().group<TransformComponent, MeshRendererComponent>();
        accumulator_.Reserve(group.size());
        accumulator_.Clear();
        world_transform_cache_.clear();
        world_transform_cache_.reserve(group.size());

        for (auto [entity, transform, renderer]: group.each()) {
            if (!renderer.visible || !renderer.material.IsValid() || !renderer.mesh.IsValid()) {
                continue;
            }

            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            const Math::Mat4 world_matrix = hierarchy == nullptr || hierarchy->parent == entt::null
                                                    ? transform.WorldMatrix()
                                                    : ResolveCachedWorldMatrix(world, entity, world_transform_cache_);
            accumulator_.Add(renderer.material, renderer.mesh, world_matrix);
        }

        const CameraData active_camera =
                has_manual_camera_override_ ? manual_camera_override_ : ResolveWorldCamera(world);

        EnvironmentLightFrameData environment_light = ResolveEnvironmentLight(world);
        environment_light.texture_ibl_enabled = pbr_ibl_resources_.generated;
        environment_light.ibl_prefiltered_mip_count =
                pbr_ibl_resources_.generated ? static_cast<float>(pbr_ibl_resources_.prefiltered_mip_count) : 1.f;
        const Math::Vec3 active_camera_position = ExtractCameraPosition(active_camera);
        const ActiveReflectionProbe active_probe =
                ResolveActiveReflectionProbe(world, world_transform_cache_, active_camera_position);
        if (active_probe.enabled && pbr_ibl_resources_.generated) {
            environment_light.enabled = true;
        }

        RenderDebugStats &stats = debug_registry_.Stats();
        stats.reflection_probe_active = active_probe.enabled;
        stats.reflection_probe_priority = active_probe.priority;
        stats.reflection_probe_radius = active_probe.radius;
        stats.reflection_probe_intensity = active_probe.intensity;
        stats.reflection_probe_camera_influence =
                active_probe.enabled
                        ? std::clamp(1.f - Math::Length(active_camera_position - active_probe.position) /
                                                   std::max(active_probe.radius, 0.001f),
                                     0.f, 1.f)
                        : 0.f;

        PerFrameProps props{.camera = active_camera,
                            .frame_clock = Math::Vec4(context.DeltaSeconds(),
                                                      static_cast<float>(context.TotalSeconds()), 0.0f, 0.0f),
                            .camera_position = ExtractCameraPosition(active_camera),
                            .exposure = desc_.post_process.exposure,
                            .directional_light = ResolveDirectionalLight(world),
                            .environment_light = environment_light,
                            .shadows = pbr_shadow_frame_data_,
                            .reflection_probe_position_radius =
                                    Math::Vec4(active_probe.position.x, active_probe.position.y,
                                               active_probe.position.z, active_probe.radius),
                            .reflection_probe_params =
                                    Math::Vec4(active_probe.enabled ? 1.f : 0.f, active_probe.intensity,
                                               static_cast<float>(active_probe.priority), 0.f),
                            .pbr_debug_params =
                                    Math::Vec4(DebugModeFromName(debug_registry_.SelectedName()), 0.f, 0.f, 0.f)};
        props.point_light_count = ResolvePointLights(world, world_transform_cache_, props.point_lights,
                                                     pbr_shadow_resources_.max_point_lights);

        context.SetPerFrameProps(props);
        context.SetPbrGlobalResources(pbr_global_resources_);
        context.SetFrameBuffer(scene_framebuffer_);
        context.Clear(desc_.clear_color);
        const float skybox_intensity =
                active_probe.enabled ? active_probe.intensity
                                     : std::max(environment_light.intensity, environment_light.specular_intensity);
        DrawPbrSkybox(context, active_camera, skybox_intensity);

        const FrameBufferColorView scene_color = context.GetFrameBufferColorView(scene_framebuffer_);
        const FrameBufferDepthView scene_depth = context.GetFrameBufferDepthView(scene_framebuffer_);
        context.SetGlobalColorTexture(GlobalTextureSlot::SceneColor, scene_color);
        context.SetGlobalDepthTexture(GlobalTextureSlot::SceneDepth, scene_depth);

        if (desc_.pbr.visual_debug) {
            debug_registry_.RegisterView(RenderDebugView{
                    .name = "scene/color-hdr-before-tonemap",
                    .kind = RenderDebugViewKind::Color,
                    .color_view = scene_color,
            });
            debug_registry_.RegisterView(RenderDebugView{
                    .name = "scene/depth",
                    .kind = RenderDebugViewKind::Depth,
                    .depth_view = scene_depth,
            });
            debug_registry_.RegisterView(RenderDebugView{.name = "material/base-color",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "material/world-normal",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "material/metallic",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "material/roughness",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "material/ao", .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "material/emissive",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "lighting/light-counts",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "probes/reflection-influence",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "probes/reflection-selected",
                                                         .kind = RenderDebugViewKind::Overlay});
            debug_registry_.RegisterView(RenderDebugView{.name = "ibl/specular-roughness-lod",
                                                         .kind = RenderDebugViewKind::Overlay});
        }

        for (const RenderBatch &batch: accumulator_.Batches()) {
            context.SubmitBatch(batch);
        }
    }

    void RenderSystem::PumpModelUploads() {
        if (backend_ == nullptr || models_ == nullptr) {
            return;
        }

        std::vector<PendingModelUpload> pending_uploads;
        {
            std::lock_guard lock{models_->mutex};
            for (auto it = models_->records.begin(); it != models_->records.end(); ++it) {
                ModelRegistry::Record &record = it.value();
                if (record.state != ModelLoadState::Pending || record.decoded_result == nullptr) {
                    continue;
                }

                pending_uploads.push_back(PendingModelUpload{
                        .handle = ModelHandle{.id = it.key(), .generation = record.generation},
                        .result = std::move(*record.decoded_result),
                        .material_pipeline = record.material_pipeline,
                        .completion = record.completion,
                });
                record.decoded_result.reset();
            }
        }

        for (PendingModelUpload &upload: pending_uploads) {
            if (!upload.result.IsSuccess()) {
                std::string error_message = upload.result.error_message.empty()
                                                    ? "Failed to import model"
                                                    : std::move(upload.result.error_message);
                bool reject_future = false;
                {
                    std::lock_guard lock{models_->mutex};
                    const auto it = models_->records.find(upload.handle.id);
                    if (it != models_->records.end() && it.value().generation == upload.handle.generation) {
                        it.value().state = ModelLoadState::Failed;
                        it.value().error_message = error_message;
                        reject_future = true;
                    }
                }

                if (reject_future) {
                    upload.completion.Reject(std::move(error_message));
                }
                continue;
            }

            UploadedModelResources resources = BuildModelResources(upload.result.asset, upload.material_pipeline);
            std::string error_message = resources.error_message;

            bool keep_uploaded_meshes = false;
            bool resolve_future = false;
            bool reject_future = false;
            {
                std::lock_guard lock{models_->mutex};
                const auto it = models_->records.find(upload.handle.id);
                if (it != models_->records.end() && it.value().generation == upload.handle.generation) {
                    if (error_message.empty()) {
                        it.value().meshes = std::move(resources.meshes);
                        it.value().mesh_names = std::move(resources.mesh_names);
                        it.value().materials = std::move(resources.materials);
                        it.value().mesh_material_indices = std::move(resources.mesh_material_indices);
                        it.value().nodes = std::move(resources.nodes);
                        it.value().state = ModelLoadState::Ready;
                        keep_uploaded_meshes = true;
                        resolve_future = true;
                    } else {
                        it.value().state = ModelLoadState::Failed;
                        it.value().error_message = error_message;
                        reject_future = true;
                    }
                }
            }

            if (!keep_uploaded_meshes) {
                DestroyUploadedMeshes(*backend_, resources.meshes);
            }

            if (resolve_future) {
                upload.completion.Resolve(upload.handle);
            } else if (reject_future) {
                upload.completion.Reject(std::move(error_message));
            }
        }
    }

    void RenderSystem::DestroyAllModels() {
        if (models_ == nullptr) {
            return;
        }

        StopModelLoadWorker();

        std::vector<ModelRegistry::Record> records;
        {
            std::lock_guard lock{models_->mutex};
            records.reserve(models_->records.size());
            for (auto it = models_->records.begin(); it != models_->records.end(); ++it) {
                records.push_back(std::move(it.value()));
            }
            models_->records.clear();
            models_->texture_cache.clear();
        }

        for (const ModelRegistry::Record &record: records) {
            if (record.state == ModelLoadState::Pending) {
                record.completion.Cancel("Model load was cancelled");
            }
        }

        if (backend_ != nullptr) {
            for (const ModelRegistry::Record &record: records) {
                for (MeshHandle mesh: record.meshes) {
                    backend_->DestroyMesh(mesh);
                }
            }
        }
    }

    TextureHandle RenderSystem::LoadModelTexture(const ModelTextureAsset &texture) {
        if (!texture.IsValid() || models_ == nullptr) {
            return {};
        }

        const std::string cache_key = ModelTextureCacheKey(texture);
        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->texture_cache.find(cache_key);
            if (it != models_->texture_cache.end()) {
                return it->second;
            }
        }

        const TextureLoadDesc desc{
                .path = texture.path,
                .data = texture.data,
                .format = texture.srgb ? TextureFormat::RGBA8UnormSrgb : TextureFormat::RGBA8Unorm,
                .generate_mipmaps = true,
                .flip_vertically = false,
                .premultiply_alpha = false,
        };

        TextureHandle loaded_texture = LoadTexture2DAsync(desc);
        if (!loaded_texture.IsValid()) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto existing = models_->texture_cache.find(cache_key);
        if (existing != models_->texture_cache.end()) {
            return existing->second;
        }

        models_->texture_cache[cache_key] = loaded_texture;
        return loaded_texture;
    }

    MaterialHandle RenderSystem::ResolveModelMaterial(const ModelMaterialAsset &material,
                                                      ModelMaterialPipeline pipeline) {
        const ModelTextureAsset *base_color_texture = FindModelTexture(material, ModelTextureSemantic::BaseColor);
        if (pipeline == ModelMaterialPipeline::PbrStandard) {
            PbrStandardDesc desc = PbrStandardDesc::Linear(material.base_color, material.metallic, material.roughness);

            if (base_color_texture != nullptr) {
                desc.base_color_texture = LoadModelTexture(*base_color_texture);
            }

            if (const ModelTextureAsset *normal_texture = FindModelTexture(material, ModelTextureSemantic::Normal)) {
                desc.normal_texture = LoadModelTexture(*normal_texture);
            }

            if (const ModelTextureAsset *metallic_texture =
                        FindModelTexture(material, ModelTextureSemantic::Metallic)) {
                desc.metallic_texture = LoadModelTexture(*metallic_texture);
            }

            if (const ModelTextureAsset *roughness_texture =
                        FindModelTexture(material, ModelTextureSemantic::Roughness)) {
                desc.roughness_texture = LoadModelTexture(*roughness_texture);
            }

            if (const ModelTextureAsset *metallic_roughness_texture =
                        FindModelTexture(material, ModelTextureSemantic::MetallicRoughness)) {
                desc.metallic_roughness_texture = LoadModelTexture(*metallic_roughness_texture);
            }

            if (const ModelTextureAsset *ambient_occlusion_texture =
                        FindModelTexture(material, ModelTextureSemantic::Occlusion)) {
                desc.ambient_occlusion_texture = LoadModelTexture(*ambient_occlusion_texture);
            }

            if (const ModelTextureAsset *emissive_texture =
                        FindModelTexture(material, ModelTextureSemantic::Emissive)) {
                desc.emissive_texture = LoadModelTexture(*emissive_texture);
                if (desc.emissive_texture.IsValid()) {
                    desc.props.emissive = {1.f, 1.f, 1.f, 0.f};
                }
            }

            return Material::PbrStandard(desc).Resolve(*this);
        }

        if (base_color_texture != nullptr) {
            const TextureHandle albedo = LoadModelTexture(*base_color_texture);
            if (albedo.IsValid()) {
                return Material::TexturedUnlit(albedo, TexturedUnlitProps{.color = material.base_color}).Resolve(*this);
            }
        }

        return Material::Unlit(UnlitProps{.color = material.base_color}).Resolve(*this);
    }

    RenderSystem::UploadedModelResources RenderSystem::BuildModelResources(const ModelAsset &asset,
                                                                           ModelMaterialPipeline material_pipeline) {
        UploadedModelResources resources;
        if (backend_ == nullptr || !asset.IsValid()) {
            resources.error_message = "Invalid model asset";
            return resources;
        }

        resources.meshes.reserve(asset.meshes.size());
        resources.mesh_names.reserve(asset.meshes.size());
        resources.mesh_material_indices.reserve(asset.meshes.size());
        resources.materials.reserve(asset.materials.empty() ? 1u : asset.materials.size());
        resources.nodes = asset.nodes;

        if (asset.materials.empty()) {
            const MaterialHandle material = material_pipeline == ModelMaterialPipeline::PbrStandard
                                                    ? Material::PbrStandard().Resolve(*this)
                                                    : Material::Unlit().Resolve(*this);
            if (!material.IsValid()) {
                resources.error_message = backend_->LastError().empty() ? "Failed to resolve default model material"
                                                                        : std::string{backend_->LastError()};
                return resources;
            }
            resources.materials.push_back(material);
        } else {
            for (const ModelMaterialAsset &material_asset: asset.materials) {
                const MaterialHandle material = ResolveModelMaterial(material_asset, material_pipeline);
                if (!material.IsValid()) {
                    resources.error_message = backend_->LastError().empty() ? "Failed to resolve model material"
                                                                            : std::string{backend_->LastError()};
                    return resources;
                }
                resources.materials.push_back(material);
            }
        }

        for (const ModelMeshAsset &mesh: asset.meshes) {
            const MeshDesc mesh_desc{
                    .vertices = mesh.vertices,
                    .indices = mesh.indices,
            };

            MeshHandle mesh_handle = backend_->UploadMesh(mesh_desc);
            if (!mesh_handle.IsValid()) {
                resources.error_message = backend_->LastError().empty() ? "Failed to upload model mesh"
                                                                        : std::string{backend_->LastError()};
                DestroyUploadedMeshes(*backend_, resources.meshes);
                return resources;
            }

            resources.meshes.push_back(mesh_handle);
            resources.mesh_names.push_back(mesh.name);
            resources.mesh_material_indices.push_back(
                    NormalizeModelMaterialIndex(mesh.material_index, resources.materials.size()));
        }

        if (resources.nodes.empty()) {
            resources.nodes.reserve(resources.meshes.size());
            for (std::uint32_t mesh_index = 0; mesh_index < resources.meshes.size(); ++mesh_index) {
                resources.nodes.push_back(ModelNodeAsset{
                        .name = MakeModelMeshNodeName(resources.mesh_names[mesh_index], mesh_index),
                        .parent_index = kInvalidModelNodeIndex,
                        .local_transform = Math::Identity(),
                        .mesh_indices = {mesh_index},
                });
            }
        }

        return resources;
    }

    bool RenderSystem::CreateSceneFrameBuffer() {
        if (backend_ == nullptr) {
            return false;
        }

        FrameBufferDesc desc;
        desc.width = surface_width_;
        desc.height = surface_height_;
        desc.sample_color = true;
        desc.has_depth = true;
        desc.sample_depth = true;
        desc.color_format = desc_.scene_color_format;
        desc.depth_format = FrameBufferFormat::Depth32Float;

        scene_framebuffer_ = backend_->CreateFrameBuffer(desc);
        return scene_framebuffer_.IsValid();
    }

    void RenderSystem::DestroySceneFrameBuffer() {
        if (backend_ == nullptr || !scene_framebuffer_.IsValid()) {
            return;
        }

        backend_->DestroyFrameBuffer(scene_framebuffer_);
        scene_framebuffer_ = {};
    }

    CameraData RenderSystem::ResolveWorldCamera(World &world) const {
        entt::entity best_entity = entt::null;
        const CameraComponent *best_camera = nullptr;

        auto group = world.Registry().group<CameraComponent>(entt::get<TransformComponent>);
        for (const entt::entity entity: group) {
            const CameraComponent &camera = group.get<CameraComponent>(entity);

            if (!camera.enabled) {
                continue;
            }

            if (best_camera == nullptr || camera.priority > best_camera->priority) {
                best_camera = &camera;
                best_entity = entity;
            }
        }

        if (best_entity == entt::null || best_camera == nullptr) {
            return default_camera_;
        }

        const Node camera_node{best_entity, &world};
        return BuildCameraData(camera_node.GetWorldPosition(), camera_node.GetWorldRotation(), *best_camera);
    }

    CameraData RenderSystem::BuildCameraData(const Math::Vec3 &position, const Math::Quat &rotation,
                                             const CameraComponent &camera) const {
        const int width = surface_width_ > 0 ? surface_width_ : 1;
        const int height = surface_height_ > 0 ? surface_height_ : 1;

        const float aspect_ratio = camera.aspect_mode == CameraAspectMode::Fixed
                                           ? camera.fixed_aspect_ratio
                                           : static_cast<float>(width) / static_cast<float>(height);

        const Math::Vec3 forward = rotation * Math::Vec3{0.f, 0.f, 1.f};
        const Math::Vec3 up = rotation * Math::Vec3{0.f, 1.f, 0.f};

        CameraData data;
        data.view = Math::LookAtLH(position, position + forward, up);

        if (camera.projection_type == CameraProjectionType::Perspective) {
            data.projection =
                    Math::PerspectiveLH(Math::Deg2Rad(camera.fov_y_degrees), aspect_ratio, camera.near_z, camera.far_z);
            return data;
        }

        const float half_height = camera.orthographic_height * 0.5f;
        const float half_width = half_height * aspect_ratio;

        data.projection =
                Math::OrthoLH(-half_width, half_width, -half_height, half_height, camera.near_z, camera.far_z);

        return data;
    }
} // namespace CoreEngine
