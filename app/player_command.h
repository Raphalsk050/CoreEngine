#pragma once

#include "core/math/math.h"

namespace Game {
    struct PlayerCommand {
        CoreEngine::Math::Vec2 movement_input{0.0f, 0.0f};
        CoreEngine::Math::Vec3 world_move{0.0f, 0.0f, 0.0f};
        bool jump_pressed = false;
        bool run_held = false;
    };
} // namespace Game
