#include "core/network/player/networked_player_movement.h"

#include "core/math/math.h"

#include <cmath>

namespace CoreEngine {
    void NetworkedPlayerMovementSimulation::ApplyInputCommand(
        TransformComponent &transform,
        const NetworkedPlayerMovementComponent &movement,
        const PlayerInputCommand &command,
        float fixed_delta_time) noexcept {
        if (fixed_delta_time <= 0.0f) {
            return;
        }

        Math::Vec3 move{
            std::sin(command.look_yaw) * command.move_y + std::cos(command.look_yaw) * command.move_x,
            0.0f,
            std::cos(command.look_yaw) * command.move_y - std::sin(command.look_yaw) * command.move_x,
        };

        const float length_squared = Math::Dot(move, move);
        if (length_squared <= 0.0f) {
            return;
        }

        if (length_squared > 1.0f) {
            move = Math::Normalize(move);
        }

        const float speed = ResolveSpeed(movement, command);
        transform.SetPosition(transform.Position() + move * speed * fixed_delta_time);
        RotateThroughMovement(fixed_delta_time, transform, move);
    }

    PredictedMovementState NetworkedPlayerMovementSimulation::BuildMovementState(
        const TransformComponent &transform) noexcept {
        return PredictedMovementState{
            .position = transform.Position(),
            .rotation = transform.Rotation(),
            .velocity = {},
            .movement_flags = 0,
        };
    }

    float NetworkedPlayerMovementSimulation::ResolveSpeed(
        const NetworkedPlayerMovementComponent &movement,
        const PlayerInputCommand &command) noexcept {
        if (movement.sprint_action.IsValid() && command.IsActionDown(movement.sprint_action)) {
            return movement.run_speed;
        }

        switch (movement.default_movement_type) {
            case NetworkedPlayerMovementType::Crouch:
                return movement.crouch_speed;
            case NetworkedPlayerMovementType::Walk:
                return movement.walk_speed;
            case NetworkedPlayerMovementType::Run:
                return movement.run_speed;
        }

        return movement.walk_speed;
    }

    void NetworkedPlayerMovementSimulation::RotateThroughMovement(float fixed_delta_time,
                                                                  TransformComponent &transform,
                                                                  Math::Vec3 move) noexcept {
        Math::Vec3 facing = move;
        facing.y = 0.0f;

        if (Math::LengthSquared(facing) > 0.0001f) {
            facing = Math::Normalize(facing);

            const float yaw = std::atan2(facing.x, facing.z);
            const auto target_rotation = Math::AngleAxis(yaw, Math::Vec3{0.0f, 1.0f, 0.0f});

            const float turn_alpha = 1.0f - std::exp(-20.0f * fixed_delta_time);
            transform.SetRotation(Math::Slerp(transform.Rotation(), target_rotation, turn_alpha));
        }
    }
} // namespace CoreEngine
