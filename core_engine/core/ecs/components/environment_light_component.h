#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    /**
     * @brief Stores scene-wide environment lighting for PBR shading.
     *
     * Responsibility: provide backend-independent ambient irradiance/radiance terms
     * that can later be replaced or augmented by texture-based IBL resources.
     */
    struct EnvironmentLightComponent {
        Math::Vec3 diffuse_irradiance{0.f, 0.f, 0.f};
        float intensity = 1.f;
        Math::Vec3 specular_radiance{0.f, 0.f, 0.f};
        float specular_intensity = 1.f;
        bool enabled = true;
    };
} // namespace CoreEngine
