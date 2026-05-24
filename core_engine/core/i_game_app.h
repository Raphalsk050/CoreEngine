#pragma once

#include "engine.h"
#include "application/engine_context.h"
#include "application/fixed_frame_context.h"
#include "application/frame_context.h"

namespace CoreEngine {
    class IGameApp {
    public:
        virtual ~IGameApp() = default;

        virtual void Init(const EngineContext &context) = 0;

        virtual void FixedUpdate(const FixedFrameContext &frame) {
            (void) frame;
        }

        virtual void Update(const FrameContext &frame) = 0;

        virtual void Shutdown(const EngineContext &context) = 0;
    };
}
