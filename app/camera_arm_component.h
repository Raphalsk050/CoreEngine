#pragma once
#include "core/math/math.h"

namespace Game {
    struct CameraArmComponent {
        float camera_arm_length = 1.5f;
        CoreEngine::Math::Vec3 camera_position_offset{0.0f, 0.0f, 0.0f};
        CoreEngine::Math::Vec3 camera_target_offset{0.0f, 0.0f, 0.0f};
    };
} // namespace Game
