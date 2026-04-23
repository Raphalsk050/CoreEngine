#pragma once

#include "core/math/math.h"
#include "core/render/camera_data.h"

namespace CoreEngine {
    class Camera {
    public:
        Camera &LookAt(const Math::Vec3 &position,
                       const Math::Vec3 &target,
                       const Math::Vec3 &up = {0.f, 1.f, 0.f});

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

        [[nodiscard]] const Math::Vec3 &Position() const { return position_; }

        [[nodiscard]] CameraData GetCameraData() const;

    private:
        Math::Vec3 position_{0.f, 0.f, -5.f};
        Math::Vec3 target_{0.f, 0.f, 0.f};
        Math::Vec3 up_{0.f, 1.f, 0.f};
        Math::Mat4 projection_{1.f};
    };
}
