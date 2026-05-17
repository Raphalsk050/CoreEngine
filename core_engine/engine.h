#pragma once
#include <memory>
#include <string>

#include "core/render/render_backend_type.h"

namespace CoreEngine {
    struct EngineConfig {
        int windowWidth = 800;
        int windowHeight = 600;
        bool fullscreen = false;
        bool resizable = false;
        bool decorated = true;
        bool highDPI = true;
        bool vsync = true;
        bool enableImGui = false;
        RenderBackendType renderBackend = RenderBackendType::None;
        std::string windowTitle = "Sample game";
    };

    class IGameApp;

    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config);
}
