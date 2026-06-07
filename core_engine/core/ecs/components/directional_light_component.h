#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    /**
     * @brief Stores one physically parameterized directional light for scene rendering.
     *
     * Responsibility: keep direct-light authoring in world-space units without
     * coupling gameplay objects to render-backend resource details.
     */
    struct DirectionalLightComponent {
        Math::Vec3 direction{0.f, -1.f, 0.f};
        float illuminance_lux = 0.f;
        Math::Vec3 color{1.f, 1.f, 1.f};
        bool enabled = true;
    };
} // namespace CoreEngine
