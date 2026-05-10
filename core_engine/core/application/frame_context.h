#pragma once

#include "core/application/engine_context.h"

namespace CoreEngine {
    struct FrameContext : EngineContext {
        float delta_time = 0.0f;
    };
} // namespace CoreEngine
