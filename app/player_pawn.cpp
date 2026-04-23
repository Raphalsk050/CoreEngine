#include "player_pawn.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"

namespace Game {
    PlayerPawn::PlayerPawn(CoreEngine::Node node, MovementComponent movement)
        : node_(node), movement_(movement) {
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

        glm::vec2 movement = command.movement;
        const float length_squared = glm::dot(movement, movement);
        if (length_squared <= 0.0f) {
            return;
        }

        if (length_squared > 1.0f) {
            movement = glm::normalize(movement);
        }

        auto *transform = node_.TryGetComponent<CoreEngine::TransformComponent>();
        if (transform == nullptr) {
            return;
        }

        const float speed = ResolveSpeed(command);
        transform->position += glm::vec3(movement.x, 0.0f, movement.y) * speed * delta_time;
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