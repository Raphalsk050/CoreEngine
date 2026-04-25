#include "core/render/primitives.h"

#include <array>
#include <cstddef>

namespace CoreEngine::Primitives {
    namespace {
        constexpr std::array<StaticMeshVertex, 24> kCubeVertices{{
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
        }};

        constexpr std::array<uint32_t, 36> kCubeIndices{{
             0,  1,  2,  2,  3,  0,
             4,  5,  6,  6,  7,  4,
             8,  9, 10, 10, 11,  8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20,
        }};

        constexpr std::array<StaticMeshVertex, 4> kPlaneVertices{{
            {{-0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
            {{ 0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{ 0.5f, 0.f,  0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{-0.5f, 0.f,  0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
        }};

        constexpr std::array<uint32_t, 6> kPlaneIndices{{0, 1, 2, 2, 3, 0}};

        constexpr std::array<StaticMeshVertex, 4> kQuadVertices{{
            {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
            {{ 0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
            {{ 0.5f,  0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
            {{-0.5f,  0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
        }};

        constexpr std::array<uint32_t, 6> kQuadIndices{{0, 1, 2, 2, 3, 0}};

        template<typename T, std::size_t Size>
        [[nodiscard]] std::span<const T> AsSpan(const std::array<T, Size> &values) {
            return {values.data(), values.size()};
        }
    }

    std::span<const StaticMeshVertex> CubeVertices() {
        return AsSpan(kCubeVertices);
    }

    std::span<const uint32_t> CubeIndices() {
        return AsSpan(kCubeIndices);
    }

    std::span<const StaticMeshVertex> PlaneVertices() {
        return AsSpan(kPlaneVertices);
    }

    std::span<const uint32_t> PlaneIndices() {
        return AsSpan(kPlaneIndices);
    }

    std::span<const StaticMeshVertex> QuadVertices() {
        return AsSpan(kQuadVertices);
    }

    std::span<const uint32_t> QuadIndices() {
        return AsSpan(kQuadIndices);
    }

    MeshDesc MeshFor(PrimitiveType type) {
        switch (type) {
            case PrimitiveType::Cube:
                return MeshDesc{.vertices = CubeVertices(), .indices = CubeIndices()};
            case PrimitiveType::Plane:
                return MeshDesc{.vertices = PlaneVertices(), .indices = PlaneIndices()};
            case PrimitiveType::Quad:
                return MeshDesc{.vertices = QuadVertices(), .indices = QuadIndices()};
            case PrimitiveType::Sphere:
            case PrimitiveType::Count:
                return {};
        }

        return {};
    }
}
