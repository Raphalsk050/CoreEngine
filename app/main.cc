#include <filesystem>

#include "core/application/runtime.h"
#include "game/bounty_hunters_game.h"

using namespace CoreEngine;

int main() {
    const std::filesystem::path projectRoot = std::filesystem::absolute(
        std::filesystem::path{__FILE__}).parent_path().parent_path();

    Game::BountyHuntersGame bountyHuntersGame;
    EngineConfig config = EngineConfig{
        .windowWidth = 1600, .windowHeight = 900,
        .fullscreen = false,
        .resizable = true,
        .decorated = true,
        .highDPI = true,
        .vsync = false,
        .enableImGui = true,
        .enableEditor = true,
        .renderBackend = RenderBackendType::DiligentD3D11,
        .windowTitle = "BountyHunters Editor",
        .projectRoot = projectRoot,
        .editorAssetRoots = {
            projectRoot / "app" / "assets",
            projectRoot / "core_engine" / "assets",
        },
    };
    Runtime runtime(config);

    runtime.Run(bountyHuntersGame);
    return 0;
}
