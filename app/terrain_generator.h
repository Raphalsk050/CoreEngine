#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoreEngineSandbox {

    enum class TerrainFeatureKind : std::uint8_t {
        Plains,
        Hills,
        Mountains,
        Valleys,
        Detail,
    };

    /**
     * @brief Describes one additive terrain feature layer.
     *
     * Responsibility: keep terrain feature parameters compact and data-only so the
     * generator can evaluate many samples without polymorphic dispatch or heap work.
     */
    struct TerrainFeatureConfig {
        TerrainFeatureKind kind = TerrainFeatureKind::Hills;
        float amplitude_units = 1.0f;
        float noise_cell_size_units = 32.0f;
        float mask_cell_size_units = 128.0f;
        float mask_threshold = -1.0f;
        float mask_softness = 0.2f;
        float offset_x_units = 0.0f;
        float offset_z_units = 0.0f;
        int octaves = 1;
        bool enabled = true;
    };

    /**
     * @brief Provides deterministic value noise for terrain generation.
     *
     * Responsibility: produce stable, allocation-free scalar noise from world-space
     * coordinates so worker threads can generate terrain without shared state.
     */
    class TerrainNoise final {
    public:
        [[nodiscard]] static std::uint32_t Hash2D(int x, int z) noexcept;
        [[nodiscard]] float Value(float world_x, float world_z, float cell_size_units) const noexcept;
        [[nodiscard]] float Fractal(float world_x, float world_z, float cell_size_units, int octaves) const noexcept;
    };

    /**
     * @brief Owns the ordered additive terrain feature stack.
     *
     * Responsibility: evaluate a small fixed list of terrain features in cache-
     * friendly storage, keeping terrain composition extensible without allocations.
     */
    class TerrainLayerStack final {
    public:
        static constexpr std::size_t MaxLayers = 12;

        [[nodiscard]] bool AddLayer(const TerrainFeatureConfig &layer) noexcept;
        void Clear() noexcept;

        [[nodiscard]] float Evaluate(float world_x, float world_z, const TerrainNoise &noise) const noexcept;
        [[nodiscard]] std::size_t Count() const noexcept;

    private:
        std::array<TerrainFeatureConfig, MaxLayers> layers_{};
        std::size_t layer_count_ = 0;
    };

    /**
     * @brief Stores high-level terrain generation knobs for the voxel prototype.
     *
     * Responsibility: define base elevation and the additive feature stack while
     * leaving chunk size, voxel density, and rendering policy to VoxelWorldConfig.
     */
    struct TerrainGeneratorConfig {
        float base_height_units = 8.0f;
        float min_height_units = 4.0f;
        TerrainLayerStack layers{};

        [[nodiscard]] static TerrainGeneratorConfig MinecraftLike() noexcept;
    };

    /**
     * @brief Converts world-space terrain layers into voxel-space surface heights.
     *
     * Responsibility: expose a deterministic height query used by chunk and distant
     * LOD builders without depending on ECS, rendering, or streaming state.
     */
    class TerrainGenerator final {
    public:
        TerrainGenerator() noexcept;
        explicit TerrainGenerator(TerrainGeneratorConfig config) noexcept;

        [[nodiscard]] int HeightAtVoxel(int voxel_world_x, int voxel_world_z, int voxels_per_world_unit,
                                        int world_height_voxels) const noexcept;

    private:
        TerrainGeneratorConfig config_{};
        TerrainNoise noise_{};
    };

} // namespace CoreEngineSandbox
