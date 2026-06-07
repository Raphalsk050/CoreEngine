#include "core/ability/ability_system.h"

#include "core/ecs/world.h"
#include "core/log/log.h"

namespace CoreEngine {
    AbilitySystem::AbilitySystem(World &world) : world_(world) {
    }

    bool AbilitySystem::Initialize() {
        if (initialized_) {
            Log::Warn("Ability", "Ability system already initialized");
            return false;
        }

        initialized_ = true;
        return true;
    }

    void AbilitySystem::Update(const FrameContext &frame) {
        (void) frame;
    }

    AbilityComponent *AbilitySystem::TryGetAbilityComponent(Node node) {
        if (!node.IsValid() || node.OwnerWorld() != &world_) {
            return nullptr;
        }

        return node.TryGetComponent<AbilityComponent>();
    }

    AbilityComponent *AbilitySystem::EnsureAbilityComponent(Node node) {
        if (!node.IsValid() || node.OwnerWorld() != &world_) {
            return nullptr;
        }

        if (AbilityComponent *existing = node.TryGetComponent<AbilityComponent>()) {
            return existing;
        }

        return &node.AddComponent<AbilityComponent>();
    }
} // namespace CoreEngine
