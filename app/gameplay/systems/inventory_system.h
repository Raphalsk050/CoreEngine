#pragma once

#include "core/network/replication/replicated_state_types.h"
#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Owns authoritative inventory mutation.
     *
     * Responsibility: accept only server-validated pickup, drop, and equipment
     * changes before replicated inventory state is sent to clients.
     */
    class InventorySystem {
    public:
        [[nodiscard]] bool CanAddItem(const CoreEngine::InventoryComponent &inventory) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;
    };
} // namespace Game
