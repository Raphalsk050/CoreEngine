#include "gameplay/systems/match_session_system.h"

#include "core/ecs/world.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/replicated_state_types.h"

namespace Game {
    void MatchSessionSystem::Reset() noexcept {
        state_ = MatchSessionState::Lobby;
        seed_ = 0;
        state_enter_tick_ = 0;
    }

    void MatchSessionSystem::BeginMatch(std::uint32_t seed) noexcept {
        seed_ = seed;
        state_ = MatchSessionState::LoadingTargetReveal;
        state_enter_tick_ = 0;
    }

    void MatchSessionSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (context.network_system.Session().Role() == CoreEngine::NetworkRole::Client) {
            return;
        }

        if (state_ == MatchSessionState::Lobby) {
            auto players = context.world.View<CoreEngine::NetworkIdentityComponent, CoreEngine::HealthComponent>();
            if (players.begin() != players.end()) {
                seed_ = 0xB0470001u ^ context.frame.tick;
                state_ = MatchSessionState::LoadingTargetReveal;
                state_enter_tick_ = context.frame.tick;
            }
            return;
        }

        const std::uint32_t elapsed_ticks = context.frame.tick - state_enter_tick_;
        if (state_ == MatchSessionState::LoadingTargetReveal && elapsed_ticks > 30) {
            state_ = MatchSessionState::Drop;
            state_enter_tick_ = context.frame.tick;
        } else if (state_ == MatchSessionState::Drop && elapsed_ticks > 60) {
            state_ = MatchSessionState::ActiveHunt;
        }
    }
} // namespace Game
