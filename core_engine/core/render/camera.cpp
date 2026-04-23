#include "core/render/camera.h"

namespace CoreEngine {
    Camera &Camera::LookAt(const glm::vec3 &position,
                           const glm::vec3 &target,
                           const glm::vec3 &up) {
        position_ = position;
        target_   = target;
        up_       = up;
        return *this;
    }

    Camera &Camera::Perspective(float fov_y_degrees,
                                float width,
                                float height,
                                float near_z,
                                float far_z) {
        return Perspective(fov_y_degrees, width / height, near_z, far_z);
    }

    Camera &Camera::Perspective(float fov_y_degrees,
                                float aspect_ratio,
                                float near_z,
                                float far_z) {
        projection_ = glm::perspectiveLH_ZO(glm::radians(fov_y_degrees),
                                            aspect_ratio, near_z, far_z);
        return *this;
    }

    Camera &Camera::Orthographic(float left, float right,
                                 float bottom, float top,
                                 float near_z, float far_z) {
        projection_ = glm::orthoLH_ZO(left, right, bottom, top, near_z, far_z);
        return *this;
    }

    CameraData Camera::GetCameraData() const {
        CameraData data;
        data.view       = glm::lookAtLH(position_, target_, up_);
        data.projection = projection_;
        return data;
    }
}
