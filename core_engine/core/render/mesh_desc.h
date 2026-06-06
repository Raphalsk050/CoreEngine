#pragma once

#include <cstdint>
#include <span>

#include "core/render/vertex.h"

namespace CoreEngine {
    enum class MeshUsage { Static };

    enum class IndexFormat { UInt32 };

    struct MeshDesc {
        std::span<const StaticMeshVertex> vertices;
        std::span<const uint32_t> indices;
        MeshUsage usage = MeshUsage::Static;
        IndexFormat index_format = IndexFormat::UInt32;

        [[nodiscard]] bool IsValid() const { return !vertices.empty() && !indices.empty(); }
    };
} // namespace CoreEngine
