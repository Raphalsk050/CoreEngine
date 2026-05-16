#pragma once

#include <cstdint>

#include "gameplay_system_context.h"

namespace Game {
    enum class MatchSessionState : std::uint8_t {
        Lobby,
        LoadingTargetReveal,
        Drop,
        ActiveHunt,
        ExtractionActive,
        MatchEnding,
        Completed,
    };

    /**
     * @brief Owns high-level authoritative match state transitions.
     *
     * Responsibility: keep lobby, drop, hunt, extraction, and result phases in
     * one server-driven state machine.
     */
    class MatchSessionSystem {
    public:
        void Reset() noexcept;

        void BeginMatch(std::uint32_t seed) noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;

        [[nodiscard]] MatchSessionState State() const noexcept {
            return state_;
        }

        [[nodiscard]] std::uint32_t Seed() const noexcept {
            return seed_;
        }

    private:
        MatchSessionState state_ = MatchSessionState::Lobby;
        std::uint32_t seed_ = 0;
        std::uint32_t state_enter_tick_ = 0;
    };
} // namespace Game
