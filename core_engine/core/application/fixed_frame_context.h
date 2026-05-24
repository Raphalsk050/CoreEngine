#pragma once

#include "core/application/engine_context.h"
#include "core/simulation/simulation_frame.h"

namespace CoreEngine {
    struct FixedFrameContext : EngineContext {
        SimulationFrame simulation{};
    };
} // namespace CoreEngine
