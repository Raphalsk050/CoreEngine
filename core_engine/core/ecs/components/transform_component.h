#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    struct TransformComponent {
        Math::Vec3 position = Math::Vec3(0.0, 0.0, 0.0);
        Math::Quat rotation = Math::Quat(1.0, 0.0, 0.0, 0.0);
        Math::Vec3 scale    = Math::Vec3(1.0, 1.0, 1.0);

        explicit TransformComponent(Math::Vec3 position = Math::Vec3(0.0, 0.0, 0.0),
                                    Math::Quat rotation = Math::Quat(1.0, 0.0, 0.0, 0.0),
                                    Math::Vec3 scale    = Math::Vec3(1.0, 1.0, 1.0))
            : position(position), rotation(rotation), scale(scale) {}

        [[nodiscard]] Math::Mat4 WorldMatrix() const {
            return Math::ComposeTransform(position, rotation, scale);
        }
    };
} // namespace CoreEngine
