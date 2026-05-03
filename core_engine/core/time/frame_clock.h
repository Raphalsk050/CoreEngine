#pragma once

#include <chrono>
#include <cstdint>

namespace CoreEngine {
    class FrameClock final {
    public:
        using Clock = std::chrono::steady_clock;

        FrameClock();

        void Reset() noexcept;

        [[nodiscard]] float TickSeconds() noexcept;

        [[nodiscard]] double TotalSeconds() const noexcept;

        [[nodiscard]] std::uint64_t FrameIndex() const noexcept;

    private:
        Clock::time_point start_;
        Clock::time_point previous_;
        Clock::time_point current_;
        std::uint64_t frame_index_ = 0;
    };
} // namespace CoreEngine
