#pragma once

#include <cstdint>

#include "gameplay_system_context.h"

namespace Game {
    enum class WorkbenchType : std::uint8_t {
        Armory,
        Forge,
        TechBench,
        UpgradeStation,
    };

    /**
     * @brief Validates server-authoritative workbench crafting requests.
     *
     * Responsibility: keep recipe, proximity, resource, and capacity checks out
     * of UI prediction code.
     */
    class CraftingSystem {
    public:
        [[nodiscard]] bool IsRecipeAllowed(WorkbenchType bench, std::uint32_t recipe_id) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;
    };
} // namespace Game
