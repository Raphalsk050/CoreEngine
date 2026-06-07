#pragma once
#include "core/application/engine_context.h"
#include "core/ecs/node.h"
#include "core/math/math.h"

namespace TopDownGame {
    struct CameraInfo {
        float camera_yaw = 0.0f;
        float camera_pitch = 0.0f;
        float camera_mouse_look_speed_degrees = 0.15f;
        float camera_distance = 10.0f;
        float camera_max_pitch = CoreEngine::Math::Deg2Rad(-55.0f);
        float camera_min_pitch = CoreEngine::Math::Deg2Rad(-55.0f);

        float camera_max_yaw = CoreEngine::Math::Deg2Rad(0.0f);
        float camera_min_yaw = CoreEngine::Math::Deg2Rad(-0.0f);
    };

    class Camera {
    public:
        Camera(const CoreEngine::EngineContext &context);

        void AddLookDelta(float yaw_delta_radians, float pitch_delta_radians) noexcept;

        void SetCameraPosition(CoreEngine::Math::Vec3 new_position) noexcept;

        [[nodiscard]] CoreEngine::Math::Quat GetCameraOrientation() const noexcept;

        [[nodiscard]] const CameraInfo &GetCameraInfo() const noexcept {
            return camera_info_;
        }

    private:
        CameraInfo camera_info_;
        CoreEngine::Node camera_node_;
        CoreEngine::Node camera_root_node_;
    };
} // namespace TopDownGame
