#include "core/application/runtime.h"
#include "game/bounty_hunters_game.h"

using namespace CoreEngine;

int main() {
    Game::BountyHuntersGame bountyHuntersGame;
    EngineConfig config = EngineConfig{
        .windowWidth = 800, .windowHeight = 600,
        .fullscreen = false,
        .resizable = true,
        .decorated = true,
        .highDPI = true,
        .vsync = false,
        .enableImGui = true,
        .renderBackend = RenderBackendType::DiligentD3D11,
        .windowTitle = "BountyHunters"
    };
    Runtime runtime(config);

    runtime.Run(bountyHuntersGame);
    return 0;
}