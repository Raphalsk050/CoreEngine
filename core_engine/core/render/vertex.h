#pragma once

#include <glm/glm.hpp>

namespace CoreEngine {
    struct StaticMeshVertex {
        glm::vec3 position{0.f, 0.f, 0.f};
        glm::vec3 normal{0.f, 1.f, 0.f};
        glm::vec3 color{1.f, 1.f, 1.f};
        glm::vec2 uv{0.f, 0.f};
    };

    using Vertex = StaticMeshVertex;
}