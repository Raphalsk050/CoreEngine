#pragma once
#include "player/character_attributes.h"

namespace TopDownGame {
    struct Attribute {
        AttributeType type;
        float base_value = 0.0f;
        float current_value = 0.0f;
    };
} // namespace TopDownGame
