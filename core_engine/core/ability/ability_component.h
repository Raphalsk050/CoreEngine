#pragma once
#include "core/ability/attribute.h"
#include "core/ability/effect.h"

namespace TopDownGame {
    struct AbilityComponent {
        Attribute attributes[50];
        Effect effects[50];
    };
} // namespace TopDownGame
