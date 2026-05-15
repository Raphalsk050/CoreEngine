#pragma once

#include <cstdint>

#include "core/simulation/simulation_frame.h"

namespace CoreEngine {
    struct FixedTickClockDesc {
        std::uint32_t tick_rate = 60;
        std::uint32_t snapshot_rate = 20;
        double max_accumulated_seconds = 0.25;
    };

    /**
     * @brief Converts variable frame time into bounded fixed simulation ticks.
     *
     * Responsibility: own simulation tick cadence and keep gameplay simulation
     * independent from render frame delta.
     */
    class FixedTickClock {
    public:
        void Configure(const FixedTickClockDesc &desc) noexcept;

        void Reset() noexcept;

        void AddFrameDelta(double delta_seconds) noexcept;

        [[nodiscard]] bool ConsumeTick(SimulationFrame &out_frame) noexcept;

        [[nodiscard]] bool ShouldSendSnapshot() const noexcept;

        [[nodiscard]] std::uint32_t CurrentTick() const noexcept {
            return tick_;
        }

        [[nodiscard]] float FixedDeltaTime() const noexcept {
            return fixed_delta_time_;
        }

        [[nodiscard]] float InterpolationAlpha() const noexcept;

        [[nodiscard]] std::uint32_t SnapshotIntervalTicks() const noexcept {
            return snapshot_interval_ticks_;
        }

    private:
        std::uint32_t tick_rate_ = 60;
        std::uint32_t snapshot_rate_ = 20;
        std::uint32_t snapshot_interval_ticks_ = 3;
        std::uint32_t tick_ = 0;
        float fixed_delta_time_ = 1.0f / 60.0f;
        double accumulated_seconds_ = 0.0;
        double max_accumulated_seconds_ = 0.25;
    };
} // namespace CoreEngine
