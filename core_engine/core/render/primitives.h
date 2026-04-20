#pragma once

#include <cstdint>
#include <span>

#include "core/render/mesh_desc.h"
#include "core/render/primitive_type.h"

namespace CoreEngine::Primitives {
    [[nodiscard]] std::span<const StaticMeshVertex> CubeVertices();
    [[nodiscard]] std::span<const uint32_t> CubeIndices();
    [[nodiscard]] std::span<const StaticMeshVertex> PlaneVertices();
    [[nodiscard]] std::span<const uint32_t> PlaneIndices();
    [[nodiscard]] std::span<const StaticMeshVertex> QuadVertices();
    [[nodiscard]] std::span<const uint32_t> QuadIndices();

    [[nodiscard]] MeshDesc MeshFor(PrimitiveType type);
}