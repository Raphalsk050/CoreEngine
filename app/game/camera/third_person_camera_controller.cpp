#include "third_person_camera_controller.h"

#include <algorithm>
#include <cmath>

#include "core/ecs/components/spring_arm_component.h"
#include "core/ecs/world.h"

namespace Game {
    namespace {
        constexpr float kDirectionEpsilon = 1.0e-6f;

        [[nodiscard]] CoreEngine::Math::Vec3 BuildOrbitForward(
            const CoreEngine::SpringArmComponent &arm) noexcept {
            const float cos_pitch = std::cos(arm.orbit_pitch_radians);
            return CoreEngine::Math::Normalize(CoreEngine::Math::Vec3{
                cos_pitch * std::sin(arm.orbit_yaw_radians),
                std::sin(arm.orbit_pitch_radians),
                cos_pitch * std::cos(arm.orbit_yaw_radians),
            });
        }

        [[nodiscard]] float FollowAlpha(float follow_speed, float delta_seconds) noexcept {
            if (follow_speed <= 0.0f || delta_seconds <= 0.0f) {
                return 1.0f;
            }

            return 1.0f - std::exp(-follow_speed * delta_seconds);
        }
    } // namespace

    bool ThirdPersonCameraController::Attach(CoreEngine::Node camera_node,
                                             CoreEngine::Node target_node) noexcept {
        if (!camera_node.IsValid() || !target_node.IsValid()) {
            Detach();
            return false;
        }

        camera_node_ = camera_node;
        target_node_ = target_node;
        if (camera_node_.TryGetComponent<CoreEngine::SpringArmComponent>() == nullptr) {
            camera_node_.AddComponent<CoreEngine::SpringArmComponent>();
        }

        return true;
    }

    void ThirdPersonCameraController::Detach() noexcept {
        camera_node_ = {};
        target_node_ = {};
    }

    void ThirdPersonCameraController::ApplyLookDeltaRadians(
        const CoreEngine::Math::Vec2 &look_delta) noexcept {
        CoreEngine::SpringArmComponent *arm = Arm();
        if (arm == nullptr) {
            return;
        }

        arm->orbit_yaw_radians += look_delta.x;
        arm->orbit_pitch_radians = CoreEngine::Math::Clamp(
            arm->orbit_pitch_radians - look_delta.y,
            CoreEngine::Math::Deg2Rad(-80.0f),
            CoreEngine::Math::Deg2Rad(80.0f));
    }

    void ThirdPersonCameraController::AdjustDistance(float distance_delta) noexcept {
        CoreEngine::SpringArmComponent *arm = Arm();
        if (arm == nullptr) {
            return;
        }

        arm->rest_length = CoreEngine::Math::Clamp(
            arm->rest_length + distance_delta,
            min_distance_,
            max_distance_);
    }

    void ThirdPersonCameraController::SetDistanceLimits(float min_distance,
                                                        float max_distance) noexcept {
        min_distance_ = std::max(0.0f, min_distance);
        max_distance_ = std::max(min_distance_, max_distance);

        CoreEngine::SpringArmComponent *arm = Arm();
        if (arm != nullptr) {
            arm->rest_length = CoreEngine::Math::Clamp(arm->rest_length, min_distance_, max_distance_);
        }
    }

    void ThirdPersonCameraController::Update(float delta_seconds) noexcept {
        CoreEngine::SpringArmComponent *arm = Arm();
        if (arm == nullptr || !camera_node_.IsValid() || !target_node_.IsValid()) {
            return;
        }

        const float resolved_length = std::max(0.0f, arm->rest_length);
        const CoreEngine::Math::Vec3 pivot = PivotPosition(*arm);
        const CoreEngine::Math::Vec3 desired_position = pivot - Forward() * resolved_length;
        const CoreEngine::Math::Quat desired_rotation = LookRotationLH(
            pivot - desired_position,
            {0.0f, 1.0f, 0.0f});
        const CoreEngine::Math::Vec3 desired_camera_position =
            desired_position + desired_rotation * arm->camera_offset_local;

        CoreEngine::Math::Vec3 final_position = desired_camera_position;
        CoreEngine::Math::Quat final_rotation = desired_rotation;
        if (arm->runtime.initialized) {
            if (arm->smoothing.position_enabled) {
                final_position = CoreEngine::Math::Lerp(
                    arm->runtime.smoothed_camera_position,
                    desired_camera_position,
                    FollowAlpha(arm->smoothing.position_follow_speed, delta_seconds));
            }
            if (arm->smoothing.rotation_enabled) {
                final_rotation = CoreEngine::Math::Slerp(
                    arm->runtime.smoothed_camera_rotation,
                    desired_rotation,
                    FollowAlpha(arm->smoothing.rotation_follow_speed, delta_seconds));
            }
        }

        arm->runtime.desired_camera_position = desired_camera_position;
        arm->runtime.smoothed_camera_position = final_position;
        arm->runtime.smoothed_camera_rotation = final_rotation;
        arm->runtime.resolved_length = resolved_length;
        arm->runtime.obstruction_applied = false;
        arm->runtime.initialized = true;

        camera_node_.SetPosition(final_position);
        camera_node_.SetRotation(final_rotation);
    }

    CoreEngine::Math::Vec3 ThirdPersonCameraController::PlanarForward() const noexcept {
        CoreEngine::Math::Vec3 forward = Forward();
        forward.y = 0.0f;

        if (CoreEngine::Math::LengthSquared(forward) <= kDirectionEpsilon) {
            return {0.0f, 0.0f, 1.0f};
        }

        return CoreEngine::Math::Normalize(forward);
    }

    CoreEngine::Math::Vec3 ThirdPersonCameraController::PlanarRight() const noexcept {
        return CoreEngine::Math::Normalize(
            CoreEngine::Math::Cross({0.0f, 1.0f, 0.0f}, PlanarForward()));
    }

    float ThirdPersonCameraController::YawRadians() const noexcept {
        const CoreEngine::SpringArmComponent *arm = Arm();
        return arm != nullptr ? arm->orbit_yaw_radians : 0.0f;
    }

    float ThirdPersonCameraController::PitchRadians() const noexcept {
        const CoreEngine::SpringArmComponent *arm = Arm();
        return arm != nullptr ? arm->orbit_pitch_radians : 0.0f;
    }

    CoreEngine::SpringArmComponent *ThirdPersonCameraController::Arm() noexcept {
        return camera_node_.IsValid() ? camera_node_.TryGetComponent<CoreEngine::SpringArmComponent>() : nullptr;
    }

    const CoreEngine::SpringArmComponent *ThirdPersonCameraController::Arm() const noexcept {
        return camera_node_.IsValid() ? camera_node_.TryGetComponent<CoreEngine::SpringArmComponent>() : nullptr;
    }

    CoreEngine::Math::Vec3 ThirdPersonCameraController::Forward() const noexcept {
        const CoreEngine::SpringArmComponent *arm = Arm();
        if (arm == nullptr) {
            return {0.0f, 0.0f, 1.0f};
        }

        CoreEngine::Math::Vec3 forward = BuildOrbitForward(*arm);
        if (arm->orientation_mode == CoreEngine::SpringArmOrientationMode::OwnerRotation &&
            target_node_.IsValid()) {
            forward = target_node_.GetWorldRotation() * forward;
        }

        if (CoreEngine::Math::LengthSquared(forward) <= kDirectionEpsilon) {
            return {0.0f, 0.0f, 1.0f};
        }

        return CoreEngine::Math::Normalize(forward);
    }

    CoreEngine::Math::Vec3 ThirdPersonCameraController::PivotPosition(
        const CoreEngine::SpringArmComponent &arm) const noexcept {
        if (!target_node_.IsValid()) {
            return arm.pivot_offset_local;
        }

        return target_node_.GetWorldPosition() + target_node_.GetWorldRotation() * arm.pivot_offset_local;
    }

    CoreEngine::Math::Quat ThirdPersonCameraController::LookRotationLH(
        const CoreEngine::Math::Vec3 &forward,
        const CoreEngine::Math::Vec3 &up) noexcept {
        CoreEngine::Math::Vec3 z = CoreEngine::Math::LengthSquared(forward) > kDirectionEpsilon
                                       ? CoreEngine::Math::Normalize(forward)
                                       : CoreEngine::Math::Vec3{0.0f, 0.0f, 1.0f};
        CoreEngine::Math::Vec3 safe_up = CoreEngine::Math::LengthSquared(up) > kDirectionEpsilon
                                             ? CoreEngine::Math::Normalize(up)
                                             : CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f};
        CoreEngine::Math::Vec3 x = CoreEngine::Math::Cross(safe_up, z);
        if (CoreEngine::Math::LengthSquared(x) <= kDirectionEpsilon) {
            safe_up = std::fabs(z.y) < 0.999f
                          ? CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f}
                          : CoreEngine::Math::Vec3{1.0f, 0.0f, 0.0f};
            x = CoreEngine::Math::Cross(safe_up, z);
        }

        x = CoreEngine::Math::Normalize(x);
        const CoreEngine::Math::Vec3 y = CoreEngine::Math::Cross(z, x);
        return CoreEngine::Math::Mat4ToQuat(CoreEngine::Math::Mat4(CoreEngine::Math::Mat3{x, y, z}));
    }
} // namespace Game
