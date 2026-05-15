#include "gameplay/systems/economy_result_system.h"

namespace Game {
    EconomyPayout EconomyResultSystem::Calculate(bool delivered_correct_beacon,
                                                 bool delivered_live_target) const noexcept {
        return EconomyPayout{
            .base_reward = delivered_correct_beacon ? 100 : 0,
            .live_bonus = delivered_live_target ? 75 : 0,
            .style_bonus = 0,
            .penalties = 0,
        };
    }

    void EconomyResultSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
