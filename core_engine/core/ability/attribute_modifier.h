#pragma once
#include <cstdint>

#include "core/ability/character_attributes.h"

namespace CoreEngine {
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
