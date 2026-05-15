#include "gameplay/systems/inventory_system.h"

namespace Game {
    bool InventorySystem::CanAddItem(const CoreEngine::InventoryComponent &inventory) const noexcept {
        return inventory.item_ids.size() < inventory.capacity;
    }

    void InventorySystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
