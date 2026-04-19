#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/render/camera_data.h"

namespace CoreEngine {
    class Camera {
    public:
        Camera &LookAt(const glm::vec3 &position,
                       const glm::vec3 &target,
                       const glm::vec3 &up = {0.f, 1.f, 0.f});

        Camera &Perspective(float fov_y_degrees,
                            float width,
                            float height,
                            float near_z,
                            float far_z);

        Camera &Perspective(float fov_y_degrees,
                            float aspect_ratio,
                            float near_z,
                            float far_z);

        Camera &Orthographic(float left, float right,
                             float bottom, float top,
                             float near_z, float far_z);

        [[nodiscard]] const glm::vec3 &Position() const { return position_; }

        [[nodiscard]] CameraData GetCameraData() const;

    private:
        glm::vec3 position_{0.f, 0.f, -5.f};
        glm::vec3 target_{0.f, 0.f, 0.f};
        glm::vec3 up_{0.f, 1.f, 0.f};
        glm::mat4 projection_{1.f};
    };
}
