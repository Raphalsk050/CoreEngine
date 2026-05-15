#pragma once

#include "engine.h"
#include "application/engine_context.h"
#include "application/frame_context.h"
#include "core/simulation/simulation_frame.h"

namespace CoreEngine {
    class IGameApp {
    public:
        virtual ~IGameApp() = default;

        virtual void Init(const EngineContext &context) = 0;

        virtual void FixedUpdate(const SimulationFrame &frame) {
            (void) frame;
        }

        virtual void Update(const FrameContext &frame) = 0;

        virtual void Shutdown(const EngineContext &context) = 0;
    };
}
