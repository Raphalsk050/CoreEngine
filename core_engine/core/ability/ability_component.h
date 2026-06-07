#pragma once
#include <cstddef>

#include "core/ability/attribute.h"
#include "core/ability/base_attributes.h"
#include "core/ability/effect.h"

namespace CoreEngine {
    constexpr std::size_t AbilityAttributeCapacity = 50;
    constexpr std::size_t AbilityEffectCapacity = 50;

    constexpr std::size_t LifeAttributeIndex = 0;
    constexpr std::size_t ShieldAttributeIndex = 1;
    constexpr std::size_t StaminaAttributeIndex = 2;
    constexpr std::size_t RunSpeedAttributeIndex = 3;
    constexpr std::size_t WalkSpeedAttributeIndex = 4;
    constexpr std::size_t JumpHeightAttributeIndex = 5;

    struct AbilityComponent {
        Attribute attributes[AbilityAttributeCapacity]{
                BaseAttributes::life_attribute,
                BaseAttributes::shield_attribute,
                BaseAttributes::stamina_attribute,
                BaseAttributes::run_speed_attribute,
                BaseAttributes::walk_speed_attribute,
                BaseAttributes::jump_height_attribute,
        };
        Effect effects[AbilityEffectCapacity]{};
    };
} // namespace CoreEngine
