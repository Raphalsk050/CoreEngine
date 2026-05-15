#include "third_person_camera_controller.h"

#include <cmath>

#include "core/math/math.h"

namespace {
    using namespace CoreEngine::Math;

    [[nodiscard]] Quat LookRotationLH(const Vec3 &forward, const Vec3 &up) {
        const auto z = Normalize(forward);
        const auto x = Normalize(Cross(up, z));
        const auto y = Cross(z, x);
        const Mat3 basis{x, y, z};
        return Mat4ToQuat(Mat4(basis));
    }
}

namespace Game {
    void ThirdPersonCameraController::Attach(CoreEngine::Node camera_node, CoreEngine::Node target_node) {
        camera_node_ = camera_node;
        target_node_ = target_node;
    }

    void ThirdPersonCameraController::ApplyLookDelta(const Vec2 &look_delta) noexcept {
        yaw_degrees_ += look_delta.x;
        pitch_degrees_ = Clamp(
            pitch_degrees_ - look_delta.y,
            min_pitch_degrees_,
            max_pitch_degrees_);
    }

    Vec3 ThirdPersonCameraController::Forward() const noexcept {
        const float yaw_radians = Deg2Rad(yaw_degrees_);
        const float pitch_radians = Deg2Rad(pitch_degrees_);

        return Normalize(Vec3{
            std::cos(pitch_radians) * std::sin(yaw_radians),
            std::sin(pitch_radians),
            std::cos(pitch_radians) * std::cos(yaw_radians),
        });
    }

    Vec3 ThirdPersonCameraController::PlanarForward() const noexcept {
        Vec3 forward = Forward();
        forward.y = 0.0f;

        const float length_squared = Dot(forward, forward);
        if (length_squared <= 0.0f) {
            return {0.0f, 0.0f, 1.0f};
        }

        return Normalize(forward);
    }

    Vec3 ThirdPersonCameraController::PlanarRight() const noexcept {
        return Normalize(
            Cross(Vec3{0.0f, 1.0f, 0.0f}, PlanarForward()));
    }

    void ThirdPersonCameraController::SetFocusOffset(const Vec3 &offset) noexcept {
        focus_offset_ = offset;
    }

    void ThirdPersonCameraController::SetDistance(float distance) noexcept {
        distance_ = distance;
    }

    float ThirdPersonCameraController::YawRadians() const noexcept {
        return Deg2Rad(yaw_degrees_);
    }

    float ThirdPersonCameraController::PitchRadians() const noexcept {
        return Deg2Rad(pitch_degrees_);
    }

    void ThirdPersonCameraController::Update(float delta_time) {
        if (!camera_node_.IsValid() || !target_node_.IsValid()) {
            return;
        }

        const Vec3 focus = target_node_.GetPosition() + focus_offset_;
        const Vec3 desired_position = focus - Forward() * distance_;
        const Vec3 current_position = camera_node_.GetPosition();

        const float alpha = 1.0f - std::exp(-follow_sharpness_ * delta_time);
        const Vec3 new_position = Lerp(current_position, desired_position, alpha);

        camera_node_.SetPosition(new_position);
        camera_node_.SetRotation(LookRotationLH(focus - new_position, {0.0f, 1.0f, 0.0f}));
    }
}
