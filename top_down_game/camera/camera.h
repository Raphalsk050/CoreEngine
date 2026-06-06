#pragma once
#include "core/application/engine_context.h"
#include "core/ecs/node.h"
#include "core/math/math.h"

namespace TopDownGame {
    struct CameraInfo {
        float camera_yaw = 0.0f;
        float camera_pitch = 0.0f;
        float camera_mouse_look_speed = 2.0f;
        float camera_max_pitch = 80.0f;
        float camera_min_pitch = -80.0f;
    };

    class Camera {
    public:
        Camera(const CoreEngine::EngineContext &context);

        void AddLookDelta(float yaw_delta_radians, float pitch_delta_radians) noexcept;

        [[nodiscard]] CoreEngine::Math::Quat GetCameraOrientation() const noexcept;

        [[nodiscard]] const CameraInfo &GetCameraInfo() const noexcept { return camera_info_; }

    private:
        CameraInfo camera_info_;
        CoreEngine::Node camera_node_;
    };
} // namespace TopDownGame
