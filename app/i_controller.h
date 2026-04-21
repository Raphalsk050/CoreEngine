#pragma once

#include "i_possessable.h"

namespace CoreEngine {
    struct FrameContext;
}

namespace Game {
    class IController {
    public:
        virtual ~IController() = default;

        virtual void Update(const CoreEngine::FrameContext &frame) = 0;

        virtual void Possess(IPossessable &possessable) = 0;

        virtual void Unpossess() = 0;
    };
} // namespace Game