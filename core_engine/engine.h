#pragma once
#include <memory>

#include "core/render/render_backend_type.h"

#define CENGINE_DEBUG_BUILD 1

namespace CoreEngine {
    struct EngineConfig {
        EngineConfig() = default;

        int windowWidth = 800;
        int windowHeight = 600;
        bool fullscreen = false;
        bool resizable = false;
        bool decorated = true;
        bool highDPI = true;
        bool vsync = true;
        bool enableImGui = false;
        RenderBackendType renderBackend = RenderBackendType::None;
        const char *windowTitle = "Sample game";
    };

    class IGameApp;

    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config);
}
