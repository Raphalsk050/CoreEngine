#pragma once

#include "core/simulation/fixed_tick_clock.h"

namespace CoreEngine {
    /**
     * @brief Owns per-frame fixed simulation stepping.
     *
     * Responsibility: expose fixed-tick frames to runtime and gameplay without
     * mixing simulation cadence with render presentation cadence.
     */
    class SimulationScheduler {
    public:
        void Configure(const FixedTickClockDesc &desc) noexcept;

        void Reset() noexcept;

        void BeginFrame(float frame_delta_seconds) noexcept;

        [[nodiscard]] bool ConsumeFixedFrame(SimulationFrame &out_frame) noexcept;

        [[nodiscard]] const SimulationFrame &LastFrame() const noexcept {
            return last_frame_;
        }

        [[nodiscard]] FixedTickClock &Clock() noexcept {
            return clock_;
        }

        [[nodiscard]] const FixedTickClock &Clock() const noexcept {
            return clock_;
        }

    private:
        FixedTickClock clock_;
        SimulationFrame last_frame_{};
    };
} // namespace CoreEngine
