#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "core/math/math.h"
#include "core/render/vertex.h"

namespace CoreEngine {
    inline constexpr std::uint32_t kInvalidModelNodeIndex = std::numeric_limits<std::uint32_t>::max();

    enum class ModelLoadState {
        Invalid,
        Pending,
        Ready,
        Failed,
    };

    enum class ModelMergeMode : std::uint8_t {
        None,
        All,
        ByMaterial,
    };

    enum class ModelTextureSemantic : std::uint8_t {
        BaseColor,
        Normal,
        Metallic,
        Roughness,
        MetallicRoughness,
        Emissive,
        Occlusion,
    };

    struct ModelLoadDesc {
        std::string path;
        bool triangulate = true;
        bool join_identical_vertices = true;
        bool generate_normals = true;
        bool calculate_tangents = false;
        bool convert_to_left_handed = true;
        bool flip_uvs = false;
        bool merge_submeshes = false;
        ModelMergeMode merge_mode = ModelMergeMode::None;
        bool load_materials = true;

        [[nodiscard]] bool IsValid() const {
            return !path.empty();
        }
    };

    struct ModelTextureAsset {
        ModelTextureSemantic semantic = ModelTextureSemantic::BaseColor;
        std::string path;
        std::vector<unsigned char> data;
        bool srgb = true;

        [[nodiscard]] bool IsValid() const {
            return !path.empty() || !data.empty();
        }
    };

    struct ModelMaterialAsset {
        std::string name;
        Math::Vec4 base_color{1.f, 1.f, 1.f, 1.f};
        float metallic = 0.f;
        float roughness = 1.f;
        std::vector<ModelTextureAsset> textures;
    };

    struct ModelMeshAsset {
        std::string name;
        std::uint32_t material_index = 0;
        std::vector<StaticMeshVertex> vertices;
        std::vector<std::uint32_t> indices;

        [[nodiscard]] bool IsValid() const {
            return !vertices.empty() && !indices.empty();
        }
    };

    struct ModelNodeAsset {
        std::string name;
        std::uint32_t parent_index = kInvalidModelNodeIndex;
        Math::Mat4 local_transform{1.f};
        std::vector<std::uint32_t> mesh_indices;

        [[nodiscard]] bool IsRoot() const {
            return parent_index == kInvalidModelNodeIndex;
        }
    };

    struct ModelAsset {
        std::vector<ModelMeshAsset> meshes;
        std::vector<ModelMaterialAsset> materials;
        std::vector<ModelNodeAsset> nodes;

        [[nodiscard]] bool IsValid() const {
            return !meshes.empty();
        }
    };

    struct ModelLoadResult {
        ModelAsset asset;
        std::string error_message;

        [[nodiscard]] bool IsSuccess() const {
            return error_message.empty() && asset.IsValid();
        }
    };
} // namespace CoreEngine
