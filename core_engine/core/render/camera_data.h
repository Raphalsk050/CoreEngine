#pragma once

#include <glm/glm.hpp>

namespace CoreEngine {
    struct CameraData {
        glm::mat4 view       = glm::mat4(1.f);
        glm::mat4 projection = glm::mat4(1.f);
    };
}
