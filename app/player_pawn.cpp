#include "player_pawn.h"

#include "core/math/math.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"

namespace Game {
    PlayerPawn::PlayerPawn(CoreEngine::Node node, MovementComponent movement,
                           CameraArmComponent camera_arm_component)
        : node_(node), movement_(movement), camera_arm_(camera_arm_component) {
    }

    void PlayerPawn::OnPossessed() {
        possessed_ = true;
    }

    void PlayerPawn::OnUnpossessed() {
        possessed_ = false;
    }

    void PlayerPawn::ApplyPlayerCommand(const PlayerCommand &command, float delta_time) {
        if (!possessed_ || !node_.IsValid() || delta_time <= 0.0f) {
            return;
        }

        CoreEngine::Math::Vec3 move = command.world_move;
        const float length_squared = CoreEngine::Math::Dot(move, move);
        if (length_squared <= 0.0f) {
            return;
        }

        if (length_squared > 1.0f) {
            move = CoreEngine::Math::Normalize(move);
        }

        auto *transform = node_.TryGetComponent<CoreEngine::TransformComponent>();
        if (transform == nullptr) {
            return;
        }

        const float speed = ResolveSpeed(command);
        transform->SetPosition(transform->Position() + move * speed * delta_time);

        RotateThroughMovement(delta_time, transform, move);
    }

    void PlayerPawn::ApplyPlayerInputCommand(const CoreEngine::PlayerInputCommand &command, float fixed_delta_time) {
        if (!possessed_ || !node_.IsValid() || fixed_delta_time <= 0.0f) {
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

        auto *transform = node_.TryGetComponent<CoreEngine::TransformComponent>();
        if (transform == nullptr) {
            return;
        }

        const float speed = ResolveSpeed(command);
        transform->SetPosition(transform->Position() + move * speed * fixed_delta_time);
        RotateThroughMovement(fixed_delta_time, transform, move);
    }

    CoreEngine::PredictedMovementState PlayerPawn::BuildMovementState() const noexcept {
        if (!node_.IsValid()) {
            return {};
        }

        const auto *transform = node_.TryGetComponent<CoreEngine::TransformComponent>();
        if (transform == nullptr) {
            return {};
        }

        return CoreEngine::PredictedMovementState{
            .position = transform->Position(),
            .rotation = transform->Rotation(),
            .velocity = {},
            .movement_flags = 0,
        };
    }

    CoreEngine::Node &PlayerPawn::Node() noexcept {
        return node_;
    }

    const CoreEngine::Node &PlayerPawn::Node() const noexcept {
        return node_;
    }

    bool PlayerPawn::IsPossessed() const noexcept {
        return possessed_;
    }

    float PlayerPawn::ResolveSpeed(const PlayerCommand &command) const noexcept {
        if (command.run_held) {
            return movement_.run_speed;
        }

        switch (movement_.default_movement_type) {
            case MovementType::Crouch:
                return movement_.crouch_speed;
            case MovementType::Walk:
                return movement_.walk_speed;
            case MovementType::Run:
                return movement_.run_speed;
        }

        return movement_.walk_speed;
    }

    float PlayerPawn::ResolveSpeed(const CoreEngine::PlayerInputCommand &command) const noexcept {
        if (command.IsButtonDown(CoreEngine::PlayerInputButton::Sprint)) {
            return movement_.run_speed;
        }

        switch (movement_.default_movement_type) {
            case MovementType::Crouch:
                return movement_.crouch_speed;
            case MovementType::Walk:
                return movement_.walk_speed;
            case MovementType::Run:
                return movement_.run_speed;
        }

        return movement_.walk_speed;
    }

    void PlayerPawn::RotateThroughMovement(float delta_time,
                                           CoreEngine::TransformComponent *transform,
                                           CoreEngine::Math::Vec3 move) noexcept {
        CoreEngine::Math::Vec3 facing = move;
        facing.y = 0.0f;

        if (CoreEngine::Math::LengthSquared(facing) > 0.0001f) {
            facing = CoreEngine::Math::Normalize(facing);

            const float yaw = std::atan2(facing.x, facing.z); // +Z is forward, LH
            const auto target_rotation =
                    CoreEngine::Math::AngleAxis(yaw, CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f});

            const float turn_alpha = 1.0f - std::exp(-20.0f * delta_time);
            transform->SetRotation(CoreEngine::Math::Slerp(transform->Rotation(), target_rotation, turn_alpha));
        }
    }
} // namespace Game
