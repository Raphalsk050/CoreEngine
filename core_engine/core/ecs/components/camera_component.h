#pragma once

#include <cstdint>

namespace CoreEngine {
    enum class CameraProjectionType : std::uint8_t {
        Perspective,
        Orthographic
    };

    enum class CameraAspectMode : std::uint8_t {
        RenderSurface,
        Fixed
    };

    struct CameraComponent {
        CameraProjectionType projection_type = CameraProjectionType::Perspective;
        CameraAspectMode aspect_mode = CameraAspectMode::RenderSurface;

        float fov_y_degrees = 60.0f;
        float orthographic_height = 10.0f;

        float near_z = 0.01f;
        float far_z = 1000.0f;

        float fixed_aspect_ratio = 16.0f / 9.0f;

        int priority = 0;
        bool enabled = true;
    };
} // namespace CoreEngine
