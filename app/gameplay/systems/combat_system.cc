#include "gameplay/systems/combat_system.h"

namespace Game {
    float CombatSystem::ComputeDamage(float base_damage, HitRegion region) const noexcept {
        return region == HitRegion::Head ? base_damage * 1.5f : base_damage;
    }

    void CombatSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
