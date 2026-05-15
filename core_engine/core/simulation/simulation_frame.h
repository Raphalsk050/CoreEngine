#pragma once

#include <cstdint>

namespace CoreEngine {
    struct SimulationFrame {
        std::uint32_t tick = 0;
        float fixed_delta_time = 1.0f / 60.0f;
        double simulation_time = 0.0;
        float interpolation_alpha = 0.0f;
    };
} // namespace CoreEngine
