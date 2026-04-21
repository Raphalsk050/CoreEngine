#pragma once

#include "player_command.h"

namespace Game {
    class IPossessable {
    public:
        virtual ~IPossessable() = default;

        virtual void OnPossessed() = 0;

        virtual void OnUnpossessed() = 0;

        virtual void ApplyPlayerCommand(const PlayerCommand &command, float delta_time) = 0;
    };
} // namespace Game