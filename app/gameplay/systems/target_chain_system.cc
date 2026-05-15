#include "gameplay/systems/target_chain_system.h"

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
        (void) context;
    }
} // namespace Game
