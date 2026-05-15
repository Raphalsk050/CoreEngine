#include "gameplay/systems/extraction_system.h"

namespace Game {
    void ExtractionSystem::Activate(CoreEngine::ExtractionStateComponent &state,
                                    float arrival_seconds) const noexcept {
        state.state = CoreEngine::ExtractionState::ShipInbound;
        state.timer_seconds = arrival_seconds;
        state.public_event_active = true;
    }

    void ExtractionSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
