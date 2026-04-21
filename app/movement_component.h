#pragma once

#include "movement_type.h"

namespace Game {
    struct MovementComponent {
        float crouch_speed = 1.5f;
        float walk_speed = 3.0f;
        float run_speed = 6.0f;
        MovementType default_movement_type = MovementType::Walk;
    };
} // namespace Game
