#include "core/simulation/fixed_tick_clock.h"

#include <algorithm>

namespace CoreEngine {
    void FixedTickClock::Configure(const FixedTickClockDesc &desc) noexcept {
        tick_rate_ = std::max(1u, desc.tick_rate);
        snapshot_rate_ = std::clamp(desc.snapshot_rate, 1u, tick_rate_);
        snapshot_interval_ticks_ = std::max(1u, tick_rate_ / snapshot_rate_);
        fixed_delta_time_ = 1.0f / static_cast<float>(tick_rate_);
        max_accumulated_seconds_ = std::max(static_cast<double>(fixed_delta_time_), desc.max_accumulated_seconds);
    }

    void FixedTickClock::Reset() noexcept {
        tick_ = 0;
        accumulated_seconds_ = 0.0;
    }

    void FixedTickClock::AddFrameDelta(double delta_seconds) noexcept {
        if (delta_seconds <= 0.0) {
            return;
        }

        accumulated_seconds_ = std::min(accumulated_seconds_ + delta_seconds, max_accumulated_seconds_);
    }

    bool FixedTickClock::ConsumeTick(SimulationFrame &out_frame) noexcept {
        if (accumulated_seconds_ + 1.0e-9 < fixed_delta_time_) {
            return false;
        }

        accumulated_seconds_ -= fixed_delta_time_;
        ++tick_;
        out_frame = SimulationFrame{
            .tick = tick_,
            .fixed_delta_time = fixed_delta_time_,
            .simulation_time = static_cast<double>(tick_) * static_cast<double>(fixed_delta_time_),
            .interpolation_alpha = InterpolationAlpha(),
        };
        return true;
    }

    bool FixedTickClock::ShouldSendSnapshot() const noexcept {
        return tick_ != 0 && (tick_ % snapshot_interval_ticks_) == 0;
    }

    float FixedTickClock::InterpolationAlpha() const noexcept {
        if (fixed_delta_time_ <= 0.0f) {
            return 0.0f;
        }

        return static_cast<float>(std::clamp(accumulated_seconds_ / fixed_delta_time_, 0.0, 1.0));
    }
} // namespace CoreEngine
