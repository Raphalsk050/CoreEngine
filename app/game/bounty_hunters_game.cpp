#include "bounty_hunters_game.h"

#include "core/log/log.h"
#include "game/network/bounty_replication.h"

namespace Game {
    void BountyHuntersGame::Init(const CoreEngine::EngineContext &context) {
        if (!RegisterBountyReplicatedComponents(context.multiplayer)) {
            CoreEngine::Log::Warn("Game", "One or more Bounty replicated components failed to register");
        }

        player_ = std::make_unique<Player>();
        if (!player_->Initialize(context)) {
            CoreEngine::Log::Warn("Game", "Player initialized with degraded multiplayer or input state");
        }
    }

    void BountyHuntersGame::FixedUpdate(const CoreEngine::FixedFrameContext &frame) {
        if (player_ != nullptr) {
            player_->FixedUpdate(frame);
        }
    }

    void BountyHuntersGame::Update(const CoreEngine::FrameContext &frame) {
        if (player_ != nullptr) {
            player_->Update(frame);
        }
    }

    void BountyHuntersGame::Shutdown(const CoreEngine::EngineContext &context) {
        (void) context;
        if (player_ != nullptr) {
            player_->Shutdown();
            player_.reset();
        }
    }
} // Game
