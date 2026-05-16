#pragma once

#include <cstdint>
#include <unordered_map>

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

        void FixedUpdate(const GameplaySystemContext &context);

    private:
        std::unordered_map<CoreEngine::PeerId, std::uint32_t> next_allowed_capture_tick_by_peer_;
    };
} // namespace Game
