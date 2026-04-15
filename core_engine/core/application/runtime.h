#pragma once
#include "engine.h"

namespace CoreEngine {
    class IGameApp;

    class Runtime {
    public:
        explicit Runtime(const EngineConfig &config);

        int Run(IGameApp &app);

    private:
        bool Initialize();

        void Tick(IGameApp *app, float deltaTime);

        void Shutdown(IGameApp &app);

        bool running_ = false;
        EngineConfig config_;
    };
}
