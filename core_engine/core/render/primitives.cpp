#include "core/render/primitives.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace CoreEngine::Primitives {
    namespace {
        constexpr std::array<StaticMeshVertex, 24> kCubeVertices{{
                {{-0.5f, 0.5f, -0.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{0.5f, -0.5f, -0.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, -0.5f, -0.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

                {{0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{-0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{-0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

                {{-0.5f, 0.5f, 0.5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{-0.5f, 0.5f, -0.5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{-0.5f, -0.5f, -0.5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, -0.5f, 0.5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

                {{0.5f, 0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{0.5f, 0.5f, 0.5f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

                {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

                {{-0.5f, -0.5f, -0.5f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{0.5f, -0.5f, -0.5f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{0.5f, -0.5f, 0.5f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, -0.5f, 0.5f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
        }};

        constexpr std::array<uint32_t, 36> kCubeIndices{{
                0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
                12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
        }};

        constexpr std::array<StaticMeshVertex, 4> kPlaneVertices{{
                {{-0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
                {{0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{0.5f, 0.f, 0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, 0.f, 0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
        }};

        constexpr std::array<uint32_t, 6> kPlaneIndices{{0, 1, 2, 2, 3, 0}};

        constexpr std::array<StaticMeshVertex, 4> kQuadVertices{{
                {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
                {{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                {{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
                {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
        }};

        constexpr std::array<uint32_t, 6> kQuadIndices{{0, 1, 2, 2, 3, 0}};

        constexpr std::uint32_t kSphereStackCount = 16;
        constexpr std::uint32_t kSphereSliceCount = 32;
        constexpr float kSphereRadius = 0.5f;

        template<typename T, std::size_t Size>
        [[nodiscard]] std::span<const T> AsSpan(const std::array<T, Size> &values) {
            return {values.data(), values.size()};
        }

        [[nodiscard]] std::vector<StaticMeshVertex> BuildSphereVertices() {
            std::vector<StaticMeshVertex> vertices;
            vertices.reserve((kSphereStackCount + 1u) * (kSphereSliceCount + 1u));

            for (std::uint32_t stack = 0; stack <= kSphereStackCount; ++stack) {
                const float v = static_cast<float>(stack) / static_cast<float>(kSphereStackCount);
                const float phi = Math::Pi * v;
                const float sin_phi = std::sin(phi);
                const float cos_phi = std::cos(phi);

                for (std::uint32_t slice = 0; slice <= kSphereSliceCount; ++slice) {
                    const float u = static_cast<float>(slice) / static_cast<float>(kSphereSliceCount);
                    const float theta = Math::TwoPi * u;
                    const float sin_theta = std::sin(theta);
                    const float cos_theta = std::cos(theta);

                    const Math::Vec3 normal{sin_phi * cos_theta, cos_phi, sin_phi * sin_theta};
                    vertices.push_back(StaticMeshVertex{
                            .position = normal * kSphereRadius,
                            .normal = normal,
                            .color = {1.f, 1.f, 1.f},
                            .uv = {u, v},
                    });
                }
            }

            return vertices;
        }

        [[nodiscard]] std::vector<std::uint32_t> BuildSphereIndices() {
            std::vector<std::uint32_t> indices;
            indices.reserve(kSphereStackCount * kSphereSliceCount * 6u);

            const std::uint32_t stride = kSphereSliceCount + 1u;
            for (std::uint32_t stack = 0; stack < kSphereStackCount; ++stack) {
                for (std::uint32_t slice = 0; slice < kSphereSliceCount; ++slice) {
                    const std::uint32_t upper0 = stack * stride + slice;
                    const std::uint32_t upper1 = upper0 + 1u;
                    const std::uint32_t lower0 = (stack + 1u) * stride + slice;
                    const std::uint32_t lower1 = lower0 + 1u;

                    if (stack == 0u) {
                        indices.insert(indices.end(), {upper0, lower1, lower0});
                    } else if (stack + 1u == kSphereStackCount) {
                        indices.insert(indices.end(), {upper0, upper1, lower0});
                    } else {
                        indices.insert(indices.end(), {upper0, upper1, lower0, upper1, lower1, lower0});
                    }
                }
            }

            return indices;
        }

        [[nodiscard]] const std::vector<StaticMeshVertex> &SphereVertexStorage() {
            static const std::vector<StaticMeshVertex> vertices = BuildSphereVertices();
            return vertices;
        }

        [[nodiscard]] const std::vector<std::uint32_t> &SphereIndexStorage() {
            static const std::vector<std::uint32_t> indices = BuildSphereIndices();
            return indices;
        }
    } // namespace

    std::span<const StaticMeshVertex> CubeVertices() { return AsSpan(kCubeVertices); }

    std::span<const uint32_t> CubeIndices() { return AsSpan(kCubeIndices); }

    std::span<const StaticMeshVertex> PlaneVertices() { return AsSpan(kPlaneVertices); }

    std::span<const uint32_t> PlaneIndices() { return AsSpan(kPlaneIndices); }

    std::span<const StaticMeshVertex> QuadVertices() { return AsSpan(kQuadVertices); }

    std::span<const uint32_t> QuadIndices() { return AsSpan(kQuadIndices); }

    std::span<const StaticMeshVertex> SphereVertices() {
        const std::vector<StaticMeshVertex> &vertices = SphereVertexStorage();
        return {vertices.data(), vertices.size()};
    }

    std::span<const uint32_t> SphereIndices() {
        const std::vector<std::uint32_t> &indices = SphereIndexStorage();
        return {indices.data(), indices.size()};
    }

    MeshDesc MeshFor(PrimitiveType type) {
        switch (type) {
            case PrimitiveType::Cube:   return MeshDesc{.vertices = CubeVertices(), .indices = CubeIndices()};
            case PrimitiveType::Plane:  return MeshDesc{.vertices = PlaneVertices(), .indices = PlaneIndices()};
            case PrimitiveType::Quad:   return MeshDesc{.vertices = QuadVertices(), .indices = QuadIndices()};
            case PrimitiveType::Sphere: return MeshDesc{.vertices = SphereVertices(), .indices = SphereIndices()};
            case PrimitiveType::Count:  return {};
        }

        return {};
    }
} // namespace CoreEngine::Primitives
