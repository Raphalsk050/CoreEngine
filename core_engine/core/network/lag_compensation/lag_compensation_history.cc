#include "core/network/lag_compensation/lag_compensation_history.h"

#include <limits>

namespace CoreEngine {
    void LagCompensationHistory::Reset() noexcept {
        next_ = 0;
        count_ = 0;
    }

    void LagCompensationHistory::Store(const LagCompensationSample &sample) noexcept {
        samples_[next_] = sample;
        next_ = (next_ + 1u) % kCapacity;
        if (count_ < kCapacity) {
            ++count_;
        }
    }

    bool LagCompensationHistory::FindClosest(NetworkEntityId entity_id,
                                             double server_time,
                                             LagCompensationSample &out_sample) const noexcept {
        double best_delta = std::numeric_limits<double>::max();
        bool found = false;

        for (std::size_t i = 0; i < count_; ++i) {
            const LagCompensationSample &sample = samples_[i];
            if (sample.entity_id != entity_id) {
                continue;
            }

            const double delta = sample.server_time > server_time
                                     ? sample.server_time - server_time
                                     : server_time - sample.server_time;
            if (delta < best_delta) {
                best_delta = delta;
                out_sample = sample;
                found = true;
            }
        }

        return found;
    }
} // namespace CoreEngine
