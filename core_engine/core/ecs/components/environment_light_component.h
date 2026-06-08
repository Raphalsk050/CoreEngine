#pragma once

#include <string>

#include "core/math/math.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    /**
     * @brief Stores scene-wide environment lighting for PBR shading.
     *
     * Responsibility: provide backend-independent ambient irradiance/radiance terms
     * and optional precomputed IBL assets for static environment lighting.
     */
    struct EnvironmentLightComponent {
        std::string hdr_equirectangular_path{};
        std::string precomputed_environment_cube_path{};
        std::string precomputed_irradiance_cube_path{};
        std::string precomputed_prefiltered_specular_cube_path{};
        std::string precomputed_brdf_lut_path{};
        std::string precomputed_ibl_manifest_path{};
        TextureHandle environment_map{};
        Math::Vec3 diffuse_irradiance{0.f, 0.f, 0.f};
        float intensity = 1.f;
        Math::Vec3 specular_radiance{0.f, 0.f, 0.f};
        float specular_intensity = 1.f;
        bool bake_precomputed_ibl_if_missing = false;
        bool enabled = true;
    };
} // namespace CoreEngine
