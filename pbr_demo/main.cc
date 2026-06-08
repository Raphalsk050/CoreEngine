#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/directional_light_component.h"
#include "core/ecs/components/environment_light_component.h"
#include "core/ecs/components/point_light_component.h"
#include "core/ecs/components/reflection_probe_component.h"
#include "core/ecs/world.h"
#include "core/i_game_app.h"
#include "core/input/input_system.h"
#include "core/log/log.h"
#include "core/math/math.h"
#include "core/render/material.h"
#include "core/render/primitive_type.h"
#include "core/render/render_desc.h"
#include "core/render/render_system.h"
#include "core/render/texture_desc.h"

namespace {
    constexpr float kCameraMoveSpeed = 5.5f;
    constexpr float kCameraSprintMultiplier = 4.0f;
    constexpr float kCameraKeyLookSpeed = 1.65f;
    constexpr float kCameraMouseLookSpeed = 0.0025f;
    constexpr float kCameraMaxPitch = CoreEngine::Math::Deg2Rad(84.0f);

    constexpr CoreEngine::InputActionId kMoveCameraAction = CoreEngine::MakeInputActionId(101);
    constexpr CoreEngine::InputActionId kLookCameraAction = CoreEngine::MakeInputActionId(102);
    constexpr CoreEngine::InputActionId kCameraUpAction = CoreEngine::MakeInputActionId(103);
    constexpr CoreEngine::InputActionId kCameraDownAction = CoreEngine::MakeInputActionId(104);
    constexpr CoreEngine::InputActionId kCameraSprintAction = CoreEngine::MakeInputActionId(105);

    [[nodiscard]] std::string FindAssetPath(std::string_view relative_path) {
        namespace fs = std::filesystem;

        const fs::path relative{relative_path};
        const fs::path source_root = fs::path{__FILE__}.parent_path().parent_path();
        const fs::path current = fs::current_path();
        const std::array<fs::path, 5> roots{
                current,     current.parent_path(),     current.parent_path().parent_path(),
                source_root, source_root.parent_path(),
        };

        for (const fs::path &root: roots) {
            std::error_code error;
            const fs::path candidate = root / relative;
            if (fs::exists(candidate, error)) {
                return candidate.string();
            }
        }

        return std::string{relative_path};
    }

    struct IblCachePaths {
        std::string environment_cube_path{};
        std::string irradiance_cube_path{};
        std::string prefiltered_specular_cube_path{};
        std::string brdf_lut_path{};
        std::string manifest_path{};
    };

    [[nodiscard]] IblCachePaths BuildIblCachePaths(std::string_view cache_name) {
        namespace fs = std::filesystem;

        const fs::path source_root = fs::path{__FILE__}.parent_path().parent_path();
        const fs::path cache_root = source_root / "build" / "pbr_cache";
        const std::string name{cache_name};
        return IblCachePaths{
                .environment_cube_path = (cache_root / (name + ".environment.dds")).string(),
                .irradiance_cube_path = (cache_root / (name + ".irradiance.dds")).string(),
                .prefiltered_specular_cube_path = (cache_root / (name + ".prefiltered.dds")).string(),
                .brdf_lut_path = (cache_root / (name + ".brdf.dds")).string(),
                .manifest_path = (cache_root / (name + ".iblcache")).string(),
        };
    }

    [[nodiscard]] CoreEngine::Material MakePbr(const CoreEngine::Math::Vec4 &srgb_base_color, float metallic,
                                               float roughness) {
        return CoreEngine::Material::PbrStandard(
                CoreEngine::PbrStandardDesc::Srgb(srgb_base_color, metallic, roughness));
    }

    [[nodiscard]] CoreEngine::Material MakeEmissivePbr(const CoreEngine::Math::Vec4 &srgb_base_color,
                                                       const CoreEngine::Math::Vec3 &emissive) {
        CoreEngine::PbrStandardDesc desc = CoreEngine::PbrStandardDesc::Srgb(srgb_base_color, 0.0f, 0.35f);
        desc.props.emissive = CoreEngine::Math::Vec4{emissive.x, emissive.y, emissive.z, 1.0f};
        return CoreEngine::Material::PbrStandard(desc);
    }

    enum class PbrDemoMode {
        FullPbr,
        PbrFeaturesOff,
    };

    [[nodiscard]] constexpr std::string_view ModeName(PbrDemoMode mode) noexcept {
        switch (mode) {
            case PbrDemoMode::FullPbr:        return "full";
            case PbrDemoMode::PbrFeaturesOff: return "features-off";
        }

        return "unknown";
    }

    [[nodiscard]] PbrDemoMode ParsePbrDemoMode(int argc, char **argv) noexcept {
        for (int index = 1; index < argc; ++index) {
            if (argv[index] == nullptr) {
                continue;
            }

            const std::string_view arg{argv[index]};
            if (arg == "--pbr-features-off" || arg == "--pbr-baseline") {
                return PbrDemoMode::PbrFeaturesOff;
            }
        }

        return PbrDemoMode::FullPbr;
    }

    void ApplyPbrFeaturesOff(CoreEngine::PbrRenderSettings &settings) noexcept {
        settings = CoreEngine::PbrRenderSettings::Low();
        settings.preset = CoreEngine::PbrQualityPreset::Custom;
        settings.shadows.directional_shadows = false;
        settings.shadows.point_shadows = false;
        settings.shadows.directional_shadow_resolution = 128u;
        settings.shadows.max_shadowed_point_lights = 0u;
        settings.shadows.point_shadow_resolution = 64u;
        settings.ibl.enabled = false;
        settings.ibl.environment_cube_resolution = 16u;
        settings.ibl.irradiance_resolution = 8u;
        settings.ibl.prefiltered_specular_resolution = 16u;
        settings.ibl.prefiltered_specular_mip_count = 1u;
        settings.ibl.brdf_lut_resolution = 16u;
        settings.visual_debug = false;
    }

    /**
     * @brief Owns the standalone PBR showcase scene.
     *
     * Responsibility: build a small ECS scene that exercises PBR materials,
     * directional and point shadows, runtime IBL, reflection probes, and visual
     * debug view selection without leaking demo policy into CoreEngine modules.
     */
    class PbrDemoApp final : public CoreEngine::IGameApp {
    public:
        explicit PbrDemoApp(PbrDemoMode mode) noexcept : mode_(mode) {
        }

        void Init(const CoreEngine::EngineContext &context) override {
            BindCameraControls(context.input_system);
            ConfigurePostProcess(context.render_system);
            CreateCamera(context.world);
            CreateScene(context);
        }

        void Update(const CoreEngine::FrameContext &frame) override {
            elapsed_seconds_ += frame.delta_time;
            UpdateCamera(frame);
            AnimateDemo(frame.delta_time);
            UpdateDebugViewControls(frame.render_system, frame.input_system);
            ReportPerfStats(frame.render_system, frame.delta_time);
        }

        void Shutdown(const CoreEngine::EngineContext &context) override {
            for (CoreEngine::Node &node: owned_nodes_) {
                if (node.IsValid()) {
                    node.Destroy();
                }
            }
            owned_nodes_.clear();

            static_cast<void>(context);
        }

    private:
        [[nodiscard]] bool UsesRuntimeIbl() const noexcept {
            return mode_ == PbrDemoMode::FullPbr;
        }

        static void BindCameraControls(CoreEngine::InputSystem &input_system) noexcept {
            static_cast<void>(input_system.BindAxis2D(kMoveCameraAction, CoreEngine::Key::A, CoreEngine::Key::D,
                                                      CoreEngine::Key::S, CoreEngine::Key::W));
            static_cast<void>(input_system.BindAxis2D(kLookCameraAction, CoreEngine::Key::Left, CoreEngine::Key::Right,
                                                      CoreEngine::Key::Down, CoreEngine::Key::Up));
            static_cast<void>(input_system.BindButton(kCameraUpAction, CoreEngine::Key::E));
            static_cast<void>(input_system.BindButton(kCameraDownAction, CoreEngine::Key::Q));
            static_cast<void>(input_system.BindButton(kCameraSprintAction, CoreEngine::Key::LeftShift));
        }

        static void ConfigurePostProcess(CoreEngine::RenderSystem &render_system) {
            render_system.SetPostProcess(CoreEngine::PostProcessDesc{
                    .exposure = 0.00018f,
                    .tone_mapping = CoreEngine::ToneMappingOperator::AcesFilmic,
            });
        }

        void CreateCamera(CoreEngine::World &world) {
            camera_position_ = CoreEngine::Math::Vec3{0.0f, 3.8f, -11.5f};
            camera_yaw_ = 0.0f;
            camera_pitch_ = CoreEngine::Math::Deg2Rad(-12.0f);

            camera_node_ = world.CreateNode("PBR Demo Camera");
            owned_nodes_.push_back(camera_node_);
            camera_node_.AddComponent<CoreEngine::CameraComponent>(CoreEngine::CameraComponent{
                    .projection_type = CoreEngine::CameraProjectionType::Perspective,
                    .aspect_mode = CoreEngine::CameraAspectMode::RenderSurface,
                    .fov_y_degrees = 55.0f,
                    .orthographic_height = 10.0f,
                    .near_z = 0.02f,
                    .far_z = 280.0f,
                    .fixed_aspect_ratio = 16.0f / 9.0f,
                    .priority = 100,
                    .enabled = true,
            });
            ApplyCameraTransform();
        }

        void CreateScene(const CoreEngine::EngineContext &context) {
            std::string hdr_path;
            if (UsesRuntimeIbl()) {
                hdr_path = FindAssetPath("core_engine/assets/textures/TestCubeMap_radiance.hdr");
            }

            CreateEnvironment(context.world, hdr_path);
            CreateLights(context.world);
            CreateProbe(context.world);
            CreateGeometry(context.world, context.render_system);
        }

        void CreateEnvironment(CoreEngine::World &world, const std::string &hdr_path) {
            const bool use_ibl = UsesRuntimeIbl();
            const std::string cache_name =
                    hdr_path.empty() ? std::string{"pbr_environment"} : std::filesystem::path{hdr_path}.stem().string();
            const IblCachePaths ibl_cache = BuildIblCachePaths(cache_name.empty() ? "pbr_environment" : cache_name);
            CoreEngine::Node environment = world.CreateNode("PBR Environment");
            owned_nodes_.push_back(environment);
            environment.AddComponent<CoreEngine::EnvironmentLightComponent>(CoreEngine::EnvironmentLightComponent{
                    .hdr_equirectangular_path = use_ibl ? hdr_path : std::string{},
                    .precomputed_environment_cube_path = use_ibl ? ibl_cache.environment_cube_path : std::string{},
                    .precomputed_irradiance_cube_path = use_ibl ? ibl_cache.irradiance_cube_path : std::string{},
                    .precomputed_prefiltered_specular_cube_path =
                            use_ibl ? ibl_cache.prefiltered_specular_cube_path : std::string{},
                    .precomputed_brdf_lut_path = use_ibl ? ibl_cache.brdf_lut_path : std::string{},
                    .precomputed_ibl_manifest_path = use_ibl ? ibl_cache.manifest_path : std::string{},
                    .environment_map = {},
                    .diffuse_irradiance = CoreEngine::Math::Vec3{0.035f, 0.04f, 0.045f},
                    .intensity = use_ibl ? 900.0f : 0.0f,
                    .specular_radiance = CoreEngine::Math::Vec3{0.04f, 0.045f, 0.05f},
                    .specular_intensity = use_ibl ? 900.0f : 0.0f,
                    .bake_precomputed_ibl_if_missing = use_ibl,
                    .enabled = true,
            });
        }

        void CreateLights(CoreEngine::World &world) {
            CoreEngine::Node sun = world.CreateNode("PBR Directional Light");
            owned_nodes_.push_back(sun);
            sun.AddComponent<CoreEngine::DirectionalLightComponent>(CoreEngine::DirectionalLightComponent{
                    .direction = CoreEngine::Math::Normalize(CoreEngine::Math::Vec3{-0.42f, -1.0f, 0.35f}),
                    .illuminance_lux = 12000.0f,
                    .color = CoreEngine::Math::Vec3{1.0f, 0.96f, 0.9f},
                    .enabled = true,
                    .cast_shadows = true,
                    .shadow_strength = 0.9f,
                    .shadow_bias = 0.0012f,
                    .shadow_normal_bias = 0.018f,
            });

            point_light_node_ = world.CreateNode("PBR Shadowed Point Light");
            owned_nodes_.push_back(point_light_node_);
            point_light_node_.SetPosition(CoreEngine::Math::Vec3{3.0f, 3.6f, -2.6f});
            point_light_node_.AddComponent<CoreEngine::PointLightComponent>(CoreEngine::PointLightComponent{
                    .color = CoreEngine::Math::Vec3{1.0f, 0.58f, 0.32f},
                    .luminous_intensity_cd = 260.0f,
                    .range = 9.5f,
                    .enabled = true,
                    .cast_shadows = true,
                    .shadow_near_z = 0.08f,
                    .shadow_bias = 0.004f,
                    .shadow_normal_bias = 0.025f,
            });
        }

        void CreateProbe(CoreEngine::World &world) {
            if (!UsesRuntimeIbl()) {
                return;
            }

            CoreEngine::Node probe = world.CreateNode("PBR Static Reflection Probe");
            owned_nodes_.push_back(probe);
            probe.SetPosition(CoreEngine::Math::Vec3{0.0f, 1.2f, -3.0f});
            probe.AddComponent<CoreEngine::ReflectionProbeComponent>(CoreEngine::ReflectionProbeComponent{
                    .environment_map = {},
                    .radius = 16.0f,
                    .box_extent = CoreEngine::Math::Vec3{0.0f, 0.0f, 0.0f},
                    .intensity = 900.0f,
                    .priority = 10,
                    .use_scene_environment = true,
                    .enabled = true,
            });
        }

        void CreateGeometry(CoreEngine::World &world, CoreEngine::RenderSystem &render_system) {
            CreatePrimitive(world, render_system, "PBR Ground Plane", CoreEngine::PrimitiveType::Plane,
                            CoreEngine::Math::Vec3{0.0f, 0.0f, -1.0f}, CoreEngine::Math::Vec3{18.0f, 1.0f, 16.0f},
                            MakePbr(CoreEngine::Math::Vec4{0.43f, 0.45f, 0.42f, 1.0f}, 0.0f, 0.82f), false);

            CreatePrimitive(world, render_system, "PBR Left Side Wall", CoreEngine::PrimitiveType::Cube,
                            CoreEngine::Math::Vec3{-7.2f, 1.35f, 4.0f}, CoreEngine::Math::Vec3{0.22f, 2.7f, 19.0f},
                            MakePbr(CoreEngine::Math::Vec4{0.32f, 0.37f, 0.43f, 1.0f}, 0.0f, 0.72f), false);
            CreatePrimitive(world, render_system, "PBR Right Side Wall", CoreEngine::PrimitiveType::Cube,
                            CoreEngine::Math::Vec3{7.2f, 1.35f, 4.0f}, CoreEngine::Math::Vec3{0.22f, 2.7f, 19.0f},
                            MakePbr(CoreEngine::Math::Vec4{0.32f, 0.37f, 0.43f, 1.0f}, 0.0f, 0.72f), false);

            CreatePrimitive(world, render_system, "PBR Tall Shadow Caster", CoreEngine::PrimitiveType::Cube,
                            CoreEngine::Math::Vec3{-3.2f, 1.15f, 1.5f}, CoreEngine::Math::Vec3{1.1f, 2.3f, 1.1f},
                            MakePbr(CoreEngine::Math::Vec4{0.35f, 0.42f, 0.62f, 1.0f}, 0.0f, 0.42f), true);

            CoreEngine::Node angled_caster = CreatePrimitive(
                    world, render_system, "PBR Angled Shadow Caster", CoreEngine::PrimitiveType::Cube,
                    CoreEngine::Math::Vec3{-1.5f, 0.7f, -1.6f}, CoreEngine::Math::Vec3{0.7f, 1.4f, 0.7f},
                    MakePbr(CoreEngine::Math::Vec4{0.78f, 0.52f, 0.32f, 1.0f}, 0.0f, 0.38f), true);
            if (angled_caster.IsValid()) {
                angled_caster.SetRotation(CoreEngine::Math::AngleAxis(CoreEngine::Math::Deg2Rad(31.0f),
                                                                      CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f}));
            }

            CreatePrimitive(world, render_system, "PBR Mirror Reference", CoreEngine::PrimitiveType::Sphere,
                            CoreEngine::Math::Vec3{3.2f, 0.92f, 1.65f}, CoreEngine::Math::Vec3{0.92f, 0.92f, 0.92f},
                            MakePbr(CoreEngine::Math::Vec4{0.86f, 0.9f, 0.92f, 1.0f}, 1.0f, 0.08f), true);

            CreateMaterialGrid(world, render_system);
            CreateCascadeTestGeometry(world, render_system);
            CreateProbeReferenceRoom(world, render_system);
            CreateStressLights(world, render_system);

            if (point_light_node_.IsValid()) {
                point_light_marker_node_ = CreatePrimitive(
                        world, render_system, "PBR Point Light Marker", CoreEngine::PrimitiveType::Sphere,
                        point_light_node_.GetPosition(), CoreEngine::Math::Vec3{0.16f, 0.16f, 0.16f},
                        MakeEmissivePbr(CoreEngine::Math::Vec4{1.0f, 0.58f, 0.32f, 1.0f},
                                        CoreEngine::Math::Vec3{1.4f, 0.55f, 0.2f}),
                        false, CoreEngine::RenderMobility::Dynamic);
            }
        }

        void CreateCascadeTestGeometry(CoreEngine::World &world, CoreEngine::RenderSystem &render_system) {
            CreatePrimitive(world, render_system, "PBR CSM Test Floor", CoreEngine::PrimitiveType::Plane,
                            CoreEngine::Math::Vec3{0.0f, -0.01f, 28.0f}, CoreEngine::Math::Vec3{12.0f, 1.0f, 52.0f},
                            MakePbr(CoreEngine::Math::Vec4{0.36f, 0.39f, 0.37f, 1.0f}, 0.0f, 0.88f), false);

            constexpr std::array<float, 6> z_positions{8.0f, 14.0f, 22.0f, 32.0f, 45.0f, 62.0f};
            for (std::size_t index = 0; index < z_positions.size(); ++index) {
                const float z = z_positions[index];
                const float height = 1.4f + static_cast<float>(index % 3u) * 0.55f;
                const float side = index % 2u == 0u ? -2.8f : 2.8f;
                CreatePrimitive(world, render_system, "PBR CSM Pillar", CoreEngine::PrimitiveType::Cube,
                                CoreEngine::Math::Vec3{side, height * 0.5f, z},
                                CoreEngine::Math::Vec3{0.85f, height, 0.85f},
                                MakePbr(CoreEngine::Math::Vec4{0.54f, 0.48f, 0.4f, 1.0f}, 0.0f, 0.55f), true);
                CreatePrimitive(world, render_system, "PBR CSM Marker Sphere", CoreEngine::PrimitiveType::Sphere,
                                CoreEngine::Math::Vec3{-side * 0.72f, 0.55f, z + 1.6f},
                                CoreEngine::Math::Vec3{0.45f, 0.45f, 0.45f},
                                MakePbr(CoreEngine::Math::Vec4{0.42f, 0.62f, 0.76f, 1.0f}, 0.0f, 0.5f), true);
            }
        }

        void CreateProbeReferenceRoom(CoreEngine::World &world, CoreEngine::RenderSystem &render_system) {
            CreatePrimitive(world, render_system, "PBR Probe Room Floor", CoreEngine::PrimitiveType::Cube,
                            CoreEngine::Math::Vec3{5.0f, 0.08f, -2.0f}, CoreEngine::Math::Vec3{3.2f, 0.16f, 3.2f},
                            MakePbr(CoreEngine::Math::Vec4{0.25f, 0.30f, 0.35f, 1.0f}, 0.0f, 0.35f), false);
            CreatePrimitive(world, render_system, "PBR Probe Room Back Wall", CoreEngine::PrimitiveType::Cube,
                            CoreEngine::Math::Vec3{5.0f, 1.2f, -0.3f}, CoreEngine::Math::Vec3{3.2f, 2.4f, 0.18f},
                            MakePbr(CoreEngine::Math::Vec4{0.50f, 0.38f, 0.34f, 1.0f}, 0.0f, 0.62f), false);
            CreatePrimitive(world, render_system, "PBR Probe Room Reflector", CoreEngine::PrimitiveType::Sphere,
                            CoreEngine::Math::Vec3{5.0f, 0.72f, -2.0f}, CoreEngine::Math::Vec3{0.62f, 0.62f, 0.62f},
                            MakePbr(CoreEngine::Math::Vec4{0.82f, 0.87f, 0.9f, 1.0f}, 1.0f, 0.12f), true);
        }

        void CreateStressLights(CoreEngine::World &world, CoreEngine::RenderSystem &render_system) {
            constexpr std::array<CoreEngine::Math::Vec3, 4> colors{{
                    {0.55f, 0.75f, 1.0f},
                    {0.65f, 1.0f, 0.55f},
                    {1.0f, 0.48f, 0.44f},
                    {0.86f, 0.62f, 1.0f},
            }};
            constexpr std::array<CoreEngine::Math::Vec3, 4> positions{{
                    {-5.4f, 2.3f, -3.2f},
                    {-4.8f, 2.2f, 3.8f},
                    {4.8f, 2.2f, 3.8f},
                    {5.4f, 2.3f, -3.2f},
            }};

            for (std::size_t index = 0; index < positions.size(); ++index) {
                CoreEngine::Node light = world.CreateNode("PBR Stress Fill Light");
                owned_nodes_.push_back(light);
                light.SetPosition(positions[index]);
                light.AddComponent<CoreEngine::PointLightComponent>(CoreEngine::PointLightComponent{
                        .color = colors[index],
                        .luminous_intensity_cd = 45.0f,
                        .range = 5.0f,
                        .enabled = true,
                        .cast_shadows = false,
                        .shadow_near_z = 0.08f,
                        .shadow_bias = 0.004f,
                        .shadow_normal_bias = 0.025f,
                });

                CreatePrimitive(
                        world, render_system, "PBR Stress Light Marker", CoreEngine::PrimitiveType::Sphere,
                        positions[index], CoreEngine::Math::Vec3{0.08f, 0.08f, 0.08f},
                        MakeEmissivePbr(CoreEngine::Math::Vec4{colors[index].x, colors[index].y, colors[index].z, 1.0f},
                                        colors[index] * 0.65f),
                        false);
            }
        }

        void CreateMaterialGrid(CoreEngine::World &world, CoreEngine::RenderSystem &render_system) {
            constexpr std::array<float, 5> roughness_values{0.08f, 0.22f, 0.42f, 0.68f, 0.92f};
            constexpr std::array<float, 2> metallic_values{0.0f, 1.0f};

            for (std::size_t row = 0; row < metallic_values.size(); ++row) {
                for (std::size_t column = 0; column < roughness_values.size(); ++column) {
                    const float x = -4.8f + static_cast<float>(column) * 1.75f;
                    const float z = -2.75f + static_cast<float>(row) * 2.25f;
                    const float metallic = metallic_values[row];
                    const CoreEngine::Math::Vec4 color = metallic > 0.5f
                                                                 ? CoreEngine::Math::Vec4{0.86f, 0.69f, 0.48f, 1.0f}
                                                                 : CoreEngine::Math::Vec4{0.32f, 0.58f, 0.88f, 1.0f};
                    CreatePrimitive(world, render_system, "PBR Material Sphere", CoreEngine::PrimitiveType::Sphere,
                                    CoreEngine::Math::Vec3{x, 0.78f, z}, CoreEngine::Math::Vec3{0.72f, 0.72f, 0.72f},
                                    MakePbr(color, metallic, roughness_values[column]), true);
                }
            }
        }

        CoreEngine::Node CreatePrimitive(CoreEngine::World &world, CoreEngine::RenderSystem &render_system,
                                         std::string_view name, CoreEngine::PrimitiveType type,
                                         const CoreEngine::Math::Vec3 &position, const CoreEngine::Math::Vec3 &scale,
                                         CoreEngine::Material material, bool cast_shadows,
                                         CoreEngine::RenderMobility mobility = CoreEngine::RenderMobility::Static) {
            CoreEngine::Node node = world.CreateNode(std::string{name});
            node.SetPosition(position);
            node.SetScale(scale);
            if (!render_system.SetPrimitiveRenderer(
                        node, CoreEngine::PrimitiveRendererDesc::WithMaterial(type, std::move(material), true,
                                                                              cast_shadows, mobility))) {
                node.Destroy();
                return {};
            }

            owned_nodes_.push_back(node);
            return node;
        }

        [[nodiscard]] CoreEngine::Math::Quat CameraOrientation() const noexcept {
            const CoreEngine::Math::Quat yaw =
                    CoreEngine::Math::AngleAxis(camera_yaw_, CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f});
            const CoreEngine::Math::Quat pitch =
                    CoreEngine::Math::AngleAxis(camera_pitch_, CoreEngine::Math::Vec3{-1.0f, 0.0f, 0.0f});
            return yaw * pitch;
        }

        void ApplyCameraTransform() {
            if (!camera_node_.IsValid()) {
                return;
            }

            camera_node_.SetPosition(camera_position_);
            camera_node_.SetRotation(CameraOrientation());
        }

        void UpdateCamera(const CoreEngine::FrameContext &frame) {
            if (!camera_node_.IsValid()) {
                return;
            }

            const CoreEngine::InputVector2 move_axis = frame.input_system.GetAxis2D(kMoveCameraAction);
            const CoreEngine::InputVector2 key_look_axis = frame.input_system.GetAxis2D(kLookCameraAction);
            const CoreEngine::InputVector2 mouse_delta =
                    frame.input_system.IsMouseButtonDown(CoreEngine::MouseButton::Right)
                            ? frame.input_system.MouseDelta()
                            : CoreEngine::InputVector2{};

            camera_yaw_ +=
                    key_look_axis.x * kCameraKeyLookSpeed * frame.delta_time + mouse_delta.x * kCameraMouseLookSpeed;
            camera_pitch_ +=
                    key_look_axis.y * kCameraKeyLookSpeed * frame.delta_time - mouse_delta.y * kCameraMouseLookSpeed;
            camera_pitch_ = std::clamp(camera_pitch_, -kCameraMaxPitch, kCameraMaxPitch);

            const float vertical_axis = (frame.input_system.IsActionDown(kCameraUpAction) ? 1.0f : 0.0f) -
                                        (frame.input_system.IsActionDown(kCameraDownAction) ? 1.0f : 0.0f);
            const float speed = kCameraMoveSpeed *
                                (frame.input_system.IsActionDown(kCameraSprintAction) ? kCameraSprintMultiplier : 1.0f);
            const CoreEngine::Math::Quat orientation = CameraOrientation();
            const CoreEngine::Math::Vec3 right = orientation * CoreEngine::Math::Vec3{1.0f, 0.0f, 0.0f};
            const CoreEngine::Math::Vec3 forward = orientation * CoreEngine::Math::Vec3{0.0f, 0.0f, 1.0f};
            const CoreEngine::Math::Vec3 up{0.0f, 1.0f, 0.0f};

            const CoreEngine::Math::Vec3 velocity =
                    (right * move_axis.x) + (forward * move_axis.y) + (up * vertical_axis);
            camera_position_ += velocity * (speed * frame.delta_time);
            ApplyCameraTransform();
        }

        void AnimateDemo(float delta_time) {
            if (!point_light_node_.IsValid()) {
                return;
            }

            point_light_angle_ += delta_time * 0.55f;
            const CoreEngine::Math::Vec3 position{
                    std::cos(point_light_angle_) * 3.1f,
                    3.45f + std::sin(elapsed_seconds_ * 0.7f) * 0.25f,
                    -1.2f + std::sin(point_light_angle_) * 2.1f,
            };
            point_light_node_.SetPosition(position);
            if (point_light_marker_node_.IsValid()) {
                point_light_marker_node_.SetPosition(position);
            }
        }

        void UpdateDebugViewControls(CoreEngine::RenderSystem &render_system,
                                     const CoreEngine::InputSystem &input_system) {
            if (input_system.WasKeyPressed(CoreEngine::Key::F2)) {
                render_system.ClearDebugView();
                debug_view_index_ = 0;
                return;
            }

            if (!input_system.WasKeyPressed(CoreEngine::Key::F1)) {
                return;
            }

            const std::span<const CoreEngine::RenderDebugView> views = render_system.GetAvailableDebugViews();
            if (views.empty()) {
                render_system.ClearDebugView();
                debug_view_index_ = 0;
                return;
            }

            debug_view_index_ = (debug_view_index_ + 1u) % (views.size() + 1u);
            if (debug_view_index_ == 0u) {
                render_system.ClearDebugView();
                return;
            }

            static_cast<void>(render_system.SetDebugView(views[debug_view_index_ - 1u].name));
        }

        void ReportPerfStats(const CoreEngine::RenderSystem &render_system, float delta_seconds) {
            perf_report_accumulator_ += delta_seconds;
            if (perf_report_accumulator_ < 2.0f) {
                return;
            }
            const CoreEngine::RenderDebugStats &stats = render_system.GetDebugStats();
            if (stats.frame_cpu_ms <= 0.0f) {
                return;
            }
            perf_report_accumulator_ = 0.0f;

            const double shadow_mb = static_cast<double>(stats.estimated_shadow_bytes) / (1024.0 * 1024.0);
            const double ibl_mb = static_cast<double>(stats.estimated_ibl_bytes) / (1024.0 * 1024.0);
            CoreEngine::Log::Info(
                    "PbrDemo",
                    "mode={} perf cpu_ms frame={:.2f} upload={:.2f} setup={:.2f} shadow={:.2f} scene={:.2f} "
                    "debug={:.2f} composite={:.2f} ui={:.2f} imgui={:.2f} present={:.2f}; shadows cascades={} "
                    "point_lights={} draws={}/{} bytes={:.2f} MiB; ibl bytes={:.2f} MiB ready={} generated={}; "
                    "probe active={} priority={} influence={:.2f} radius={:.2f} intensity={:.2f}",
                    ModeName(mode_), stats.frame_cpu_ms, stats.model_upload_cpu_ms, stats.frame_setup_cpu_ms,
                    stats.shadow_cpu_ms, stats.forward_opaque_cpu_ms, stats.debug_cpu_ms, stats.composite_cpu_ms,
                    stats.ui_cpu_ms, stats.imgui_cpu_ms, stats.present_cpu_ms, stats.shadow_cascade_count,
                    stats.shadowed_point_light_count, stats.directional_shadow_draws, stats.point_shadow_draws,
                    shadow_mb, ibl_mb, stats.estimated_ibl_bytes > 0, stats.ibl_generated_this_frame,
                    stats.reflection_probe_active, stats.reflection_probe_priority,
                    stats.reflection_probe_camera_influence, stats.reflection_probe_radius,
                    stats.reflection_probe_intensity);
        }

        PbrDemoMode mode_ = PbrDemoMode::FullPbr;
        std::vector<CoreEngine::Node> owned_nodes_{};
        CoreEngine::Node camera_node_{};
        CoreEngine::Node point_light_node_{};
        CoreEngine::Node point_light_marker_node_{};
        CoreEngine::Math::Vec3 camera_position_{0.0f, 4.1f, -11.5f};
        float camera_yaw_ = 0.0f;
        float camera_pitch_ = CoreEngine::Math::Deg2Rad(-15.0f);
        float elapsed_seconds_ = 0.0f;
        float point_light_angle_ = 0.0f;
        float perf_report_accumulator_ = 0.0f;
        std::size_t debug_view_index_ = 0;
    };
} // namespace

int main(int argc, char **argv) {
    const PbrDemoMode mode = ParsePbrDemoMode(argc, argv);
    auto app = std::make_unique<PbrDemoApp>(mode);

    CoreEngine::EngineConfig config;
    config.window_width = 1280;
    config.window_height = 720;
    config.resizable = true;
    config.window_title =
            mode == PbrDemoMode::PbrFeaturesOff ? "CoreEngine PBR Demo (features off)" : "CoreEngine PBR Demo";
    config.render_backend = CoreEngine::RenderBackendType::DiligentD3D11;
    config.enable_imgui = false;
    if (mode == PbrDemoMode::PbrFeaturesOff) {
        ApplyPbrFeaturesOff(config.pbr);
    }

    return CoreEngine::RunEngine(std::move(app), config);
}
