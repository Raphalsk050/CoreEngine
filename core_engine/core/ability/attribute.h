#pragma once
#include "core/ability/character_attributes.h"

namespace CoreEngine {
    struct Attribute {
        AttributeType type;
        float base_value = 0.0f;
        float current_value = 0.0f;
    };
} // namespace TopDownGame
