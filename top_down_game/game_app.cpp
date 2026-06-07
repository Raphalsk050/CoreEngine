#include "game_app.h"
#include "controller/controller.h"

namespace TopDownGame {
    void GameApp::Init(const CoreEngine::EngineContext &context) {
        controller_ = std::make_unique<Controller>(context);
        player_ = std::make_unique<Player>(context);
        scenario_ = std::make_unique<Scenario>(context);

        controller_->Possess(*player_);
    }

    void GameApp::Update(const CoreEngine::FrameContext &frame) {
        if (controller_ != nullptr) {
            controller_->Update(frame);
        }
    }

    void GameApp::Shutdown(const CoreEngine::EngineContext &context) {
    }
} // namespace TopDownGame
