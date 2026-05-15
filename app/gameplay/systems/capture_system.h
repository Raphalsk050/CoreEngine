#pragma once

#include "core/network/replication/replicated_state_types.h"
#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Validates and advances Cryo-Cuffs capture attempts.
     *
     * Responsibility: keep capture completion server-authoritative while clients
     * may only present predicted UI/animation feedback.
     */
    class CaptureSystem {
    public:
        [[nodiscard]] bool CanStartCapture(const CoreEngine::HealthComponent &target) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;
    };
} // namespace Game
