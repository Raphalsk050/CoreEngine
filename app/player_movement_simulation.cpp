#include "player_movement_simulation.h"

#include "core/math/math.h"

#include <cmath>

namespace Game {
    void PlayerMovementSimulation::ApplyInputCommand(CoreEngine::TransformComponent &transform,
                                                     const MovementComponent &movement,
                                                     const CoreEngine::PlayerInputCommand &command,
                                                     float fixed_delta_time) noexcept {
        if (fixed_delta_time <= 0.0f) {
            return;
        }

        CoreEngine::Math::Vec3 move{
            std::sin(command.look_yaw) * command.move_y + std::cos(command.look_yaw) * command.move_x,
            0.0f,
            std::cos(command.look_yaw) * command.move_y - std::sin(command.look_yaw) * command.move_x,
        };

        const float length_squared = CoreEngine::Math::Dot(move, move);
        if (length_squared <= 0.0f) {
            return;
        }

        if (length_squared > 1.0f) {
            move = CoreEngine::Math::Normalize(move);
        }

        const float speed = ResolveSpeed(movement, command);
        transform.SetPosition(transform.Position() + move * speed * fixed_delta_time);
        RotateThroughMovement(fixed_delta_time, transform, move);
    }

    CoreEngine::PredictedMovementState PlayerMovementSimulation::BuildMovementState(
        const CoreEngine::TransformComponent &transform) noexcept {
        return CoreEngine::PredictedMovementState{
            .position = transform.Position(),
            .rotation = transform.Rotation(),
            .velocity = {},
            .movement_flags = 0,
        };
    }

    float PlayerMovementSimulation::ResolveSpeed(const MovementComponent &movement,
                                                 const CoreEngine::PlayerInputCommand &command) noexcept {
        if (command.IsButtonDown(CoreEngine::PlayerInputButton::Sprint)) {
            return movement.run_speed;
        }

        switch (movement.default_movement_type) {
            case MovementType::Crouch:
                return movement.crouch_speed;
            case MovementType::Walk:
                return movement.walk_speed;
            case MovementType::Run:
                return movement.run_speed;
        }

        return movement.walk_speed;
    }

    void PlayerMovementSimulation::RotateThroughMovement(float fixed_delta_time,
                                                         CoreEngine::TransformComponent &transform,
                                                         CoreEngine::Math::Vec3 move) noexcept {
        CoreEngine::Math::Vec3 facing = move;
        facing.y = 0.0f;

        if (CoreEngine::Math::LengthSquared(facing) > 0.0001f) {
            facing = CoreEngine::Math::Normalize(facing);

            const float yaw = std::atan2(facing.x, facing.z);
            const auto target_rotation =
                CoreEngine::Math::AngleAxis(yaw, CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f});

            const float turn_alpha = 1.0f - std::exp(-20.0f * fixed_delta_time);
            transform.SetRotation(CoreEngine::Math::Slerp(transform.Rotation(), target_rotation, turn_alpha));
        }
    }
} // namespace Game
