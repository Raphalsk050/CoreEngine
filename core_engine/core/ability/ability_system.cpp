#include "core/ability/ability_system.h"

#include "core/log/log.h"

namespace CoreEngine {
    AbilitySystem::AbilitySystem() {
    }

    bool AbilitySystem::Initialize() {
        if (initialized_) {
            Log::Warn("Ability", "Ability system already initialized");
            return false;
        }

        initialized_ = true;
    }

    void AbilitySystem::Update(const FrameContext &frame) {
    }
} // namespace CoreEngine
