#pragma once
#include <memory>
#include <string>

#include "core/render/render_backend_type.h"
#include "core/render/render_desc.h"

namespace CoreEngine {
    struct EngineConfig {
        int window_width = 800;
        int window_height = 600;
        bool fullscreen = false;
        bool resizable = false;
        bool decorated = true;
        bool high_dpi = true;
        bool vsync = true;
        bool enable_imgui = false;
        RenderBackendType render_backend = RenderBackendType::None;
        PbrRenderSettings pbr{};
        std::string window_title = "Sample game";
    };

    class IGameApp;

    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config);
} // namespace CoreEngine
