#pragma once

#include "camera_arm_component.h"
#include "i_possessable.h"
#include "movement_component.h"
#include "core/ecs/node.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/prediction/prediction_buffer.h"

namespace Game {
    class PlayerPawn final : public IPossessable {
    public:
        PlayerPawn() = default;

        explicit PlayerPawn(CoreEngine::Node node, MovementComponent movement = {},
                            CameraArmComponent camera_arm_component = {}

        );

        void OnPossessed() override;

        void OnUnpossessed() override;

        void ApplyPlayerCommand(const PlayerCommand &command, float delta_time) override;

        void ApplyPlayerInputCommand(const CoreEngine::PlayerInputCommand &command, float fixed_delta_time);

        [[nodiscard]] CoreEngine::PredictedMovementState BuildMovementState() const noexcept;

        [[nodiscard]] CoreEngine::Node &Node() noexcept;

        [[nodiscard]] const CoreEngine::Node &Node() const noexcept;

        [[nodiscard]] bool IsPossessed() const noexcept;

    private:
        [[nodiscard]] float ResolveSpeed(const PlayerCommand &command) const noexcept;

        [[nodiscard]] float ResolveSpeed(const CoreEngine::PlayerInputCommand &command) const noexcept;

        void RotateThroughMovement(float delta_time,
                                   CoreEngine::TransformComponent *transform,
                                   CoreEngine::Math::Vec3 move) noexcept;

        CoreEngine::Node node_{};
        MovementComponent movement_{};
        CameraArmComponent camera_arm_{};
        bool possessed_ = false;
    };
} // namespace Game
