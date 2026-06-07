#pragma once
#include "core/application/frame_context.h"

namespace CoreEngine {

    class AbilitySystem {
    public:
        AbilitySystem();
        bool Initialize();
        void Update(const FrameContext &frame);

    private:
        bool initialized_ = false;
    };

} // namespace CoreEngine
