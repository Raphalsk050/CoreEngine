#pragma once

#include <cstdint>

#include "core/network/interpolation/snapshot_interpolation_buffer.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    struct SnapshotInterpolationStats {
        std::uint64_t samples_inserted = 0;
        std::uint64_t samples_dropped = 0;
        std::uint64_t entities_sampled = 0;
    };

    /**
     * @brief Applies remote snapshot interpolation policy.
     *
     * Responsibility: keep interpolation delay and extrapolation accounting
     * outside gameplay simulation and transport code.
     */
    class SnapshotInterpolationSystem {
    public:
        void Reset() noexcept;

        [[nodiscard]] bool PushSample(SnapshotInterpolationBuffer<32> &buffer,
                                      const SnapshotSample &sample) noexcept;

        [[nodiscard]] bool Sample(const SnapshotInterpolationBuffer<32> &buffer,
                                  double synchronized_server_time,
                                  SnapshotSample &out_sample) noexcept;

        void SetInterpolationDelay(double delay_seconds) noexcept;

        [[nodiscard]] double InterpolationDelay() const noexcept {
            return interpolation_delay_seconds_;
        }

        [[nodiscard]] const SnapshotInterpolationStats &Stats() const noexcept {
            return stats_;
        }

    private:
        SnapshotInterpolationStats stats_;
        double interpolation_delay_seconds_ = 0.1;
    };
} // namespace CoreEngine
