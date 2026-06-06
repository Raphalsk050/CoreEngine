#include <core/application/application.h>
#include <core/application/runtime.h>
#include "game_app.h"

using namespace CoreEngine;

int main() {
    EngineConfig config{
        .window_width = 1280,
        .window_height = 720,
        .fullscreen = false,
        .resizable = true,
        .decorated = true,
        .high_dpi = true,
        .vsync = false,
        .render_backend = RenderBackendType::DiligentD3D11,
        .window_title = "Third person game",
    };

    auto app = std::make_unique<TopDownGame::GameApp>();

    return RunEngine(std::move(app), config);
}
