#pragma once
#include <memory>

namespace CoreEngine {
    struct EngineConfig {
        EngineConfig() = default;

        int windowWidth = 800;
        int windowHeight = 600;
        const char *windowTitle = "My Game";
    };

    class IGameApp;

    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config);
}
