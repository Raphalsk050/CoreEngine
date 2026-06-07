#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    /**
     * @brief Stores physically based point-light authoring data for scene rendering.
     *
     * Responsibility: keep local punctual light parameters independent from render
     * backend buffers; position comes from the entity TransformComponent.
     */
    struct PointLightComponent {
        Math::Vec3 color{1.f, 1.f, 1.f};
        float luminous_intensity_cd = 0.f;
        float range = 10.f;
        bool enabled = true;
    };
} // namespace CoreEngine
