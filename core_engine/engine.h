#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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
        bool enableEditor = false;
        RenderBackendType renderBackend = RenderBackendType::None;
        std::string windowTitle = "Sample game";
        std::filesystem::path projectRoot;
        std::vector<std::filesystem::path> editorAssetRoots;
    };

    class IGameApp;

    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config);
}
