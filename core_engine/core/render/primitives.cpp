#include "core/render/primitives.h"

namespace CoreEngine::Primitives {
    std::vector<Vertex> CubeVertices() {
        return {
            {{-0.5f,  0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{ 0.5f,  0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{ 0.5f, -0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{-0.5f, -0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

            {{ 0.5f,  0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{-0.5f,  0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{-0.5f, -0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{ 0.5f, -0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

            {{-0.5f,  0.5f,  0.5f}, {-1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{-0.5f,  0.5f, -0.5f}, {-1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{-0.5f, -0.5f, -0.5f}, {-1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{-0.5f, -0.5f,  0.5f}, {-1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

            {{ 0.5f,  0.5f, -0.5f}, { 1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{ 0.5f,  0.5f,  0.5f}, { 1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{ 0.5f, -0.5f,  0.5f}, { 1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{ 0.5f, -0.5f, -0.5f}, { 1.f,  0.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

            {{-0.5f,  0.5f,  0.5f}, { 0.f,  1.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{ 0.5f,  0.5f,  0.5f}, { 0.f,  1.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{ 0.5f,  0.5f, -0.5f}, { 0.f,  1.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{-0.5f,  0.5f, -0.5f}, { 0.f,  1.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

            {{-0.5f, -0.5f, -0.5f}, { 0.f, -1.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{ 0.5f, -0.5f, -0.5f}, { 0.f, -1.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{ 0.5f, -0.5f,  0.5f}, { 0.f, -1.f,  0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{-0.5f, -0.5f,  0.5f}, { 0.f, -1.f,  0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
        };
    }

    std::vector<uint16_t> CubeIndices() {
        return {
             0,  1,  2,  2,  3,  0,
             4,  5,  6,  6,  7,  4,
             8,  9, 10, 10, 11,  8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20,
        };
    }

    std::vector<Vertex> PlaneVertices() {
        return {
            {{-0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{ 0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{ 0.5f, 0.f,  0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{-0.5f, 0.f,  0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
        };
    }

    std::vector<uint16_t> PlaneIndices() {
        return {0, 1, 2, 2, 3, 0};
    }

    std::vector<Vertex> QuadVertices() {
        return {
            {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
            {{ 0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{ 0.5f,  0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{-0.5f,  0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
        };
    }

    std::vector<uint16_t> QuadIndices() {
        return {0, 1, 2, 2, 3, 0};
    }

    std::vector<Vertex> VerticesFor(PrimitiveType type) {
        switch (type) {
            case PrimitiveType::Cube:   return CubeVertices();
            case PrimitiveType::Plane:  return PlaneVertices();
            case PrimitiveType::Quad:   return QuadVertices();
            case PrimitiveType::Sphere: return CubeVertices();
        }
        return {};
    }

    std::vector<uint16_t> IndicesFor(PrimitiveType type) {
        switch (type) {
            case PrimitiveType::Cube:   return CubeIndices();
            case PrimitiveType::Plane:  return PlaneIndices();
            case PrimitiveType::Quad:   return QuadIndices();
            case PrimitiveType::Sphere: return CubeIndices();
        }
        return {};
    }
}
