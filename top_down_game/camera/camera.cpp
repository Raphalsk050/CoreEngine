#include "camera.h"

#include "core/ecs/world.h"

namespace TopDownGame {

    Camera::Camera(const CoreEngine::EngineContext &context) { camera_node_ = context.world.CreateNode("Camera Node"); }
    void Camera::AddLookDelta(float yaw_delta_radians, float pitch_delta_radians) noexcept {
        camera_info_.camera_yaw += yaw_delta_radians;
        camera_info_.camera_pitch += pitch_delta_radians;

        camera_info_.camera_pitch =
                std::clamp(camera_info_.camera_pitch, camera_info_.camera_min_pitch, camera_info_.camera_max_pitch);

        if (!camera_node_.IsValid())
            return;

        camera_node_.SetRotation(GetCameraOrientation());
    }
    CoreEngine::Math::Quat Camera::GetCameraOrientation() const noexcept {
        const CoreEngine::Math::Quat yaw =
                CoreEngine::Math::AngleAxis(camera_info_.camera_yaw, CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f});
        const CoreEngine::Math::Quat pitch =
                CoreEngine::Math::AngleAxis(camera_info_.camera_pitch, CoreEngine::Math::Vec3{-1.0f, 0.0f, 0.0f});
        return yaw * pitch;
    }
} // namespace TopDownGame
