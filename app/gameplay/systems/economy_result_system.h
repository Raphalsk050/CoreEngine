#pragma once

#include <cstdint>

#include "gameplay_system_context.h"

namespace Game {
    struct EconomyPayout {
        std::int32_t base_reward = 0;
        std::int32_t live_bonus = 0;
        std::int32_t style_bonus = 0;
        std::int32_t penalties = 0;

        [[nodiscard]] std::int32_t Total() const noexcept {
            return base_reward + live_bonus + style_bonus - penalties;
        }
    };

    /**
     * @brief Calculates authoritative extraction and match rewards.
     *
     * Responsibility: keep payout results derived from replicated match state,
     * not from client UI or client inventory claims.
     */
    class EconomyResultSystem {
    public:
        [[nodiscard]] EconomyPayout Calculate(bool delivered_correct_beacon,
                                              bool delivered_live_target) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;
    };
} // namespace Game
