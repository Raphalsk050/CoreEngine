#pragma once

#include <cstdint>

#include "core/math/math.h"

namespace CoreEngine {
    using ReplicatedComponentTypeId = std::uint16_t;

    enum class AuthorityPolicy : std::uint8_t {
        ServerOnly,
        OwnerPredictedServerAuthoritative,
        OwnerOnlyPrivate,
        PublicInterpolated,
        EventReliable,
    };

    enum class ReplicationReliability : std::uint8_t {
        UnreliableSnapshot,
        ReliableEvent,
    };

    enum class ReplicatedComponentFlags : std::uint32_t {
        None = 0,
        Interpolated = 1u << 0u,
        Predicted = 1u << 1u,
        OwnerOnly = 1u << 2u,
        Critical = 1u << 3u,
    };

    struct PlayerMovementStateComponent {
        Math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        bool grounded = true;
        bool crouching = false;
        bool sprinting = false;
        std::uint32_t last_processed_input_sequence = 0;
    };

    inline constexpr ReplicatedComponentTypeId kNetworkIdentityComponentTypeId = 1;
    inline constexpr ReplicatedComponentTypeId kNetworkTransformComponentTypeId = 2;
    inline constexpr ReplicatedComponentTypeId kPlayerMovementStateComponentTypeId = 3;
    inline constexpr ReplicatedComponentTypeId kFirstGameReplicatedComponentTypeId = 100;
} // namespace CoreEngine
