#include <core/i_game_app.h>
#include <iostream>
#include <string>

#include <core/application/application.h>
#include <core/ecs/components/transform_component.h>
#include <core/ecs/components/name_component.h>
#include <core/ecs/world.h>
#include <core/ecs/world_access.h>
#include <core/log/log.h>

#include "core/debug/debug.h"
#include "core/render/render_system.h"

class MyGameApp final : public CoreEngine::IGameApp {
public:
    MyGameApp() = default;

    void Init(const CoreEngine::EngineContext &context) override {
        (void) context;
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        const float delta_time = frame.delta_time;

        CoreEngine::Log::Debug("Game",
                               "Update: DeltaTime: " + std::to_string(delta_time));

        CoreEngine::Log::Debug("Game",
                               "Update: FPS: " + std::to_string(1.0 / delta_time));
    }

    void Shutdown(const CoreEngine::EngineContext &context) override {
        (void) context;
    }
};

int main() {
    std::unique_ptr<CoreEngine::IGameApp> gameApp = std::make_unique<MyGameApp>();

    CoreEngine::EngineConfig config;
    config.windowWidth = 1024;
    config.windowHeight = 768;
    config.fullscreen = false;
    config.decorated = true;
    config.resizable = false;
    config.windowTitle = "My Awesome Game";
    config.renderBackend = CoreEngine::RenderBackendType::DiligentD3D12;
    config.vsync = false;

    return CoreEngine::RunEngine(std::move(gameApp), config);
}
