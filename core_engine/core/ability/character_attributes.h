#pragma once
#include <cstdint>

namespace TopDownGame {
    enum class AttributeType : std::uint8_t {
        Life,
        Shield,
        Stamina,
        WalkSpeed,
        RunSpeed,
        CrouchSpeed,
        JumpHeight,
    };
}
