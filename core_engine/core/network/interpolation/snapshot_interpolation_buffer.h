#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/math/math.h"

namespace CoreEngine {
    struct SnapshotSample {
        std::uint32_t server_tick = 0;
        double server_time = 0.0;
        Math::Vec3 position{0.0f, 0.0f, 0.0f};
        Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Math::Vec3 scale{1.0f, 1.0f, 1.0f};
        Math::Vec3 linear_velocity{0.0f, 0.0f, 0.0f};
        std::uint32_t component_mask = 0;
    };

    [[nodiscard]] inline Math::Vec3 HermitePosition(const SnapshotSample &previous,
                                                    const SnapshotSample &next,
                                                    float alpha) noexcept {
        const float t2 = alpha * alpha;
        const float t3 = t2 * alpha;
        const float h00 = (2.0f * t3) - (3.0f * t2) + 1.0f;
        const float h10 = t3 - (2.0f * t2) + alpha;
        const float h01 = (-2.0f * t3) + (3.0f * t2);
        const float h11 = t3 - t2;
        const float duration = static_cast<float>(next.server_time - previous.server_time);

        return (previous.position * h00) +
               (previous.linear_velocity * (duration * h10)) +
               (next.position * h01) +
               (next.linear_velocity * (duration * h11));
    }

    /**
     * @brief Fixed-size temporal buffer for remote entity snapshot samples.
     *
     * Responsibility: store ordered remote samples without per-frame allocation
     * so presentation can interpolate or briefly extrapolate visual state.
     */
    template<std::size_t Capacity = 32>
    class SnapshotInterpolationBuffer {
    public:
        void Reset() noexcept {
            count_ = 0;
            start_ = 0;
            newest_tick_ = 0;
        }

        bool Push(const SnapshotSample &sample) noexcept {
            if (sample.server_tick <= newest_tick_ && count_ > 0) {
                return false;
            }

            newest_tick_ = sample.server_tick;
            const std::size_t index = (start_ + count_) % Capacity;
            samples_[index] = sample;
            if (count_ < Capacity) {
                ++count_;
            } else {
                start_ = (start_ + 1u) % Capacity;
            }
            return true;
        }

        [[nodiscard]] bool Sample(double render_time, SnapshotSample &out_sample) const noexcept {
            if (count_ == 0) {
                return false;
            }

            const SnapshotSample &oldest = At(0);
            if (render_time <= oldest.server_time || count_ == 1) {
                out_sample = oldest;
                return true;
            }

            for (std::size_t i = 1; i < count_; ++i) {
                const SnapshotSample &previous = At(i - 1u);
                const SnapshotSample &next = At(i);
                if (render_time <= next.server_time) {
                    const double span = next.server_time - previous.server_time;
                    const float alpha = span > 0.0 ? static_cast<float>((render_time - previous.server_time) / span) : 0.0f;
                    out_sample = SnapshotSample{
                        .server_tick = next.server_tick,
                        .server_time = render_time,
                        .position = HermitePosition(previous, next, alpha),
                        .rotation = Math::Slerp(previous.rotation, next.rotation, alpha),
                        .scale = Math::Lerp(previous.scale, next.scale, alpha),
                        .linear_velocity = Math::Lerp(previous.linear_velocity, next.linear_velocity, alpha),
                        .component_mask = next.component_mask,
                    };
                    return true;
                }
            }

            out_sample = At(count_ - 1u);
            constexpr double kMaxExtrapolationSeconds = 0.25;
            const double extrapolation = render_time - out_sample.server_time;
            if (extrapolation > 0.0 && extrapolation <= kMaxExtrapolationSeconds) {
                out_sample.position += out_sample.linear_velocity * static_cast<float>(extrapolation);
                out_sample.server_time = render_time;
            }
            return true;
        }

        [[nodiscard]] std::size_t Count() const noexcept {
            return count_;
        }

    private:
        [[nodiscard]] const SnapshotSample &At(std::size_t index) const noexcept {
            return samples_[(start_ + index) % Capacity];
        }

        std::array<SnapshotSample, Capacity> samples_{};
        std::size_t start_ = 0;
        std::size_t count_ = 0;
        std::uint32_t newest_tick_ = 0;
    };
} // namespace CoreEngine
