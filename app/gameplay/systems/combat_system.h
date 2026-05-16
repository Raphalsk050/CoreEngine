#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/network/replication/replicated_state_types.h"
#include "gameplay_system_context.h"

namespace Game {
    enum class HitRegion : std::uint8_t {
        Head,
        Torso,
        LeftArm,
        RightArm,
        Legs,
    };

    /**
     * @brief Applies server-authoritative weapon and damage validation.
     *
     * Responsibility: reject impossible combat actions and emit authoritative
     * health/armor state changes.
     */
    class CombatSystem {
    public:
        [[nodiscard]] float ComputeDamage(float base_damage, HitRegion region) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context);

    private:
        std::unordered_map<CoreEngine::PeerId, std::uint32_t> next_allowed_fire_tick_by_peer_;
    };
} // namespace Game
