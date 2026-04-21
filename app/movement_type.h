#pragma once
#include <cstdint>

namespace Game {
    enum class MovementType : uint8_t {
        CROUCH = 0,
        WALK = 1,
        RUN = 2,
    };
}
