#include "core/render/camera.h"

#include <cmath>

namespace CoreEngine {
    Camera &Camera::LookAt(const Math::Vec3 &position, const Math::Vec3 &target, const Math::Vec3 &up) {
        position_ = position;
        target_ = target;
        up_ = up;
        return *this;
    }

    Camera &Camera::Perspective(float fov_y_degrees, float width, float height, float near_z, float far_z) {
        constexpr float kDimensionEpsilon = 1.0e-5f;
        const bool valid_dimensions = std::isfinite(width) && std::isfinite(height) &&
                                      std::fabs(width) > kDimensionEpsilon && std::fabs(height) > kDimensionEpsilon;
        const float aspect_ratio = valid_dimensions ? width / height : 1.0f;
        return Perspective(fov_y_degrees, aspect_ratio, near_z, far_z);
    }

    Camera &Camera::Perspective(float fov_y_degrees, float aspect_ratio, float near_z, float far_z) {
        projection_ = Math::PerspectiveLH(Math::Deg2Rad(fov_y_degrees), aspect_ratio, near_z, far_z);
        return *this;
    }

    Camera &Camera::Orthographic(float left, float right, float bottom, float top, float near_z, float far_z) {
        projection_ = Math::OrthoLH(left, right, bottom, top, near_z, far_z);
        return *this;
    }

    CameraData Camera::GetCameraData() const {
        CameraData data;
        data.view = Math::LookAtLH(position_, target_, up_);
        data.projection = projection_;
        return data;
    }
} // namespace CoreEngine
