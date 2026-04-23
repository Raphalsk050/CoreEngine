#pragma once

#include "camera_arm_component.h"
#include "i_possessable.h"
#include "movement_component.h"
#include "core/ecs/node.h"
#include "core/ecs/components/camera_component.h"

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

        [[nodiscard]] CoreEngine::Node &Node() noexcept;

        [[nodiscard]] const CoreEngine::Node &Node() const noexcept;

        [[nodiscard]] bool IsPossessed() const noexcept;

    private:
        [[nodiscard]] float ResolveSpeed(const PlayerCommand &command) const noexcept;

        CoreEngine::Node node_{};
        MovementComponent movement_{};
        CameraArmComponent camera_arm_{};
        bool possessed_ = false;
    };
} // namespace Game
