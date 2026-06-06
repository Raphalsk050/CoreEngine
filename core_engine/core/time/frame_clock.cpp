#include "core/time/frame_clock.h"

namespace CoreEngine {
    FrameClock::FrameClock() { Reset(); }

    void FrameClock::Reset() noexcept {
        const Clock::time_point now = Clock::now();

        start_ = now;
        previous_ = now;
        current_ = now;
        frame_index_ = 0;
    }

    float FrameClock::TickSeconds() noexcept {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<float> delta = now - previous_;

        current_ = now;
        previous_ = now;
        ++frame_index_;

        return delta.count();
    }

    double FrameClock::TotalSeconds() const noexcept {
        const std::chrono::duration<double> total = current_ - start_;

        return total.count();
    }

    std::uint64_t FrameClock::FrameIndex() const noexcept { return frame_index_; }
} // namespace CoreEngine
