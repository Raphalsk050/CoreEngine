#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/render/vertex.h"

namespace CoreEngine {
    enum class ModelLoadState {
        Invalid,
        Pending,
        Ready,
        Failed,
    };

    struct ModelLoadDesc {
        std::string path;
        bool triangulate = true;
        bool join_identical_vertices = true;
        bool generate_normals = true;
        bool calculate_tangents = false;
        bool convert_to_left_handed = true;
        bool flip_uvs = false;

        [[nodiscard]] bool IsValid() const {
            return !path.empty();
        }
    };

    struct ModelMeshAsset {
        std::string name;
        std::vector<StaticMeshVertex> vertices;
        std::vector<std::uint32_t> indices;

        [[nodiscard]] bool IsValid() const {
            return !vertices.empty() && !indices.empty();
        }
    };

    struct ModelAsset {
        std::vector<ModelMeshAsset> meshes;

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
