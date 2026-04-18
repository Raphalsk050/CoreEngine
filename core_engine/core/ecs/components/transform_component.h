#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include <glm/glm.hpp>
#include <string>

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

        std::string ToString() const {
            std::string position_string = "Position: {" + std::to_string(position.x) +
                                          ", " + std::to_string(position.y) + ", " +
                                          std::to_string(position.z) + "} ";
            std::string rotation_string = "Rotation: {" + std::to_string(rotation.x) +
                                          ", " + std::to_string(rotation.y) + ", " +
                                          std::to_string(rotation.z) + ", " +
                                          std::to_string(rotation.w) + "} ";

            std::string scale_string = "Scale: {" + std::to_string(scale.x) + ", " +
                                       std::to_string(scale.y) + ", " +
                                       std::to_string(scale.z) + "} ";

            std::string output =
                    "\n" + position_string + "\n" + rotation_string + "\n" + scale_string;

            return output;
        }
    };
} // namespace CoreEngine
