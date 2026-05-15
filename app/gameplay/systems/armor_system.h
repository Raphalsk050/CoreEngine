#pragma once

#include "gameplay/systems/combat_system.h"

namespace Game {
    /**
     * @brief Routes damage into segmented armor state.
     *
     * Responsibility: keep per-body-part armor depletion separate from combat
     * hit validation and health death rules.
     */
    class ArmorSystem {
    public:
        [[nodiscard]] float ApplyDamage(CoreEngine::ArmorSegmentsComponent &armor,
                                        HitRegion region,
                                        float damage) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;
    };
} // namespace Game
