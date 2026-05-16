#pragma once

#include <cstdint>

#include "core/network/replication/network_identity_component.h"
#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Advances server-only PvE actors.
     *
     * Responsibility: keep AI authoritative on the host/server while clients
     * receive only replicated/interpolated state.
     */
    class PvEAISystem {
    public:
        void FixedUpdate(const GameplaySystemContext &context) noexcept;

    private:
        CoreEngine::NetworkEntityId ai_id_ = 0;
        std::uint32_t next_attack_tick_ = 0;
    };
} // namespace Game
