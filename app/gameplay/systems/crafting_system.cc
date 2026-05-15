#include "gameplay/systems/crafting_system.h"

namespace Game {
    bool CraftingSystem::IsRecipeAllowed(WorkbenchType bench, std::uint32_t recipe_id) const noexcept {
        (void) bench;
        return recipe_id != 0;
    }

    void CraftingSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
