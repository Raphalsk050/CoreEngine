#pragma once
#include "core/i_game_app.h"

namespace TopDownGame {
    class GameApp final : public CoreEngine::IGameApp {
    public:
        GameApp() = default;

        ~GameApp() override = default;

        void Init(const CoreEngine::EngineContext &context) override;

        void Update(const CoreEngine::FrameContext &frame) override;

        void Shutdown(const CoreEngine::EngineContext &context) override;
    };
} // TopDownGame
