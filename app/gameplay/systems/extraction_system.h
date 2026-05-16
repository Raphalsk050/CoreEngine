#pragma once

#include "core/network/replication/replicated_state_types.h"
#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Owns public LZ extraction state.
     *
     * Responsibility: make extraction activation, arrival, boarding, and
     * departure server-authoritative public gameplay events.
     */
    class ExtractionSystem {
    public:
        void Activate(CoreEngine::ExtractionStateComponent &state, float arrival_seconds) const noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;

        [[nodiscard]] const CoreEngine::ExtractionStateComponent &State() const noexcept {
            return state_;
        }

    private:
        CoreEngine::ExtractionStateComponent state_;
    };
} // namespace Game
