#include "terrain_generator.h"

#include <algorithm>
#include <cmath>

namespace CoreEngineSandbox {
    namespace {

        [[nodiscard]] float Clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }

        [[nodiscard]] float Lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

        [[nodiscard]] float Smooth01(float t) noexcept {
            const float clamped = Clamp01(t);
            return clamped * clamped * (3.0f - 2.0f * clamped);
        }

        [[nodiscard]] float SmoothStep(float edge0, float edge1, float value) noexcept {
            const float width = std::max(edge1 - edge0, 0.0001f);
            return Smooth01((value - edge0) / width);
        }

        [[nodiscard]] int FloorToInt(float value) noexcept {
            const int truncated = static_cast<int>(value);
            return value < static_cast<float>(truncated) ? truncated - 1 : truncated;
        }

        [[nodiscard]] float Random01(int x, int z) noexcept {
            constexpr float InvMax24 = 1.0f / static_cast<float>(0x00ffffffu);
            return static_cast<float>(TerrainNoise::Hash2D(x, z) & 0x00ffffffu) * InvMax24;
        }

        [[nodiscard]] float AbsFloat(float value) noexcept { return value < 0.0f ? -value : value; }

        [[nodiscard]] float EvaluateLayerMask(const TerrainFeatureConfig &layer, float world_x, float world_z,
                                              const TerrainNoise &noise) noexcept {
            if (layer.mask_threshold < 0.0f) {
                return 1.0f;
            }

            const float mask =
                    noise.Value(world_x - layer.offset_x_units * 0.37f + 8192.0f,
                                world_z + layer.offset_z_units * 0.41f - 4096.0f, layer.mask_cell_size_units);
            return SmoothStep(layer.mask_threshold, layer.mask_threshold + std::max(layer.mask_softness, 0.0001f),
                              mask);
        }

        [[nodiscard]] float EvaluateFeature(const TerrainFeatureConfig &layer, float world_x, float world_z,
                                            const TerrainNoise &noise) noexcept {
            const float sample = noise.Fractal(world_x + layer.offset_x_units, world_z + layer.offset_z_units,
                                               layer.noise_cell_size_units, layer.octaves);
            const float centered = sample * 2.0f - 1.0f;

            switch (layer.kind) {
                case TerrainFeatureKind::Plains:    return centered * layer.amplitude_units * 0.5f;
                case TerrainFeatureKind::Hills:     return centered * layer.amplitude_units;
                case TerrainFeatureKind::Mountains: {
                    const float ridge = 1.0f - AbsFloat(centered);
                    return ridge * ridge * layer.amplitude_units;
                }
                case TerrainFeatureKind::Valleys: {
                    const float valley = 1.0f - AbsFloat(centered);
                    return -(valley * valley * layer.amplitude_units);
                }
                case TerrainFeatureKind::Detail: return centered * layer.amplitude_units;
            }

            return 0.0f;
        }

    } // namespace

    std::uint32_t TerrainNoise::Hash2D(int x, int z) noexcept {
        std::uint32_t hash = static_cast<std::uint32_t>(x) * 0x8da6b343u;
        hash ^= static_cast<std::uint32_t>(z) * 0xd8163841u;
        hash ^= hash >> 13u;
        hash *= 0xcb1ab31fu;
        hash ^= hash >> 16u;
        return hash;
    }

    float TerrainNoise::Value(float world_x, float world_z, float cell_size_units) const noexcept {
        const float safe_cell_size = std::max(cell_size_units, 0.0001f);
        const float scaled_x = world_x / safe_cell_size;
        const float scaled_z = world_z / safe_cell_size;
        const int grid_x = FloorToInt(scaled_x);
        const int grid_z = FloorToInt(scaled_z);
        const float local_x = scaled_x - static_cast<float>(grid_x);
        const float local_z = scaled_z - static_cast<float>(grid_z);
        const float sx = Smooth01(local_x);
        const float sz = Smooth01(local_z);

        const float a = Random01(grid_x, grid_z);
        const float b = Random01(grid_x + 1, grid_z);
        const float c = Random01(grid_x, grid_z + 1);
        const float d = Random01(grid_x + 1, grid_z + 1);
        return Lerp(Lerp(a, b, sx), Lerp(c, d, sx), sz);
    }

    float TerrainNoise::Fractal(float world_x, float world_z, float cell_size_units, int octaves) const noexcept {
        const int octave_count = std::clamp(octaves, 1, 6);
        float amplitude = 1.0f;
        float amplitude_sum = 0.0f;
        float frequency_multiplier = 1.0f;
        float value = 0.0f;

        for (int octave = 0; octave < octave_count; ++octave) {
            value += Value(world_x, world_z, cell_size_units / frequency_multiplier) * amplitude;
            amplitude_sum += amplitude;
            amplitude *= 0.5f;
            frequency_multiplier *= 2.0f;
        }

        return amplitude_sum > 0.0f ? value / amplitude_sum : 0.0f;
    }

    bool TerrainLayerStack::AddLayer(const TerrainFeatureConfig &layer) noexcept {
        if (layer_count_ >= layers_.size()) {
            return false;
        }

        layers_[layer_count_] = layer;
        ++layer_count_;
        return true;
    }

    void TerrainLayerStack::Clear() noexcept { layer_count_ = 0; }

    float TerrainLayerStack::Evaluate(float world_x, float world_z, const TerrainNoise &noise) const noexcept {
        float height_delta = 0.0f;
        for (std::size_t i = 0; i < layer_count_; ++i) {
            const TerrainFeatureConfig &layer = layers_[i];
            if (!layer.enabled) {
                continue;
            }

            const float mask = EvaluateLayerMask(layer, world_x, world_z, noise);
            height_delta += EvaluateFeature(layer, world_x, world_z, noise) * mask;
        }

        return height_delta;
    }

    std::size_t TerrainLayerStack::Count() const noexcept { return layer_count_; }

    TerrainGeneratorConfig TerrainGeneratorConfig::MinecraftLike() noexcept {
        TerrainLayerStack layers;
        static_cast<void>(layers.AddLayer(TerrainFeatureConfig{
                .kind = TerrainFeatureKind::Plains,
                .amplitude_units = 3.0f,
                .noise_cell_size_units = 112.0f,
                .mask_threshold = -1.0f,
                .offset_x_units = 0.0f,
                .offset_z_units = 0.0f,
                .octaves = 3,
        }));
        static_cast<void>(layers.AddLayer(TerrainFeatureConfig{
                .kind = TerrainFeatureKind::Hills,
                .amplitude_units = 7.0f,
                .noise_cell_size_units = 36.0f,
                .mask_cell_size_units = 160.0f,
                .mask_threshold = 0.35f,
                .mask_softness = 0.25f,
                .offset_x_units = 1733.0f,
                .offset_z_units = -927.0f,
                .octaves = 3,
        }));
        static_cast<void>(layers.AddLayer(TerrainFeatureConfig{
                .kind = TerrainFeatureKind::Mountains,
                .amplitude_units = 18.0f,
                .noise_cell_size_units = 54.0f,
                .mask_cell_size_units = 230.0f,
                .mask_threshold = 0.62f,
                .mask_softness = 0.18f,
                .offset_x_units = 4973.0f,
                .offset_z_units = 3251.0f,
                .octaves = 4,
        }));
        static_cast<void>(layers.AddLayer(TerrainFeatureConfig{
                .kind = TerrainFeatureKind::Valleys,
                .amplitude_units = 9.0f,
                .noise_cell_size_units = 72.0f,
                .mask_cell_size_units = 190.0f,
                .mask_threshold = 0.56f,
                .mask_softness = 0.16f,
                .offset_x_units = -2119.0f,
                .offset_z_units = 6841.0f,
                .octaves = 2,
        }));
        static_cast<void>(layers.AddLayer(TerrainFeatureConfig{
                .kind = TerrainFeatureKind::Detail,
                .amplitude_units = 1.8f,
                .noise_cell_size_units = 12.0f,
                .mask_threshold = -1.0f,
                .offset_x_units = -619.0f,
                .offset_z_units = 2411.0f,
                .octaves = 2,
        }));

        return TerrainGeneratorConfig{
                .base_height_units = 10.0f,
                .min_height_units = 4.0f,
                .layers = layers,
        };
    }

    TerrainGenerator::TerrainGenerator() noexcept : TerrainGenerator(TerrainGeneratorConfig::MinecraftLike()) {}

    TerrainGenerator::TerrainGenerator(TerrainGeneratorConfig config) noexcept : config_(config) {}

    int TerrainGenerator::HeightAtVoxel(int voxel_world_x, int voxel_world_z, int voxels_per_world_unit,
                                        int world_height_voxels) const noexcept {
        const int safe_voxels_per_unit = std::max(1, voxels_per_world_unit);
        const float inv_voxels_per_unit = 1.0f / static_cast<float>(safe_voxels_per_unit);
        const float world_x = static_cast<float>(voxel_world_x) * inv_voxels_per_unit;
        const float world_z = static_cast<float>(voxel_world_z) * inv_voxels_per_unit;
        const float height_units = config_.base_height_units + config_.layers.Evaluate(world_x, world_z, noise_);

        const int max_height = std::max(0, world_height_voxels - 2);
        const int min_height =
                std::clamp(static_cast<int>(config_.min_height_units * static_cast<float>(safe_voxels_per_unit) + 0.5f),
                           0, max_height);
        const int height_voxels = static_cast<int>(height_units * static_cast<float>(safe_voxels_per_unit) + 0.5f);
        return std::clamp(height_voxels, min_height, max_height);
    }

} // namespace CoreEngineSandbox
