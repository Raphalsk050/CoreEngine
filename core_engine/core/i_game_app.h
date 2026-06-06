#pragma once

#include "application/engine_context.h"
#include "application/frame_context.h"
#include "engine.h"

namespace CoreEngine {
    class IGameApp {
    public:
        virtual ~IGameApp() = default;

        virtual void Init(const EngineContext &context) = 0;

        virtual void Update(const FrameContext &frame) = 0;

        virtual void Shutdown(const EngineContext &context) = 0;
    };
} // namespace CoreEngine
