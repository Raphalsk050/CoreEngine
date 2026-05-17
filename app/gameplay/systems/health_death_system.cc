#include "gameplay/systems/health_death_system.h"

#include "core/ecs/world.h"
#include "core/network/multiplayer_system.h"
#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/replicated_state_types.h"

namespace Game {
    void HealthDeathSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (context.multiplayer.Role() == CoreEngine::NetworkRole::Client) {
            return;
        }

        auto view = context.world.View<CoreEngine::NetworkIdentityComponent, CoreEngine::HealthComponent>();
        for (const entt::entity entity: view) {
            auto &health = view.get<CoreEngine::HealthComponent>(entity);
            if (health.health <= 0.0f) {
                health.health = 0.0f;
                health.alive = false;
                health.concussed = false;
            } else if (health.health <= health.max_health * 0.25f) {
                health.concussed = true;
            }
        }
    }
} // namespace Game
