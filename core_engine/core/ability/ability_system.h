#pragma once

#include "core/ability/ability_component.h"
#include "core/application/frame_context.h"
#include "core/ecs/node.h"

namespace CoreEngine {
    class AbilitySystem {
    public:
        AbilitySystem(World &world);
        bool Initialize();
        void Update(const FrameContext &frame);
        [[nodiscard]] AbilityComponent *TryGetAbilityComponent(Node node);
        [[nodiscard]] AbilityComponent *EnsureAbilityComponent(Node node);

    private:
        bool initialized_ = false;
        World &world_;
    };
} // namespace CoreEngine
