#pragma once

#include <cstdint>

namespace Game {
    enum class MovementType : uint8_t {
        Crouch = 0,
        Walk,
        Run,
    };
} // namespace Game