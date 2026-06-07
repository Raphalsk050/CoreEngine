#pragma once
#include "attribute.h"

namespace CoreEngine {
    struct BaseAttributes {
        inline static constexpr Attribute life_attribute{
                .type = AttributeType::Life,
                .base_value = 100.0f,
                .current_value = 100.0f,
        };
        inline static constexpr Attribute shield_attribute{
                .type = AttributeType::Shield,
                .base_value = 50.0f,
                .current_value = 50.0f,
        };
        inline static constexpr Attribute stamina_attribute{
                .type = AttributeType::Stamina,
                .base_value = 100.0f,
                .current_value = 100.0f,
        };
        inline static constexpr Attribute run_speed_attribute{
                .type = AttributeType::RunSpeed,
                .base_value = 10.0f,
                .current_value = 10.0f,
        };
        inline static constexpr Attribute walk_speed_attribute{
                .type = AttributeType::WalkSpeed,
                .base_value = 4.0f,
                .current_value = 4.0f,
        };
        inline static constexpr Attribute jump_height_attribute{
                .type = AttributeType::JumpHeight,
                .base_value = 2.0f,
                .current_value = 2.0f,
        };
    };
} // namespace CoreEngine
