#pragma once

#include <cstdint>

#include "core/math/math.h"

namespace CoreEngine {
    /**
     * @brief Stores remote authoritative transform state for interpolation.
     *
     * Responsibility: keep snapshot timing and transform data compact so
     * presentation code can interpolate without owning network protocol state.
     */
    struct NetworkTransformComponent {
        Math::Vec3 authoritative_position{0.0f, 0.0f, 0.0f};
        Math::Quat authoritative_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Math::Vec3 authoritative_scale{1.0f, 1.0f, 1.0f};
        std::uint32_t last_snapshot_tick = 0;
        float interpolation_delay_seconds = 0.1f;
        bool interpolation_enabled = true;
    };
} // namespace CoreEngine
