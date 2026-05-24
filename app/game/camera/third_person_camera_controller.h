#pragma once

#include "core/ecs/node.h"
#include "core/math/math.h"

namespace CoreEngine {
    struct SpringArmComponent;
}

namespace Game {
    /**
     * @brief Updates a camera entity from a target entity and SpringArmComponent.
     *
     * Responsibility: keep third-person camera presentation outside the player
     * simulation path so networked movement remains authoritative and deterministic.
     */
    class ThirdPersonCameraController final {
    public:
        [[nodiscard]] bool Attach(CoreEngine::Node camera_node, CoreEngine::Node target_node) noexcept;

        void Detach() noexcept;

        void ApplyLookDeltaRadians(const CoreEngine::Math::Vec2 &look_delta) noexcept;

        void AdjustDistance(float distance_delta) noexcept;

        void SetDistanceLimits(float min_distance, float max_distance) noexcept;

        void Update(float delta_seconds) noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 PlanarForward() const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 PlanarRight() const noexcept;

        [[nodiscard]] float YawRadians() const noexcept;

        [[nodiscard]] float PitchRadians() const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 Forward() const noexcept;

        [[nodiscard]] CoreEngine::Node GetCameraNode() const {
            return camera_node_;
        }

    private:
        [[nodiscard]] CoreEngine::SpringArmComponent *Arm() noexcept;

        [[nodiscard]] const CoreEngine::SpringArmComponent *Arm() const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 PivotPosition(const CoreEngine::SpringArmComponent &arm) const noexcept;

        [[nodiscard]] static CoreEngine::Math::Quat LookRotationLH(const CoreEngine::Math::Vec3 &forward,
                                                                   const CoreEngine::Math::Vec3 &up) noexcept;

        CoreEngine::Node camera_node_{};
        CoreEngine::Node target_node_{};
        float min_distance_ = 2.0f;
        float max_distance_ = 12.0f;
    };
} // namespace Game
