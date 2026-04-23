#pragma once

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace CoreEngine {
    struct TransformComponent {
        glm::vec3 position = glm::vec3(0.0, 0.0, 0.0);
        glm::quat rotation = glm::quat(1.0, 0.0, 0.0, 0.0);
        glm::vec3 scale    = glm::vec3(1.0, 1.0, 1.0);

        explicit TransformComponent(glm::vec3 position = glm::vec3(0.0, 0.0, 0.0),
                                    glm::quat rotation = glm::quat(1.0, 0.0, 0.0, 0.0),
                                    glm::vec3 scale    = glm::vec3(1.0, 1.0, 1.0))
            : position(position), rotation(rotation), scale(scale) {}

        [[nodiscard]] glm::mat4 WorldMatrix() const {
            glm::mat4 m = glm::mat4(1.f);
            m           = glm::translate(m, position);
            m           = m * glm::mat4_cast(rotation);
            m           = glm::scale(m, scale);
            return m;
        }
    };
} // namespace CoreEngine
