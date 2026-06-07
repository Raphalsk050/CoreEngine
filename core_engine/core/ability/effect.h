#pragma once
#include <functional>
#include <vector>
#include "core/ability/attribute_modifier.h"

namespace TopDownGame {
    struct Effect {
        std::vector<AttributeModifier> attribute_modifiers;
        float duration;
    };

    struct ActiveEffect {
        Effect effect;
        float remaining_duration = 0.0f;
    };
} // namespace TopDownGame
