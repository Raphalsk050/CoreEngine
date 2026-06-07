#include "camera.h"

#include <algorithm>

#include "core/ecs/components/camera_component.h"
#include "core/ecs/world.h"

namespace TopDownGame {

    Camera::Camera(const CoreEngine::EngineContext &context) {
        camera_node_ = context.world.CreateNode("Camera Node");
        camera_root_node_ = context.world.CreateNode("Camera Root Node");
        float horizontal_axis = std::cos(45.0f) * camera_info_.camera_distance;
        float vertical_axis = std::sin(45.0f) * camera_info_.camera_distance;
        camera_node_.SetPosition(CoreEngine::Math::Vec3{0.0f, vertical_axis, -horizontal_axis});
        camera_node_.SetRotation(CoreEngine::Math::AngleAxis(-45.0f, CoreEngine::Math::Vec3(1.0f, 0.0f, 0.0f)));
        camera_node_.SetParent(camera_root_node_, true);
        camera_node_.AddComponent<CoreEngine::CameraComponent>(CoreEngine::CameraComponent{});
    }

    void Camera::AddLookDelta(float yaw_delta_radians, float pitch_delta_radians) noexcept {
        camera_info_.camera_yaw += yaw_delta_radians;
        camera_info_.camera_pitch += pitch_delta_radians;

        camera_info_.camera_pitch =
                std::clamp(camera_info_.camera_pitch, camera_info_.camera_min_pitch, camera_info_.camera_max_pitch);
        
        camera_info_.camera_yaw =
                std::clamp(camera_info_.camera_yaw, camera_info_.camera_min_yaw, camera_info_.camera_max_yaw);

        if (!camera_node_.IsValid()) {
            return;
        }

        camera_node_.SetRotation(GetCameraOrientation());
    }

    void Camera::SetCameraPosition(CoreEngine::Math::Vec3 new_position) noexcept {
        camera_root_node_.SetPosition(new_position);
    }

    CoreEngine::Math::Quat Camera::GetCameraOrientation() const noexcept {
        const CoreEngine::Math::Quat yaw =
                CoreEngine::Math::AngleAxis(camera_info_.camera_yaw, CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f});
        const CoreEngine::Math::Quat pitch =
                CoreEngine::Math::AngleAxis(camera_info_.camera_pitch, CoreEngine::Math::Vec3{-1.0f, 0.0f, 0.0f});
        return yaw * pitch;
    }
} // namespace TopDownGame
