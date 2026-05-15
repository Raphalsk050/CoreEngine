#include "gameplay/systems/capture_system.h"

namespace Game {
    bool CaptureSystem::CanStartCapture(const CoreEngine::HealthComponent &target) const noexcept {
        return target.alive && (target.concussed || target.health <= target.max_health * 0.25f);
    }

    void CaptureSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
