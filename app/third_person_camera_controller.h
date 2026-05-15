#pragma once

#include "core/ecs/node.h"
#include "core/math/math.h"

namespace Game {
    class ThirdPersonCameraController final {
    public:
        void Attach(CoreEngine::Node camera_node, CoreEngine::Node target_node);

        void ApplyLookDelta(const CoreEngine::Math::Vec2 &look_delta) noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 PlanarForward() const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 PlanarRight() const noexcept;

        void SetFocusOffset(const CoreEngine::Math::Vec3 &offset) noexcept;

        void SetDistance(float distance) noexcept;

        float GetDistance() const noexcept {
            return distance_;
        }

        [[nodiscard]] float YawRadians() const noexcept;

        [[nodiscard]] float PitchRadians() const noexcept;

        void Update(float delta_time);

    private:
        [[nodiscard]] CoreEngine::Math::Vec3 Forward() const noexcept;

        CoreEngine::Node camera_node_{};
        CoreEngine::Node target_node_{};

        CoreEngine::Math::Vec3 focus_offset_{0.0f, 1.25f, 0.0f};

        float distance_ = 4.0f;
        float yaw_degrees_ = 0.0f;
        float pitch_degrees_ = -15.0f;
        float min_pitch_degrees_ = -80.0f;
        float max_pitch_degrees_ = 80.0f;
        float follow_sharpness_ = 12.0f;
    };
}
