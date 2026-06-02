#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/i_game_app.h"
#include "core/ecs/world.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/input/input_system.h"
#include "core/math/math.h"
#include "core/render/material.h"
#include "core/render/mesh_desc.h"
#include "core/render/render_system.h"

#include "terrain_generator.h"

// Free-camera tuning kept close to the sample because it is not engine policy.
constexpr float CameraMoveSpeed = 4.0f;
constexpr float CameraSprintMultiplier = 5.0f;
constexpr float CameraKeyLookSpeed = 1.8f;
constexpr float CameraMouseLookSpeed = 0.0025f;
constexpr float CameraMaxPitch = CoreEngine::Math::Deg2Rad(85.0f);

// Stable action ids let the input system store bindings without depending on key names.
constexpr CoreEngine::InputActionId MoveCameraAction = CoreEngine::MakeInputActionId(1);
constexpr CoreEngine::InputActionId LookCameraAction = CoreEngine::MakeInputActionId(2);
constexpr CoreEngine::InputActionId CameraUpAction = CoreEngine::MakeInputActionId(3);
constexpr CoreEngine::InputActionId CameraDownAction = CoreEngine::MakeInputActionId(4);
constexpr CoreEngine::InputActionId CameraSprintAction = CoreEngine::MakeInputActionId(5);

/**
 * @brief Controls voxel and distant terrain density without changing world-space dimensions.
 *
 * Responsibility: describe how many real voxels and distant proxy samples exist
 * per world unit while keeping chunk/world dimensions authored in stable units.
 */
struct VoxelWorldConfig {
    int voxels_per_world_unit = 32;
    int chunk_world_size_xz = 8;
    int world_height_units = 128;
    CoreEngineSandbox::TerrainGeneratorConfig terrain = CoreEngineSandbox::TerrainGeneratorConfig::MinecraftLike();
    int view_radius_chunks = 6;
    int distant_lod_radius_chunks = 48;
    // Higher values make the horizon more detailed but slower to build.
    // Fractional values are allowed: 0.25 means one LOD cell per 4 world units.
    float distant_lod_near_samples_per_world_unit = 2.0f;
    float distant_lod_mid_samples_per_world_unit = 0.75f;
    float distant_lod_far_samples_per_world_unit = 0.25f;
    int distant_lod_near_radius_chunks = 1;
    int distant_lod_mid_radius_chunks = 8;
    int chunk_worker_count = 0;
    int max_chunk_uploads_per_frame = 16;
};

/**
 * @brief Compact block identifier used by chunk storage.
 *
 * Responsibility: represent terrain material at block granularity without
 * carrying render data or gameplay state in the hot chunk array.
 */
enum class BlockId : std::uint16_t {
    Air,
    Grass,
    Dirt,
    Stone,
};

/**
 * @brief Integer coordinate of a chunk in the horizontal world grid.
 *
 * Responsibility: address chunks by X/Z only; vertical data lives inside each
 * chunk because this prototype uses full-height chunks.
 */
struct ChunkCoord {
    int x = 0;
    int z = 0;
};

/**
 * @brief Owns CPU block data plus the ECS/GPU handles for one renderable chunk.
 *
 * Responsibility: keep block storage contiguous and cache-friendly while
 * tracking the scene node and uploaded mesh that must be released on shutdown.
 */
struct Chunk {
    // Height-only terrain storage keeps high voxel resolutions viable for this
    // prototype. Full 3D voxel storage would grow cubically with resolution.
    ChunkCoord coord{};
    std::vector<int> surface_heights{};

    // Scene and GPU handles are non-owning from the C++ type perspective; they
    // must be explicitly destroyed through World/RenderSystem before shutdown.
    CoreEngine::Node node{};
    CoreEngine::MeshHandle mesh{};
    bool build_pending = false;
};

/**
 * @brief Owns generated voxel chunks and their renderable chunk meshes.
 *
 * Responsibility: keep block storage compact, generate deterministic terrain,
 * stream real chunks near the camera, render a cheaper distant proxy mesh, and
 * release uploaded meshes before the render backend shuts down.
 */
class VoxelWorld final {
public:
    void Initialize(CoreEngine::World &world,
                    CoreEngine::RenderSystem &render_system,
                    const CoreEngine::Math::Vec3 &initial_camera_position,
                    VoxelWorldConfig config = {}) {
        config_ = NormalizeConfig(config);
        voxel_size_ = 1.0f / static_cast<float>(config_.voxels_per_world_unit);
        terrain_generator_ = CoreEngineSandbox::TerrainGenerator(config_.terrain);

        // The material is white because block colors are baked into vertices.
        // This keeps the sample simple and avoids one material per block type.
        material_ = CoreEngine::Material::Unlit(CoreEngine::UnlitProps{
            .color = CoreEngine::Math::Vec4{1.0f, 1.0f, 1.0f, 1.0f},
        }).Resolve(render_system);
        if (!material_.IsValid()) {
            // If the backend cannot resolve a material, skip world creation
            // instead of creating nodes that cannot render.
            return;
        }

        const int active_side = config_.view_radius_chunks * 2 + 1;
        chunks_.reserve(static_cast<std::size_t>(active_side * active_side));
        StartChunkWorkers();
        UpdateStreaming(world, render_system, initial_camera_position);
    }

    void UpdateStreaming(CoreEngine::World &world,
                         CoreEngine::RenderSystem &render_system,
                         const CoreEngine::Math::Vec3 &camera_position) {
        if (!material_.IsValid()) {
            return;
        }

        ProcessCompletedChunks(world, render_system);

        const ChunkCoord center = CameraChunkCoord(camera_position);
        if (stream_center_initialized_ && SameChunkCoord(center, stream_center_)) {
            return;
        }

        stream_center_ = center;
        stream_center_initialized_ = true;
        UnloadChunksOutsideActiveArea(center, render_system);
        LoadMissingChunksAround(center);
        RequestDistantLodBuild(center);
        ProcessCompletedChunks(world, render_system);
    }

    void Shutdown(CoreEngine::RenderSystem &render_system) {
        StopChunkWorkers();

        // Destroy scene nodes and GPU resources explicitly while the engine
        // systems that own those handles are still alive.
        for (Chunk &chunk: chunks_) {
            if (chunk.node.IsValid()) {
                chunk.node.Destroy();
            }

            if (chunk.mesh.IsValid()) {
                render_system.DestroyMesh(chunk.mesh);
                chunk.mesh = {};
            }
        }
        DestroyDistantLodResources(render_system);

        chunk_lookup_.clear();
        chunks_.clear();
        material_ = {};
        config_ = {};
        voxel_size_ = 1.0f;
        stream_center_ = {};
        stream_center_initialized_ = false;
        distant_lod_center_ = {};
        distant_lod_center_initialized_ = false;
        distant_lod_build_pending_ = false;
        distant_lod_revision_ = 0;
    }

private:
    static constexpr int MaxVoxelResolution = 32;
    static constexpr int MaxViewRadiusChunks = 12;
    static constexpr int MaxDistantLodRadiusChunks = 96;
    static constexpr float MinDistantLodSamplesPerWorldUnit = 0.0625f;
    static constexpr int MaxChunkWorkerCount = 8;
    static constexpr int MaxChunkUploadsPerFrame = 16;

    enum class TerrainBuildKind : std::uint8_t {
        Chunk,
        DistantLod,
    };

    /**
     * @brief Describes CPU-only terrain work submitted to background workers.
     *
     * Responsibility: carry immutable coordinates across the worker queue
     * without exposing ECS or GPU handles to worker threads.
     */
    struct ChunkBuildRequest {
        TerrainBuildKind kind = TerrainBuildKind::Chunk;
        ChunkCoord coord{};
        std::uint64_t revision = 0;
    };

    /**
     * @brief Owns CPU-generated terrain data ready for main-thread upload.
     *
     * Responsibility: transfer heightmap and mesh buffers from workers to the
     * render thread without sharing mutable terrain storage.
     */
    struct ChunkBuildResult {
        TerrainBuildKind kind = TerrainBuildKind::Chunk;
        ChunkCoord coord{};
        std::uint64_t revision = 0;
        std::vector<int> surface_heights{};
        std::vector<CoreEngine::StaticMeshVertex> vertices{};
        std::vector<std::uint32_t> indices{};
    };

    [[nodiscard]] static VoxelWorldConfig NormalizeConfig(VoxelWorldConfig config) noexcept {
        // Keep the prototype inside sane allocation limits. Resolution 16 is
        // allowed because chunks store a heightmap instead of a full 3D volume.
        config.voxels_per_world_unit = std::clamp(config.voxels_per_world_unit, 1, MaxVoxelResolution);
        config.chunk_world_size_xz = std::clamp(config.chunk_world_size_xz, 1, 64);
        config.world_height_units = std::clamp(config.world_height_units, 16, 256);
        config.view_radius_chunks = std::clamp(config.view_radius_chunks, 0, MaxViewRadiusChunks);
        config.distant_lod_radius_chunks = std::clamp(config.distant_lod_radius_chunks,
                                                      config.view_radius_chunks + 1,
                                                      MaxDistantLodRadiusChunks);
        config.distant_lod_near_samples_per_world_unit =
                std::clamp(config.distant_lod_near_samples_per_world_unit,
                           MinDistantLodSamplesPerWorldUnit,
                           static_cast<float>(config.voxels_per_world_unit));
        config.distant_lod_mid_samples_per_world_unit =
                std::clamp(config.distant_lod_mid_samples_per_world_unit,
                           MinDistantLodSamplesPerWorldUnit,
                           config.distant_lod_near_samples_per_world_unit);
        config.distant_lod_far_samples_per_world_unit =
                std::clamp(config.distant_lod_far_samples_per_world_unit,
                           MinDistantLodSamplesPerWorldUnit,
                           config.distant_lod_mid_samples_per_world_unit);
        const int lod_distance_budget = config.distant_lod_radius_chunks - config.view_radius_chunks;
        config.distant_lod_near_radius_chunks = std::clamp(config.distant_lod_near_radius_chunks,
                                                           0,
                                                           lod_distance_budget);
        config.distant_lod_mid_radius_chunks = std::clamp(config.distant_lod_mid_radius_chunks,
                                                          config.distant_lod_near_radius_chunks,
                                                          lod_distance_budget);
        config.chunk_worker_count = std::clamp(config.chunk_worker_count, 0, MaxChunkWorkerCount);
        config.max_chunk_uploads_per_frame = std::clamp(config.max_chunk_uploads_per_frame,
                                                        1,
                                                        MaxChunkUploadsPerFrame);
        return config;
    }

    [[nodiscard]] static bool SameChunkCoord(ChunkCoord a, ChunkCoord b) noexcept {
        return a.x == b.x && a.z == b.z;
    }

    [[nodiscard]] static int FloorToInt(float value) noexcept {
        const int truncated = static_cast<int>(value);
        return value < static_cast<float>(truncated) ? truncated - 1 : truncated;
    }

    [[nodiscard]] ChunkCoord CameraChunkCoord(const CoreEngine::Math::Vec3 &position) const noexcept {
        const float chunk_size = static_cast<float>(config_.chunk_world_size_xz);
        return ChunkCoord{
            .x = FloorToInt(position.x / chunk_size),
            .z = FloorToInt(position.z / chunk_size),
        };
    }

    [[nodiscard]] bool IsInsideActiveArea(ChunkCoord coord, ChunkCoord center) const noexcept {
        const int radius = config_.view_radius_chunks;
        const int dx = coord.x - center.x;
        const int dz = coord.z - center.z;
        return dx >= -radius && dx <= radius && dz >= -radius && dz <= radius;
    }

    void DestroyChunkResources(Chunk &chunk, CoreEngine::RenderSystem &render_system) {
        if (chunk.node.IsValid()) {
            chunk.node.Destroy();
            chunk.node = {};
        }

        if (chunk.mesh.IsValid()) {
            render_system.DestroyMesh(chunk.mesh);
            chunk.mesh = {};
        }
    }

    void DestroyDistantLodResources(CoreEngine::RenderSystem &render_system) {
        if (distant_lod_node_.IsValid()) {
            distant_lod_node_.Destroy();
            distant_lod_node_ = {};
        }

        if (distant_lod_mesh_.IsValid()) {
            render_system.DestroyMesh(distant_lod_mesh_);
            distant_lod_mesh_ = {};
        }
    }

    void RemoveChunkAtIndex(std::size_t index, CoreEngine::RenderSystem &render_system) {
        Chunk &removed_chunk = chunks_[index];
        chunk_lookup_.erase(PackChunkCoord(removed_chunk.coord));
        DestroyChunkResources(removed_chunk, render_system);

        const std::size_t last_index = chunks_.size() - 1u;
        if (index != last_index) {
            chunks_[index] = std::move(chunks_[last_index]);
            chunk_lookup_[PackChunkCoord(chunks_[index].coord)] = index;
        }

        chunks_.pop_back();
    }

    void UnloadChunksOutsideActiveArea(ChunkCoord center, CoreEngine::RenderSystem &render_system) {
        for (std::size_t i = 0; i < chunks_.size();) {
            if (IsInsideActiveArea(chunks_[i].coord, center)) {
                ++i;
                continue;
            }

            RemoveChunkAtIndex(i, render_system);
        }
    }

    void ProcessCompletedChunks(CoreEngine::World &world, CoreEngine::RenderSystem &render_system) {
        int processed_results = 0;
        while (processed_results < config_.max_chunk_uploads_per_frame) {
            ChunkBuildResult result;
            {
                std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
                if (completed_builds_.empty()) {
                    break;
                }

                result = std::move(completed_builds_.front());
                completed_builds_.pop_front();
            }

            if (result.kind == TerrainBuildKind::DistantLod) {
                ProcessCompletedDistantLod(std::move(result), world, render_system);
                ++processed_results;
                continue;
            }

            const auto chunk_it = chunk_lookup_.find(PackChunkCoord(result.coord));
            if (chunk_it == chunk_lookup_.end()) {
                ++processed_results;
                continue;
            }

            if (stream_center_initialized_ && !IsInsideActiveArea(result.coord, stream_center_)) {
                RemoveChunkAtIndex(chunk_it->second, render_system);
                ++processed_results;
                continue;
            }

            Chunk &chunk = chunks_[chunk_it->second];
            if (!chunk.build_pending && chunk.mesh.IsValid()) {
                ++processed_results;
                continue;
            }

            chunk.surface_heights = std::move(result.surface_heights);
            chunk.build_pending = false;
            DestroyChunkResources(chunk, render_system);
            UploadChunkMesh(chunk, result.vertices, result.indices, world, render_system);
            ++processed_results;
        }
    }

    void ProcessCompletedDistantLod(ChunkBuildResult &&result,
                                    CoreEngine::World &world,
                                    CoreEngine::RenderSystem &render_system) {
        if (result.revision != distant_lod_revision_ ||
            !stream_center_initialized_ ||
            !SameChunkCoord(result.coord, stream_center_)) {
            return;
        }

        distant_lod_build_pending_ = false;
        if (result.vertices.empty() || result.indices.empty()) {
            return;
        }

        CoreEngine::MeshHandle mesh = render_system.CreateMesh(CoreEngine::MeshDesc{
            .vertices = std::span<const CoreEngine::StaticMeshVertex>{result.vertices.data(), result.vertices.size()},
            .indices = std::span<const std::uint32_t>{result.indices.data(), result.indices.size()},
        });
        if (!mesh.IsValid()) {
            return;
        }

        DestroyDistantLodResources(render_system);

        distant_lod_mesh_ = mesh;
        distant_lod_node_ = world.CreateNode(std::format("Distant Terrain LOD {},{}", result.coord.x, result.coord.z));
        distant_lod_node_.SetPosition(DistantLodWorldOrigin(result.coord));
        distant_lod_node_.AddComponent<CoreEngine::MeshRendererComponent>(CoreEngine::MeshRendererComponent{
            .mesh = distant_lod_mesh_,
            .material = material_,
            .visible = true,
            .cast_shadows = false,
            .topology = CoreEngine::PrimitiveTopology::TriangleList,
        });
        distant_lod_center_ = result.coord;
        distant_lod_center_initialized_ = true;
    }

    void RequestChunkBuild(ChunkCoord coord) {
        const std::uint64_t key = PackChunkCoord(coord);
        if (chunk_lookup_.find(key) != chunk_lookup_.end()) {
            return;
        }

        const std::size_t chunk_index = chunks_.size();
        Chunk &chunk = chunks_.emplace_back();
        chunk.coord = coord;
        chunk.build_pending = true;
        chunk_lookup_[key] = chunk_index;

        {
            std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
            pending_builds_.push_back(ChunkBuildRequest{
                .kind = TerrainBuildKind::Chunk,
                .coord = coord,
            });
        }
        chunk_worker_cv_.notify_one();
    }

    void RequestDistantLodBuild(ChunkCoord center) {
        if (distant_lod_build_pending_ &&
            distant_lod_center_initialized_ &&
            SameChunkCoord(center, distant_lod_center_)) {
            return;
        }

        distant_lod_build_pending_ = true;
        distant_lod_center_ = center;
        distant_lod_center_initialized_ = true;
        const std::uint64_t revision = ++distant_lod_revision_;

        {
            std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
            pending_builds_.push_back(ChunkBuildRequest{
                .kind = TerrainBuildKind::DistantLod,
                .coord = center,
                .revision = revision,
            });
        }
        chunk_worker_cv_.notify_one();
    }

    void LoadMissingChunksAround(ChunkCoord center) {
        const int radius = config_.view_radius_chunks;
        for (int z = center.z - radius; z <= center.z + radius; ++z) {
            for (int x = center.x - radius; x <= center.x + radius; ++x) {
                const ChunkCoord coord{.x = x, .z = z};
                RequestChunkBuild(coord);
            }
        }
    }

    void StartChunkWorkers() {
        {
            std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
            chunk_worker_stop_ = false;
        }

        const unsigned int hardware_threads = std::thread::hardware_concurrency();
        const int automatic_workers = hardware_threads > 1u
                                          ? std::clamp(static_cast<int>(hardware_threads) - 1, 1, 4)
                                          : 1;
        const int worker_count = config_.chunk_worker_count > 0
                                     ? config_.chunk_worker_count
                                     : automatic_workers;

        chunk_workers_.reserve(static_cast<std::size_t>(worker_count));
        for (int worker_index = 0; worker_index < worker_count; ++worker_index) {
            chunk_workers_.emplace_back([this] {
                ChunkWorkerLoop();
            });
        }
    }

    void StopChunkWorkers() {
        {
            std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
            chunk_worker_stop_ = true;
            pending_builds_.clear();
        }
        chunk_worker_cv_.notify_all();

        for (std::thread &worker: chunk_workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        chunk_workers_.clear();

        {
            std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
            pending_builds_.clear();
            completed_builds_.clear();
        }
    }

    void ChunkWorkerLoop() {
        while (true) {
            ChunkBuildRequest request;
            {
                std::unique_lock<std::mutex> lock(chunk_worker_mutex_);
                chunk_worker_cv_.wait(lock, [this] {
                    return chunk_worker_stop_ || !pending_builds_.empty();
                });

                if (chunk_worker_stop_) {
                    return;
                }

                request = pending_builds_.front();
                pending_builds_.pop_front();
            }

            ChunkBuildResult result = request.kind == TerrainBuildKind::DistantLod
                                          ? BuildDistantLodData(request.coord, request.revision)
                                          : BuildChunkData(request.coord);

            {
                std::lock_guard<std::mutex> lock(chunk_worker_mutex_);
                if (!chunk_worker_stop_) {
                    completed_builds_.push_back(std::move(result));
                }
            }
        }
    }

    [[nodiscard]] int ChunkSizeX() const noexcept {
        return config_.chunk_world_size_xz * config_.voxels_per_world_unit;
    }

    [[nodiscard]] int ChunkSizeY() const noexcept {
        return config_.world_height_units * config_.voxels_per_world_unit;
    }

    [[nodiscard]] int ChunkSizeZ() const noexcept {
        return config_.chunk_world_size_xz * config_.voxels_per_world_unit;
    }

    [[nodiscard]] std::size_t ColumnCount() const noexcept {
        return static_cast<std::size_t>(ChunkSizeX()) * static_cast<std::size_t>(ChunkSizeZ());
    }

    [[nodiscard]] std::size_t ColumnIndex(int x, int z) const noexcept {
        // X is contiguous so row scans over terrain columns stay cache-friendly.
        return static_cast<std::size_t>(z * ChunkSizeX() + x);
    }

    /**
     * @brief Describes one cube face in local block space.
     *
     * Responsibility: pair a face normal with four corners ordered for the
     * renderer's triangle winding.
     */
    struct FaceDesc {
        CoreEngine::Math::Vec3 normal;
        std::array<CoreEngine::Math::Vec3, 4> corners;
    };

    // Face order must match the neighbor checks in BuildChunkData:
    // +X, -X, +Y, -Y, +Z, -Z.
    static constexpr std::array<FaceDesc, 6> Faces{
        {
            FaceDesc{
                .normal = {1.0f, 0.0f, 0.0f},
                .corners = {
                    {
                        {1.0f, 1.0f, 0.0f},
                        {1.0f, 1.0f, 1.0f},
                        {1.0f, 0.0f, 1.0f},
                        {1.0f, 0.0f, 0.0f},
                    }
                },
            },
            FaceDesc{
                .normal = {-1.0f, 0.0f, 0.0f},
                .corners = {
                    {
                        {0.0f, 1.0f, 1.0f},
                        {0.0f, 1.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f},
                    }
                },
            },
            FaceDesc{
                .normal = {0.0f, 1.0f, 0.0f},
                .corners = {
                    {
                        {0.0f, 1.0f, 1.0f},
                        {1.0f, 1.0f, 1.0f},
                        {1.0f, 1.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f},
                    }
                },
            },
            FaceDesc{
                .normal = {0.0f, -1.0f, 0.0f},
                .corners = {
                    {
                        {0.0f, 0.0f, 0.0f},
                        {1.0f, 0.0f, 0.0f},
                        {1.0f, 0.0f, 1.0f},
                        {0.0f, 0.0f, 1.0f},
                    }
                },
            },
            FaceDesc{
                .normal = {0.0f, 0.0f, 1.0f},
                .corners = {
                    {
                        {1.0f, 1.0f, 1.0f},
                        {0.0f, 1.0f, 1.0f},
                        {0.0f, 0.0f, 1.0f},
                        {1.0f, 0.0f, 1.0f},
                    }
                },
            },
            FaceDesc{
                .normal = {0.0f, 0.0f, -1.0f},
                .corners = {
                    {
                        {0.0f, 1.0f, 0.0f},
                        {1.0f, 1.0f, 0.0f},
                        {1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f},
                    }
                },
            },
        }
    };

    // Every generated quad uses the same UVs. Textures are not sampled yet, but
    // keeping UVs valid makes this mesh compatible with textured materials later.
    static constexpr std::array<CoreEngine::Math::Vec2, 4> FaceUvs{
        {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f},
        }
    };

    [[nodiscard]] static std::uint64_t PackChunkCoord(ChunkCoord coord) noexcept {
        // Reinterpret signed coordinates as fixed-width halves. This preserves a
        // stable key for negative chunks without allocating a pair/hash object.
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32u) |
               static_cast<std::uint32_t>(coord.z);
    }

    [[nodiscard]] int TerrainHeight(int voxel_world_x, int voxel_world_z) const noexcept {
        return terrain_generator_.HeightAtVoxel(voxel_world_x,
                                                voxel_world_z,
                                                config_.voxels_per_world_unit,
                                                ChunkSizeY());
    }

    [[nodiscard]] static bool IsSolid(BlockId block) noexcept {
        // Only solid blocks emit renderable faces and occlude neighbor faces.
        return block != BlockId::Air;
    }

    [[nodiscard]] static CoreEngine::Math::Vec3 BlockColor(BlockId block,
                                                           const CoreEngine::Math::Vec3 &normal) noexcept {
        // Colors are vertex-baked for now. This avoids texture dependencies and
        // makes each chunk a single material draw.
        CoreEngine::Math::Vec3 color{1.0f, 1.0f, 1.0f};
        switch (block) {
            case BlockId::Grass:
                color = normal.y > 0.5f
                            ? CoreEngine::Math::Vec3{0.20f, 0.70f, 0.22f}
                            : CoreEngine::Math::Vec3{0.38f, 0.26f, 0.12f};
                break;
            case BlockId::Dirt:
                color = CoreEngine::Math::Vec3{0.45f, 0.28f, 0.13f};
                break;
            case BlockId::Stone:
                color = CoreEngine::Math::Vec3{0.46f, 0.48f, 0.50f};
                break;
            case BlockId::Air:
                break;
        }

        // Simple face shading gives the block world readable shape without a
        // lighting system: top faces are brighter, bottom faces darker.
        const float shade = normal.y > 0.5f ? 1.0f : (normal.y < -0.5f ? 0.55f : 0.78f);
        return color * shade;
    }

    [[nodiscard]] CoreEngine::Math::Vec3 ChunkWorldOrigin(ChunkCoord coord) const noexcept {
        // Chunk meshes are generated in local chunk coordinates, then moved by
        // the node transform to their world-grid origin.
        return CoreEngine::Math::Vec3{
            static_cast<float>(coord.x * config_.chunk_world_size_xz),
            0.0f,
            static_cast<float>(coord.z * config_.chunk_world_size_xz),
        };
    }

    [[nodiscard]] CoreEngine::Math::Vec3 DistantLodWorldOrigin(ChunkCoord center) const noexcept {
        const int radius = config_.distant_lod_radius_chunks;
        return CoreEngine::Math::Vec3{
            static_cast<float>((center.x - radius) * config_.chunk_world_size_xz),
            0.0f,
            static_cast<float>((center.z - radius) * config_.chunk_world_size_xz),
        };
    }

    [[nodiscard]] int DistantLodStepFromSamplesPerWorldUnit(float samples_per_world_unit) const noexcept {
        // Convert authored world-space LOD density into a stride on the real
        // voxel grid so proxy cells remain aligned with generated chunks.
        const float safe_samples = std::max(samples_per_world_unit, MinDistantLodSamplesPerWorldUnit);
        const float step = static_cast<float>(config_.voxels_per_world_unit) / safe_samples;
        return std::max(1, static_cast<int>(std::ceil(step)));
    }

    [[nodiscard]] int DistantLodStepForChunkDistance(int chunk_distance) const noexcept {
        const int distance_from_real_chunks = std::max(0, chunk_distance - config_.view_radius_chunks);
        if (distance_from_real_chunks <= config_.distant_lod_near_radius_chunks) {
            return DistantLodStepFromSamplesPerWorldUnit(config_.distant_lod_near_samples_per_world_unit);
        }

        if (distance_from_real_chunks <= config_.distant_lod_mid_radius_chunks) {
            return DistantLodStepFromSamplesPerWorldUnit(config_.distant_lod_mid_samples_per_world_unit);
        }

        return DistantLodStepFromSamplesPerWorldUnit(config_.distant_lod_far_samples_per_world_unit);
    }

    [[nodiscard]] std::vector<int> BuildSurfaceHeights(ChunkCoord coord) const {
        // Workers build heightmaps in private storage, so the main chunk array is
        // never read or written from background threads.
        const int chunk_size_x = ChunkSizeX();
        const int chunk_size_z = ChunkSizeZ();
        std::vector<int> surface_heights(ColumnCount(), 0);

        for (int z = 0; z < chunk_size_z; ++z) {
            for (int x = 0; x < chunk_size_x; ++x) {
                const int world_x = coord.x * chunk_size_x + x;
                const int world_z = coord.z * chunk_size_z + z;
                surface_heights[ColumnIndex(x, z)] = TerrainHeight(world_x, world_z);
            }
        }

        return surface_heights;
    }

    [[nodiscard]] int GetGeneratedSurfaceHeight(const std::vector<int> &surface_heights,
                                                int local_x,
                                                int local_z,
                                                int world_x,
                                                int world_z) const {
        const int chunk_size_x = ChunkSizeX();
        const int chunk_size_z = ChunkSizeZ();
        if (local_x >= 0 && local_x < chunk_size_x && local_z >= 0 && local_z < chunk_size_z) {
            return surface_heights[ColumnIndex(local_x, local_z)];
        }

        return TerrainHeight(world_x, world_z);
    }

    [[nodiscard]] BlockId BlockAtHeight(int y, int surface_height) const noexcept {
        if (surface_height < 0 || y > surface_height) {
            return BlockId::Air;
        }

        if (y == surface_height) {
            return BlockId::Grass;
        }

        const int dirt_depth = std::max(1, 3 * config_.voxels_per_world_unit);
        return y >= surface_height - dirt_depth ? BlockId::Dirt : BlockId::Stone;
    }

    void AddFace(std::vector<CoreEngine::StaticMeshVertex> &vertices,
                 std::vector<std::uint32_t> &indices,
                 const CoreEngine::Math::Vec3 &block_position,
                 const FaceDesc &face,
                 BlockId block) const {
        // Each visible face is emitted as an independent quad. Sharing vertices
        // is not useful here because adjacent cube faces need different normals
        // and potentially different shading/UVs.
        const std::uint32_t base_index = static_cast<std::uint32_t>(vertices.size());
        const CoreEngine::Math::Vec3 color = BlockColor(block, face.normal);

        for (std::size_t i = 0; i < face.corners.size(); ++i) {
            vertices.push_back(CoreEngine::StaticMeshVertex{
                .position = (block_position + face.corners[i]) * voxel_size_,
                .normal = face.normal,
                .color = color,
                .uv = FaceUvs[i],
            });
        }

        // Two triangles, matching the face corner order above and the renderer's
        // expected winding.
        indices.push_back(base_index + 0u);
        indices.push_back(base_index + 1u);
        indices.push_back(base_index + 2u);
        indices.push_back(base_index + 2u);
        indices.push_back(base_index + 3u);
        indices.push_back(base_index + 0u);
    }

    static void AddDistantLodQuad(std::vector<CoreEngine::StaticMeshVertex> &vertices,
                                  std::vector<std::uint32_t> &indices,
                                  const CoreEngine::Math::Vec3 &p0,
                                  const CoreEngine::Math::Vec3 &p1,
                                  const CoreEngine::Math::Vec3 &p2,
                                  const CoreEngine::Math::Vec3 &p3,
                                  const CoreEngine::Math::Vec3 &normal,
                                  BlockId block) {
        const std::uint32_t base_index = static_cast<std::uint32_t>(vertices.size());
        const CoreEngine::Math::Vec3 color = BlockColor(block, normal);
        vertices.push_back(CoreEngine::StaticMeshVertex{
            .position = p0,
            .normal = normal,
            .color = color,
        });
        vertices.push_back(CoreEngine::StaticMeshVertex{
            .position = p1,
            .normal = normal,
            .color = color,
        });
        vertices.push_back(CoreEngine::StaticMeshVertex{
            .position = p2,
            .normal = normal,
            .color = color,
        });
        vertices.push_back(CoreEngine::StaticMeshVertex{
            .position = p3,
            .normal = normal,
            .color = color,
        });

        indices.push_back(base_index + 0u);
        indices.push_back(base_index + 1u);
        indices.push_back(base_index + 2u);
        indices.push_back(base_index + 2u);
        indices.push_back(base_index + 3u);
        indices.push_back(base_index + 0u);
    }

    [[nodiscard]] ChunkBuildResult BuildDistantLodData(ChunkCoord center, std::uint64_t revision) const {
        ChunkBuildResult result{
            .kind = TerrainBuildKind::DistantLod,
            .coord = center,
            .revision = revision,
        };

        const int chunk_size_x = ChunkSizeX();
        const int chunk_size_z = ChunkSizeZ();
        const int radius = config_.distant_lod_radius_chunks;
        const int min_voxel_x = (center.x - radius) * chunk_size_x;
        const int min_voxel_z = (center.z - radius) * chunk_size_z;
        const int chunk_side_count = radius * 2 + 1;
        const int near_step_voxels =
                DistantLodStepFromSamplesPerWorldUnit(config_.distant_lod_near_samples_per_world_unit);
        const int mid_step_voxels =
                DistantLodStepFromSamplesPerWorldUnit(config_.distant_lod_mid_samples_per_world_unit);
        const int far_step_voxels =
                DistantLodStepFromSamplesPerWorldUnit(config_.distant_lod_far_samples_per_world_unit);
        const auto cells_per_chunk = [chunk_size_x, chunk_size_z](int step) noexcept {
            const int cells_x = (chunk_size_x + step - 1) / step;
            const int cells_z = (chunk_size_z + step - 1) / step;
            return static_cast<std::size_t>(cells_x * cells_z);
        };
        const auto square_chunk_count = [](int square_radius) noexcept {
            const int side = square_radius * 2 + 1;
            return static_cast<std::size_t>(side * side);
        };
        const int near_radius = std::min(radius,
                                         config_.view_radius_chunks + config_.distant_lod_near_radius_chunks);
        const int mid_radius = std::min(radius,
                                        config_.view_radius_chunks + config_.distant_lod_mid_radius_chunks);
        const std::size_t real_chunk_count = square_chunk_count(config_.view_radius_chunks);
        const std::size_t near_chunk_count = square_chunk_count(near_radius) - real_chunk_count;
        const std::size_t mid_chunk_count = square_chunk_count(mid_radius) - square_chunk_count(near_radius);
        const std::size_t far_chunk_count = static_cast<std::size_t>(chunk_side_count * chunk_side_count) -
                                            square_chunk_count(mid_radius);
        const std::size_t estimated_cells =
                near_chunk_count * cells_per_chunk(near_step_voxels) +
                mid_chunk_count * cells_per_chunk(mid_step_voxels) +
                far_chunk_count * cells_per_chunk(far_step_voxels);
        result.vertices.reserve(estimated_cells * 8u);
        result.indices.reserve(estimated_cells * 12u);

        const auto sample_height = [this](int voxel_x, int voxel_z) noexcept {
            return static_cast<float>(TerrainHeight(voxel_x, voxel_z)) * voxel_size_;
        };

        const auto emit_side = [&](float x0,
                                   float x1,
                                   float z0,
                                   float z1,
                                   float height,
                                   float neighbor_height,
                                   std::size_t face_index) {
            if (neighbor_height >= height) {
                return;
            }

            switch (face_index) {
                case 0:
                    AddDistantLodQuad(result.vertices,
                                      result.indices,
                                      CoreEngine::Math::Vec3{x1, height, z0},
                                      CoreEngine::Math::Vec3{x1, height, z1},
                                      CoreEngine::Math::Vec3{x1, neighbor_height, z1},
                                      CoreEngine::Math::Vec3{x1, neighbor_height, z0},
                                      Faces[0].normal,
                                      BlockId::Dirt);
                    break;
                case 1:
                    AddDistantLodQuad(result.vertices,
                                      result.indices,
                                      CoreEngine::Math::Vec3{x0, height, z1},
                                      CoreEngine::Math::Vec3{x0, height, z0},
                                      CoreEngine::Math::Vec3{x0, neighbor_height, z0},
                                      CoreEngine::Math::Vec3{x0, neighbor_height, z1},
                                      Faces[1].normal,
                                      BlockId::Dirt);
                    break;
                case 4:
                    AddDistantLodQuad(result.vertices,
                                      result.indices,
                                      CoreEngine::Math::Vec3{x1, height, z1},
                                      CoreEngine::Math::Vec3{x0, height, z1},
                                      CoreEngine::Math::Vec3{x0, neighbor_height, z1},
                                      CoreEngine::Math::Vec3{x1, neighbor_height, z1},
                                      Faces[4].normal,
                                      BlockId::Dirt);
                    break;
                case 5:
                    AddDistantLodQuad(result.vertices,
                                      result.indices,
                                      CoreEngine::Math::Vec3{x0, height, z0},
                                      CoreEngine::Math::Vec3{x1, height, z0},
                                      CoreEngine::Math::Vec3{x1, neighbor_height, z0},
                                      CoreEngine::Math::Vec3{x0, neighbor_height, z0},
                                      Faces[5].normal,
                                      BlockId::Dirt);
                    break;
                default:
                    break;
            }
        };

        for (int chunk_z = center.z - radius; chunk_z <= center.z + radius; ++chunk_z) {
            for (int chunk_x = center.x - radius; chunk_x <= center.x + radius; ++chunk_x) {
                const ChunkCoord lod_chunk{.x = chunk_x, .z = chunk_z};
                const int chunk_distance = std::max(chunk_x > center.x ? chunk_x - center.x : center.x - chunk_x,
                                                    chunk_z > center.z ? chunk_z - center.z : center.z - chunk_z);
                // Keep the proxy out of loaded real chunks. Rendering the proxy
                // below real terrain causes visible cracks on slopes.
                if (chunk_distance <= config_.view_radius_chunks) {
                    continue;
                }

                const int sample_step = DistantLodStepForChunkDistance(chunk_distance);
                const int chunk_origin_x = lod_chunk.x * chunk_size_x;
                const int chunk_origin_z = lod_chunk.z * chunk_size_z;

                for (int local_z = 0; local_z < chunk_size_z; local_z += sample_step) {
                    const int cell_size_z = std::min(sample_step, chunk_size_z - local_z);
                    const int voxel_z0 = chunk_origin_z + local_z;
                    const int voxel_z1 = voxel_z0 + cell_size_z;
                    const int sample_z = voxel_z0 + cell_size_z / 2;
                    for (int local_x = 0; local_x < chunk_size_x; local_x += sample_step) {
                        const int cell_size_x = std::min(sample_step, chunk_size_x - local_x);
                        const int voxel_x0 = chunk_origin_x + local_x;
                        const int voxel_x1 = voxel_x0 + cell_size_x;
                        const int sample_x = voxel_x0 + cell_size_x / 2;
                        const float height = sample_height(sample_x, sample_z);
                        const float x0 = static_cast<float>(voxel_x0 - min_voxel_x) * voxel_size_;
                        const float x1 = static_cast<float>(voxel_x1 - min_voxel_x) * voxel_size_;
                        const float z0 = static_cast<float>(voxel_z0 - min_voxel_z) * voxel_size_;
                        const float z1 = static_cast<float>(voxel_z1 - min_voxel_z) * voxel_size_;

                        AddDistantLodQuad(result.vertices,
                                          result.indices,
                                          CoreEngine::Math::Vec3{x0, height, z1},
                                          CoreEngine::Math::Vec3{x1, height, z1},
                                          CoreEngine::Math::Vec3{x1, height, z0},
                                          CoreEngine::Math::Vec3{x0, height, z0},
                                          Faces[2].normal,
                                          BlockId::Grass);

                        emit_side(x0,
                                  x1,
                                  z0,
                                  z1,
                                  height,
                                  sample_height(voxel_x1 + cell_size_x / 2, sample_z),
                                  0);
                        emit_side(x0,
                                  x1,
                                  z0,
                                  z1,
                                  height,
                                  sample_height(voxel_x0 - std::max(1, cell_size_x / 2), sample_z),
                                  1);
                        emit_side(x0,
                                  x1,
                                  z0,
                                  z1,
                                  height,
                                  sample_height(sample_x, voxel_z1 + cell_size_z / 2),
                                  4);
                        emit_side(x0,
                                  x1,
                                  z0,
                                  z1,
                                  height,
                                  sample_height(sample_x, voxel_z0 - std::max(1, cell_size_z / 2)),
                                  5);
                    }
                }
            }
        }

        return result;
    }

    [[nodiscard]] ChunkBuildResult BuildChunkData(ChunkCoord coord) const {
        ChunkBuildResult result{
            .kind = TerrainBuildKind::Chunk,
            .coord = coord,
            .surface_heights = BuildSurfaceHeights(coord),
        };
        const int chunk_size_x = ChunkSizeX();
        const int chunk_size_z = ChunkSizeZ();

        struct SideDesc {
            int dx = 0;
            int dz = 0;
            std::size_t face_index = 0;
        };
        static constexpr std::array<SideDesc, 4> SideFaces{
            {
                SideDesc{.dx = 1, .dz = 0, .face_index = 0},
                SideDesc{.dx = -1, .dz = 0, .face_index = 1},
                SideDesc{.dx = 0, .dz = 1, .face_index = 4},
                SideDesc{.dx = 0, .dz = -1, .face_index = 5},
            }
        };

        // Surface-only meshing is equivalent to culling a solid heightfield, but
        // avoids storing and scanning every hidden voxel under the terrain.
        result.vertices.reserve(static_cast<std::size_t>(chunk_size_x * chunk_size_z * 4));
        result.indices.reserve(static_cast<std::size_t>(chunk_size_x * chunk_size_z * 6));

        for (int z = 0; z < chunk_size_z; ++z) {
            for (int x = 0; x < chunk_size_x; ++x) {
                const int height = result.surface_heights[ColumnIndex(x, z)];
                const int world_x = coord.x * chunk_size_x + x;
                const int world_z = coord.z * chunk_size_z + z;

                AddFace(result.vertices,
                        result.indices,
                        CoreEngine::Math::Vec3{
                            static_cast<float>(x),
                            static_cast<float>(height),
                            static_cast<float>(z),
                        },
                        Faces[2],
                        BlockId::Grass);

                for (const SideDesc &side: SideFaces) {
                    const int neighbor_height = GetGeneratedSurfaceHeight(result.surface_heights,
                                                                          x + side.dx,
                                                                          z + side.dz,
                                                                          world_x + side.dx,
                                                                          world_z + side.dz);
                    if (neighbor_height >= height) {
                        continue;
                    }

                    for (int y = std::max(0, neighbor_height + 1); y <= height; ++y) {
                        AddFace(result.vertices,
                                result.indices,
                                CoreEngine::Math::Vec3{
                                    static_cast<float>(x),
                                    static_cast<float>(y),
                                    static_cast<float>(z),
                                },
                                Faces[side.face_index],
                                BlockAtHeight(y, height));
                    }
                }
            }
        }

        return result;
    }

    void UploadChunkMesh(Chunk &chunk,
                         const std::vector<CoreEngine::StaticMeshVertex> &vertices,
                         const std::vector<std::uint32_t> &indices,
                         CoreEngine::World &world,
                         CoreEngine::RenderSystem &render_system) {
        if (vertices.empty() || indices.empty()) {
            // Empty chunks do not allocate a GPU mesh or an ECS render node.
            return;
        }

        // CreateMesh consumes spans into local vectors; the render system owns
        // the uploaded copy after this call returns.
        chunk.mesh = render_system.CreateMesh(CoreEngine::MeshDesc{
            .vertices = std::span<const CoreEngine::StaticMeshVertex>{vertices.data(), vertices.size()},
            .indices = std::span<const std::uint32_t>{indices.data(), indices.size()},
        });
        if (!chunk.mesh.IsValid()) {
            return;
        }

        // One ECS node per chunk keeps transforms cheap and gives the renderer
        // one MeshRendererComponent per chunk mesh.
        chunk.node = world.CreateNode(std::format("Chunk {},{}", chunk.coord.x, chunk.coord.z));
        chunk.node.SetPosition(ChunkWorldOrigin(chunk.coord));
        chunk.node.AddComponent<CoreEngine::MeshRendererComponent>(CoreEngine::MeshRendererComponent{
            .mesh = chunk.mesh,
            .material = material_,
            .visible = true,
            .cast_shadows = true,
            .topology = CoreEngine::PrimitiveTopology::TriangleList,
        });
    }

    // chunks_ is main-thread owned. Workers communicate only through request and
    // result queues so ECS nodes and GPU resources never cross thread boundaries.
    std::vector<Chunk> chunks_;
    std::unordered_map<std::uint64_t, std::size_t> chunk_lookup_;
    std::deque<ChunkBuildRequest> pending_builds_;
    std::deque<ChunkBuildResult> completed_builds_;
    std::mutex chunk_worker_mutex_;
    std::condition_variable chunk_worker_cv_;
    std::vector<std::thread> chunk_workers_;
    CoreEngine::Node distant_lod_node_{};
    CoreEngine::MeshHandle distant_lod_mesh_{};
    CoreEngine::MaterialHandle material_{};
    VoxelWorldConfig config_{};
    CoreEngineSandbox::TerrainGenerator terrain_generator_{};
    float voxel_size_ = 1.0f;
    ChunkCoord stream_center_{};
    ChunkCoord distant_lod_center_{};
    std::uint64_t distant_lod_revision_ = 0;
    bool stream_center_initialized_ = false;
    bool distant_lod_center_initialized_ = false;
    bool distant_lod_build_pending_ = false;
    bool chunk_worker_stop_ = false;
};

/**
 * @brief Owns the sandbox scene and interactive debug camera.
 *
 * Responsibility: create a voxel terrain prototype and update camera movement
 * from action bindings without leaking sample logic into engine runtime systems.
 */
class SandboxApp final : public CoreEngine::IGameApp {
public:
    void Init(const CoreEngine::EngineContext &context) override {
        BindCameraControls(context.input_system);

        // Start above and behind the origin, looking down at the generated
        // terrain so the first frame has visible geometry.
        camera_position_ = CoreEngine::Math::Vec3{0.0f, 18.0f, -28.0f};
        camera_pitch_ = CoreEngine::Math::Deg2Rad(-25.0f);

        // The renderer selects enabled cameras by priority. This explicit camera
        // avoids relying on the renderer's fallback/default camera path.
        camera_node_ = context.world.CreateNode("Main Camera");
        camera_node_.AddComponent<CoreEngine::CameraComponent>(CoreEngine::CameraComponent{
            .projection_type = CoreEngine::CameraProjectionType::Perspective,
            .aspect_mode = CoreEngine::CameraAspectMode::RenderSurface,
            .fov_y_degrees = 60.0f,
            .near_z = 0.01f,
            .far_z = 2500.0f,
            .priority = 100,
            .enabled = true,
        });
        ApplyCameraTransform();

        // World generation happens after the camera exists so any first-frame
        // renderer query can find both camera and renderable chunks.
        voxel_world_.Initialize(context.world, context.render_system, camera_position_, voxel_config_);
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        // Per-frame sandbox behavior stays here; the engine input and ECS systems
        // remain generic and reusable.
        UpdateCamera(frame);
        voxel_world_.UpdateStreaming(frame.world, frame.render_system, camera_position_);
    }

    void Shutdown(const CoreEngine::EngineContext &context) override {
        // Release chunk meshes before engine render shutdown invalidates the
        // render system backend.
        voxel_world_.Shutdown(context.render_system);
    }

private:
    static void BindCameraControls(CoreEngine::InputSystem &input_system) noexcept {
        // A/D controls local X movement, W/S controls local forward movement.
        static_cast<void>(input_system.BindAxis2D(MoveCameraAction,
                                                  CoreEngine::Key::A,
                                                  CoreEngine::Key::D,
                                                  CoreEngine::Key::S,
                                                  CoreEngine::Key::W));
        // Arrow keys provide keyboard look for machines where mouse capture is
        // not desired while debugging.
        static_cast<void>(input_system.BindAxis2D(LookCameraAction,
                                                  CoreEngine::Key::Left,
                                                  CoreEngine::Key::Right,
                                                  CoreEngine::Key::Down,
                                                  CoreEngine::Key::Up));
        // Q/E are vertical debug-camera movement; LeftShift multiplies speed.
        static_cast<void>(input_system.BindButton(CameraUpAction, CoreEngine::Key::E));
        static_cast<void>(input_system.BindButton(CameraDownAction, CoreEngine::Key::Q));
        static_cast<void>(input_system.BindButton(CameraSprintAction, CoreEngine::Key::LeftShift));
    }

    [[nodiscard]] CoreEngine::Math::Quat CameraOrientation() const noexcept {
        // Yaw is applied around world up, then pitch tilts around the camera's
        // local right axis. The negative X axis matches the engine's forward
        // convention for this camera setup.
        const CoreEngine::Math::Quat yaw =
                CoreEngine::Math::AngleAxis(camera_yaw_, CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f});
        const CoreEngine::Math::Quat pitch =
                CoreEngine::Math::AngleAxis(camera_pitch_, CoreEngine::Math::Vec3{-1.0f, 0.0f, 0.0f});
        return yaw * pitch;
    }

    void ApplyCameraTransform() {
        if (!camera_node_.IsValid()) {
            // The sample can shut down safely even if camera creation failed.
            return;
        }

        camera_node_.SetPosition(camera_position_);
        camera_node_.SetRotation(CameraOrientation());
    }

    void UpdateCamera(const CoreEngine::FrameContext &frame) {
        if (!camera_node_.IsValid()) {
            return;
        }

        // InputAction values hide the concrete key bindings from movement code.
        const CoreEngine::InputVector2 move_axis = frame.input_system.GetAxis2D(MoveCameraAction);
        const CoreEngine::InputVector2 key_look_axis = frame.input_system.GetAxis2D(LookCameraAction);

        // Mouse look is active only while holding right mouse button, matching
        // common editor/debug-camera behavior.
        const CoreEngine::InputVector2 mouse_delta = frame.input_system.
                                                     IsMouseButtonDown(CoreEngine::MouseButton::Right)
                                                         ? frame.input_system.MouseDelta()
                                                         : CoreEngine::InputVector2{};

        // Keyboard look is frame-rate independent; mouse delta is already a
        // per-frame displacement from the input system.
        camera_yaw_ += key_look_axis.x * CameraKeyLookSpeed * frame.delta_time +
                mouse_delta.x * CameraMouseLookSpeed;
        camera_pitch_ += key_look_axis.y * CameraKeyLookSpeed * frame.delta_time -
                mouse_delta.y * CameraMouseLookSpeed;
        camera_pitch_ = std::clamp(camera_pitch_, -CameraMaxPitch, CameraMaxPitch);

        // Vertical movement is world-up instead of camera-up so looking down does
        // not make Q/E move diagonally.
        const float vertical_axis =
                (frame.input_system.IsActionDown(CameraUpAction) ? 1.0f : 0.0f) -
                (frame.input_system.IsActionDown(CameraDownAction) ? 1.0f : 0.0f);

        const float speed = CameraMoveSpeed *
                            (frame.input_system.IsActionDown(CameraSprintAction)
                                 ? CameraSprintMultiplier
                                 : 1.0f);
        const CoreEngine::Math::Quat orientation = CameraOrientation();
        // Build movement basis from the current camera rotation so WASD moves in
        // the direction the camera is facing.
        const CoreEngine::Math::Vec3 right = orientation * CoreEngine::Math::Vec3{1.0f, 0.0f, 0.0f};
        const CoreEngine::Math::Vec3 forward = orientation * CoreEngine::Math::Vec3{0.0f, 0.0f, 1.0f};
        const CoreEngine::Math::Vec3 up{0.0f, 1.0f, 0.0f};

        const CoreEngine::Math::Vec3 velocity =
                (right * move_axis.x) + (forward * move_axis.y) + (up * vertical_axis);
        camera_position_ += velocity * (speed * frame.delta_time);

        ApplyCameraTransform();
    }

    // Sandbox-owned systems and camera state. These are intentionally kept out
    // of CoreEngine runtime modules because they are example/game-specific.
    // Edit VoxelWorldConfig for resolution/streaming and
    // TerrainGeneratorConfig::MinecraftLike for additive terrain layers.
    VoxelWorldConfig voxel_config_{};
    VoxelWorld voxel_world_{};
    CoreEngine::Node camera_node_{};
    CoreEngine::Math::Vec3 camera_position_{0.0f, 18.0f, -28.0f};
    float camera_yaw_ = 0.0f;
    float camera_pitch_ = CoreEngine::Math::Deg2Rad(-25.0f);
};

int main() {
    // The sample app owns scene setup and per-frame behavior; RunEngine owns the
    // platform window, render loop, input system, and engine shutdown order.
    auto app = std::make_unique<SandboxApp>();

    CoreEngine::EngineConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.resizable = true;
    config.windowTitle = "CoreEngine Voxel Sandbox";

    // A real backend is required. The null backend accepts the engine loop but
    // cannot upload meshes, which would make the sample render nothing.
    config.renderBackend = CoreEngine::RenderBackendType::DiligentD3D11;

    // The prototype does not use editor UI yet, so ImGui stays disabled.
    config.enableImGui = false;

    return CoreEngine::RunEngine(std::move(app), config);
}
