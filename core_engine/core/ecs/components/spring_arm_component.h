#pragma once

#include <cstdint>

#include "core/math/math.h"

namespace CoreEngine {
    enum class SpringArmOrientationMode : std::uint8_t {
        LocalOrbit,
        OwnerRotation,
        ViewRotation
    };

    /**
     * @brief Selects which owner rotation axes affect a camera boom.
     *
     * Responsibility: keep rotation inheritance explicit so the camera rig can
     * opt into only the axes that make sense for the controlled entity.
     */
    struct SpringArmOrientationMask {
        bool pitch = false;
        bool yaw = true;
        bool roll = false;
    };

    /**
     * @brief Configures obstruction handling for a camera boom.
     *
     * Responsibility: describe how the camera system should shorten the boom
     * when geometry blocks the line from the pivot to the desired camera point.
     */
    struct SpringArmObstructionSettings {
        bool enabled = true;
        float clearance_radius = 0.25f;
        std::uint32_t blocking_layer_mask = 0xFFFFFFFFu;
    };

    /**
     * @brief Configures camera boom smoothing.
     *
     * Responsibility: describe optional positional and rotational damping
     * without tying the component to a specific update system.
     */
    struct SpringArmSmoothingSettings {
        bool position_enabled = false;
        bool rotation_enabled = false;
        bool substep_when_needed = true;

        float position_follow_speed = 12.0f;
        float rotation_follow_speed = 12.0f;
        float max_substep_seconds = 1.0f / 60.0f;
        float max_position_error = 0.0f;
    };

    /**
     * @brief Stores frame-to-frame camera boom state.
     *
     * Responsibility: preserve the small amount of runtime state needed for
     * obstruction shortening and smoothing across frames.
     */
    struct SpringArmRuntimeState {
        Math::Vec3 desired_camera_position{0.0f};
        Math::Vec3 smoothed_camera_position{0.0f};
        Math::Quat smoothed_camera_rotation{};
        float resolved_length = 4.0f;
        bool obstruction_applied = false;
        bool initialized = false;
    };

    /**
     * @brief Describes a third-person camera boom attached to an entity.
     *
     * Responsibility: store camera boom configuration and lightweight runtime
     * state; camera systems consume this data with TransformComponent and write
     * the final camera transform.
     */
    struct SpringArmComponent {
        float rest_length = 4.0f;
        Math::Vec3 pivot_offset_local{0.0f, 1.5f, 0.0f};
        Math::Vec3 camera_offset_local{0.0f};

        float orbit_pitch_radians = Math::Deg2Rad(-15.0f);
        float orbit_yaw_radians = 0.0f;
        float orbit_roll_radians = 0.0f;

        SpringArmOrientationMode orientation_mode = SpringArmOrientationMode::LocalOrbit;
        SpringArmOrientationMask inherited_axes{};
        SpringArmObstructionSettings obstruction{};
        SpringArmSmoothingSettings smoothing{};
        SpringArmRuntimeState runtime{};
    };
} // namespace CoreEngine
