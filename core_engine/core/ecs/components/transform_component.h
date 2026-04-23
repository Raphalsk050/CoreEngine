#pragma once

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include <glm/glm.hpp>

namespace CoreEngine {
    struct TransformComponent {
        glm::vec3 position = glm::vec3(0.0, 0.0, 0.0);
        glm::quat rotation = glm::quat(1.0, 0.0, 0.0, 0.0);
        glm::vec3 scale = glm::vec3(1.0, 1.0, 1.0);

        explicit TransformComponent(glm::vec3 position = glm::vec3(0.0, 0.0, 0.0),
                                    glm::quat rotation = glm::quat(1.0, 0.0, 0.0, 0.0),
                                    glm::vec3 scale = glm::vec3(1.0, 1.0, 1.0)) : position(position),
            rotation(rotation), scale(scale) {
        }
    };
} // namespace CoreEngine
