#pragma once

#include <memory>

#include "controller/controller.h"
#include "core/i_game_app.h"
#include "player/player.h"

namespace TopDownGame {
    class GameApp final : public CoreEngine::IGameApp {
    public:
        GameApp() = default;

        ~GameApp() override = default;

        void Init(const CoreEngine::EngineContext &context) override;

        void Update(const CoreEngine::FrameContext &frame) override;

        void Shutdown(const CoreEngine::EngineContext &context) override;

    private:
        std::unique_ptr<Controller> controller_;
        std::unique_ptr<Player> player_;
    };
} // namespace TopDownGame
