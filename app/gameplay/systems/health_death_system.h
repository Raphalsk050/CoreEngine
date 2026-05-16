#pragma once

#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Finalizes authoritative health transitions into death state.
     *
     * Responsibility: keep alive/dead flags consistent after combat, capture,
     * and PvE systems mutate health values on the host.
     */
    class HealthDeathSystem final {
    public:
        void FixedUpdate(const GameplaySystemContext &context) noexcept;
    };
} // namespace Game
