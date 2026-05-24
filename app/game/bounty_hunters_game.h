#pragma once

#include <memory>

#include "core/i_game_app.h"
#include "player/player.h"

namespace Game {
    /**
     * @brief App entry point that owns game-level systems.
     *
     * Responsibility: initialize and tick gameplay modules while the engine
     * runtime owns platform, rendering, fixed simulation, and networking.
     */
    class BountyHuntersGame : public CoreEngine::IGameApp {
    public:
        BountyHuntersGame() = default;

        void Init(const CoreEngine::EngineContext &context) override;

        void FixedUpdate(const CoreEngine::FixedFrameContext &frame) override;

        void Update(const CoreEngine::FrameContext &frame) override;

        void Shutdown(const CoreEngine::EngineContext &context) override;

    private:
        std::unique_ptr<Player> player_;
    };
} // Game
