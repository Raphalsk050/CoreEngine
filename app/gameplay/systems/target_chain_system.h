#pragma once

#include <span>
#include <vector>

#include "core/network/replication/replicated_state_types.h"
#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Owns server-authoritative target assignments.
     *
     * Responsibility: generate and update the closed bounty target chain without
     * exposing the full chain to clients.
     */
    class TargetChainSystem {
    public:
        void Reset();

        void BuildClosedCycle(std::span<const CoreEngine::NetworkEntityId> players);

        void FixedUpdate(const GameplaySystemContext &context) noexcept;

        [[nodiscard]] std::span<const CoreEngine::TargetAssignmentComponent> Assignments() const noexcept {
            return assignments_;
        }

    private:
        std::vector<CoreEngine::TargetAssignmentComponent> assignments_;
    };
} // namespace Game
