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
        transform->position += move * speed * delta_time;
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
} // namespace Game
