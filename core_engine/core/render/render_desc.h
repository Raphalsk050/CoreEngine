#pragma once

#include <cstdint>

#include "core/render/frame_buffer.h"
#include "core/render/render_backend_type.h"
#include "core/render/render_clear_color.h"

namespace CoreEngine {
    enum class PbrQualityPreset : std::uint32_t {
        Low,
        Medium,
        High,
        Ultra,
        Custom,
    };

    /**
     * @brief Controls cascaded directional shadow fitting and stability.
     */
    struct PbrCascadeSettings {
        float split_lambda = 0.65f;
        float bounds_padding = 1.10f;
        float caster_depth_padding = 2.0f;
        bool texel_snap = true;
    };

    struct PbrShadowSettings {
        bool directional_shadows = true;
        bool point_shadows = true;
        std::uint32_t cascade_count = 4;
        PbrCascadeSettings cascades{};
        std::uint32_t directional_shadow_resolution = 2048;
        float directional_shadow_distance = 120.f;
        float directional_shadow_bias = 0.0015f;
        float directional_shadow_normal_bias = 0.02f;
        std::uint32_t directional_shadow_pcf_radius = 2;
        std::uint32_t max_shadowed_point_lights = 2;
        std::uint32_t point_shadow_resolution = 1024;
        float point_shadow_bias = 0.005f;
        float point_shadow_normal_bias = 0.03f;
        std::uint32_t point_shadow_pcf_radius = 1;
    };

    struct PbrIblSettings {
        bool enabled = true;
        std::uint32_t environment_cube_resolution = 512;
        std::uint32_t irradiance_resolution = 64;
        std::uint32_t prefiltered_specular_resolution = 128;
        std::uint32_t prefiltered_specular_mip_count = 5;
        std::uint32_t brdf_lut_resolution = 256;
    };

    /**
     * @brief Controls PBR feature cost and debug visibility from one render configuration.
     */
    struct PbrRenderSettings {
        PbrQualityPreset preset = PbrQualityPreset::Medium;
        PbrShadowSettings shadows{};
        PbrIblSettings ibl{};
        bool visual_debug = true;

        [[nodiscard]] static PbrRenderSettings Low();

        [[nodiscard]] static PbrRenderSettings Medium();

        [[nodiscard]] static PbrRenderSettings High();

        [[nodiscard]] static PbrRenderSettings Ultra();
    };

    inline PbrRenderSettings PbrRenderSettings::Low() {
        PbrRenderSettings settings;
        settings.preset = PbrQualityPreset::Low;
        settings.shadows.cascade_count = 3;
        settings.shadows.cascades.split_lambda = 0.55f;
        settings.shadows.cascades.bounds_padding = 1.15f;
        settings.shadows.directional_shadow_resolution = 1024;
        settings.shadows.directional_shadow_distance = 80.f;
        settings.shadows.directional_shadow_pcf_radius = 1;
        settings.shadows.max_shadowed_point_lights = 1;
        settings.shadows.point_shadow_resolution = 512;
        settings.shadows.point_shadow_pcf_radius = 1;
        settings.ibl.environment_cube_resolution = 256;
        settings.ibl.irradiance_resolution = 32;
        settings.ibl.prefiltered_specular_resolution = 64;
        settings.ibl.prefiltered_specular_mip_count = 4;
        settings.ibl.brdf_lut_resolution = 128;
        return settings;
    }

    inline PbrRenderSettings PbrRenderSettings::Medium() { return {}; }

    inline PbrRenderSettings PbrRenderSettings::High() {
        PbrRenderSettings settings;
        settings.preset = PbrQualityPreset::High;
        settings.shadows.cascades.split_lambda = 0.70f;
        settings.shadows.cascades.bounds_padding = 1.08f;
        settings.shadows.directional_shadow_resolution = 4096;
        settings.shadows.directional_shadow_distance = 180.f;
        settings.shadows.directional_shadow_pcf_radius = 3;
        settings.shadows.max_shadowed_point_lights = 4;
        settings.shadows.point_shadow_resolution = 1024;
        settings.shadows.point_shadow_pcf_radius = 2;
        settings.ibl.environment_cube_resolution = 1024;
        settings.ibl.irradiance_resolution = 96;
        settings.ibl.prefiltered_specular_resolution = 256;
        settings.ibl.prefiltered_specular_mip_count = 6;
        settings.ibl.brdf_lut_resolution = 256;
        return settings;
    }

    inline PbrRenderSettings PbrRenderSettings::Ultra() {
        PbrRenderSettings settings = High();
        settings.preset = PbrQualityPreset::Ultra;
        settings.shadows.cascades.split_lambda = 0.75f;
        settings.shadows.cascades.bounds_padding = 1.05f;
        settings.shadows.directional_shadow_resolution = 4096;
        settings.shadows.directional_shadow_distance = 260.f;
        settings.shadows.directional_shadow_pcf_radius = 4;
        settings.shadows.max_shadowed_point_lights = 6;
        settings.shadows.point_shadow_resolution = 2048;
        settings.shadows.point_shadow_pcf_radius = 3;
        settings.ibl.environment_cube_resolution = 1024;
        settings.ibl.prefiltered_specular_resolution = 512;
        settings.ibl.prefiltered_specular_mip_count = 7;
        settings.ibl.brdf_lut_resolution = 512;
        return settings;
    }

    enum class ToneMappingOperator : std::uint32_t {
        None = 0,
        Reinhard = 1,
        AcesFilmic = 2,
    };

    /**
     * @brief Controls screen-space mapping from scene-linear HDR color to swapchain output.
     */
    struct PostProcessDesc {
        float exposure = 1.0f;
        ToneMappingOperator tone_mapping = ToneMappingOperator::AcesFilmic;
    };

    struct RenderDesc {
        RenderBackendType backend = RenderBackendType::None;
        RenderClearColor clear_color{};
        bool vsync = true;
        bool enable_imgui = true;
        FrameBufferFormat scene_color_format = FrameBufferFormat::RGBA16Float;
        PostProcessDesc post_process{};
        PbrRenderSettings pbr{};
        // Explicit swapchain dimensions.
        // Must match the window size at initialization time.
        // Passing 0 lets the backend infer from the HWND (may fail on some drivers).
        int width = 0;
        int height = 0;
    };
} // namespace CoreEngine
