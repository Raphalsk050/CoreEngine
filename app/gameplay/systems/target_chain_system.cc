#include "gameplay/systems/target_chain_system.h"

#include "core/ecs/world.h"
#include "core/network/multiplayer_system.h"
#include "core/network/replication/network_identity_component.h"

namespace Game {
    void TargetChainSystem::Reset() {
        assignments_.clear();
    }

    void TargetChainSystem::BuildClosedCycle(std::span<const CoreEngine::NetworkEntityId> players) {
        assignments_.clear();
        if (players.size() < 3) {
            return;
        }

        assignments_.reserve(players.size());
        for (std::size_t i = 0; i < players.size(); ++i) {
            const std::size_t target_index = (i + 1u) % players.size();
            const std::size_t hunter_index = (i + players.size() - 1u) % players.size();
            assignments_.push_back(CoreEngine::TargetAssignmentComponent{
                .target_player = players[target_index],
                .hunter_player = players[hunter_index],
                .required_beacon = 0,
                .state = CoreEngine::TargetObjectiveState::HuntAssignedTarget,
            });
        }
    }

    void TargetChainSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (context.multiplayer.Role() == CoreEngine::NetworkRole::Client ||
            !assignments_.empty()) {
            return;
        }

        std::vector<CoreEngine::NetworkEntityId> players;
        players.reserve(8);
        auto view = context.world.View<CoreEngine::NetworkIdentityComponent, CoreEngine::HealthComponent>();
        for (const entt::entity entity: view) {
            const auto &identity = view.get<CoreEngine::NetworkIdentityComponent>(entity);
            const auto &health = view.get<CoreEngine::HealthComponent>(entity);
            if (identity.IsNetworked() && health.alive) {
                players.push_back(identity.network_id);
            }
        }

        BuildClosedCycle(players);

        for (const CoreEngine::TargetAssignmentComponent &assignment: assignments_) {
            CoreEngine::Node target_node = context.multiplayer.FindNode(assignment.hunter_player);
            if (!target_node.IsValid()) {
                continue;
            }

            if (auto *existing = target_node.TryGetComponent<CoreEngine::TargetAssignmentComponent>();
                existing != nullptr) {
                *existing = assignment;
            } else {
                target_node.AddComponent<CoreEngine::TargetAssignmentComponent>(assignment);
            }
        }
    }
} // namespace Game
