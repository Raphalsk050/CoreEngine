#pragma once

#include "engine.h"

namespace CoreEngine {
    class IGameApp {
    public:
        virtual ~IGameApp() = default;

        virtual void Init() = 0;

        virtual void Update(float deltaTime) = 0;

        virtual void Shutdown() = 0;
    };
}
