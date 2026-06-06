#pragma once
#include <functional>
#include <vector>
#include "attribute_modifier.h"

namespace TopDownGame {
    struct Effect {
        float duration;
        std::vector<AttributeModifier> attribute_modifiers;
    };

    struct ActiveEffect {
        Effect effect;
        float remaining_duration = 0.0f;
    };
} // namespace TopDownGame
