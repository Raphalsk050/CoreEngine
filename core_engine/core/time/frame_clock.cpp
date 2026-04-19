#include "core/time/frame_clock.h"

namespace CoreEngine {
    FrameClock::FrameClock() {
        Reset();
    }

    void FrameClock::Reset() noexcept {
        const Clock::time_point now = Clock::now();

        start_ = now;
        previous_ = now;
        frame_index_ = 0;
    }

    float FrameClock::TickSeconds() noexcept {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<float> delta = now - previous_;

        previous_ = now;
        ++frame_index_;

        return delta.count();
    }

    double FrameClock::TotalSeconds() const noexcept {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<double> total = now - start_;

        return total.count();
    }

    std::uint64_t FrameClock::FrameIndex() const noexcept {
        return frame_index_;
    }
} // namespace CoreEngine
