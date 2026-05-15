#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/math/math.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    struct LagCompensationSample {
        NetworkEntityId entity_id = 0;
        std::uint32_t server_tick = 0;
        double server_time = 0.0;
        Math::Vec3 position{0.0f, 0.0f, 0.0f};
        Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Math::Vec3 half_extents{0.5f, 0.9f, 0.5f};
        bool alive = true;
        bool captured = false;
    };

    /**
     * @brief Stores recent authoritative hitbox samples for server rewind checks.
     *
     * Responsibility: support bounded lag compensation for combat, capture,
     * and interaction validation without mutating current simulation state.
     */
    class LagCompensationHistory {
    public:
        void Reset() noexcept;

        void Store(const LagCompensationSample &sample) noexcept;

        [[nodiscard]] bool FindClosest(NetworkEntityId entity_id,
                                       double server_time,
                                       LagCompensationSample &out_sample) const noexcept;

    private:
        static constexpr std::size_t kCapacity = 512;

        std::array<LagCompensationSample, kCapacity> samples_{};
        std::size_t next_ = 0;
        std::size_t count_ = 0;
    };
} // namespace CoreEngine
