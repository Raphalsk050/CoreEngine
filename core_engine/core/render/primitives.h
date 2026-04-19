#pragma once

#include <vector>
#include <cstdint>

#include "core/render/vertex.h"
#include "core/render/primitive_type.h"

namespace CoreEngine::Primitives {
    [[nodiscard]] std::vector<Vertex>   CubeVertices();
    [[nodiscard]] std::vector<uint16_t> CubeIndices();
    [[nodiscard]] std::vector<Vertex>   PlaneVertices();
    [[nodiscard]] std::vector<uint16_t> PlaneIndices();
    [[nodiscard]] std::vector<Vertex>   QuadVertices();
    [[nodiscard]] std::vector<uint16_t> QuadIndices();

    [[nodiscard]] std::vector<Vertex>   VerticesFor(PrimitiveType type);
    [[nodiscard]] std::vector<uint16_t> IndicesFor(PrimitiveType type);
}
