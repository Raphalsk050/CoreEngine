#include "core/network/interpolation/snapshot_interpolation_system.h"

#include <algorithm>

namespace CoreEngine {
    void SnapshotInterpolationSystem::Reset() noexcept {
        stats_ = {};
        interpolation_delay_seconds_ = 0.1;
    }

    bool SnapshotInterpolationSystem::PushSample(SnapshotInterpolationBuffer<32> &buffer,
                                                 const SnapshotSample &sample) noexcept {
        const bool inserted = buffer.Push(sample);
        if (inserted) {
            ++stats_.samples_inserted;
        } else {
            ++stats_.samples_dropped;
        }
        return inserted;
    }

    bool SnapshotInterpolationSystem::Sample(const SnapshotInterpolationBuffer<32> &buffer,
                                             double synchronized_server_time,
                                             SnapshotSample &out_sample) noexcept {
        const bool sampled = buffer.Sample(synchronized_server_time - interpolation_delay_seconds_, out_sample);
        if (sampled) {
            ++stats_.entities_sampled;
        }
        return sampled;
    }

    void SnapshotInterpolationSystem::SetInterpolationDelay(double delay_seconds) noexcept {
        interpolation_delay_seconds_ = std::clamp(delay_seconds, 0.02, 0.25);
    }
} // namespace CoreEngine
