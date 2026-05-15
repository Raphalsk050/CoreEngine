#include "core/simulation/simulation_scheduler.h"

namespace CoreEngine {
    void SimulationScheduler::Configure(const FixedTickClockDesc &desc) noexcept {
        clock_.Configure(desc);
        last_frame_.fixed_delta_time = clock_.FixedDeltaTime();
    }

    void SimulationScheduler::Reset() noexcept {
        clock_.Reset();
        last_frame_ = SimulationFrame{.fixed_delta_time = clock_.FixedDeltaTime()};
    }

    void SimulationScheduler::BeginFrame(float frame_delta_seconds) noexcept {
        clock_.AddFrameDelta(frame_delta_seconds);
    }

    bool SimulationScheduler::ConsumeFixedFrame(SimulationFrame &out_frame) noexcept {
        if (!clock_.ConsumeTick(out_frame)) {
            return false;
        }

        last_frame_ = out_frame;
        return true;
    }
} // namespace CoreEngine
