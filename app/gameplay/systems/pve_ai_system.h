#pragma once

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
    };
} // namespace Game
