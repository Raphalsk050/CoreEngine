#pragma once
#include <cstdint>

#include "player/character_attributes.h"

namespace TopDownGame {
    enum class ModifierOperation : std::uint8_t {
        Add,
        Multiply,
        Override,
    };

    struct AttributeModifier {
        AttributeType type;
        ModifierOperation operation;
        float magnitude;
    };
} // namespace TopDownGame
