#include "gameplay/systems/match_session_system.h"

namespace Game {
    void MatchSessionSystem::Reset() noexcept {
        state_ = MatchSessionState::Lobby;
        seed_ = 0;
    }

    void MatchSessionSystem::BeginMatch(std::uint32_t seed) noexcept {
        seed_ = seed;
        state_ = MatchSessionState::LoadingTargetReveal;
    }

    void MatchSessionSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (state_ == MatchSessionState::LoadingTargetReveal && context.frame.tick > 1) {
            state_ = MatchSessionState::Drop;
        } else if (state_ == MatchSessionState::Drop && context.frame.tick > 60) {
            state_ = MatchSessionState::ActiveHunt;
        }
    }
} // namespace Game
