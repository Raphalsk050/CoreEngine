#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/math/math.h"
#include "core/network/replication/network_identity_component.h"

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

    enum class ArmorSegment : std::uint8_t {
        Head,
        Torso,
        LeftArm,
        RightArm,
        Legs,
    };

    enum class TargetObjectiveState : std::uint8_t {
        HuntAssignedTarget,
        RecoverBeaconOnGround,
        KillOrCaptureBeaconCarrier,
        ExtractWithBeacon,
        Completed,
    };

    enum class AIState : std::uint8_t {
        Idle,
        Patrol,
        Chase,
        Attack,
        Dead,
    };

    enum class ExtractionState : std::uint8_t {
        Closed,
        Armed,
        ShipInbound,
        BoardingOpen,
        Departed,
    };

    struct PlayerMovementStateComponent {
        Math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        bool grounded = true;
        bool crouching = false;
        bool sprinting = false;
        std::uint32_t last_processed_input_sequence = 0;
    };

    struct HealthComponent {
        float health = 100.0f;
        float max_health = 100.0f;
        bool alive = true;
        bool concussed = false;
    };

    struct ArmorPart {
        float hit_points = 0.0f;
        float max_hit_points = 0.0f;

        [[nodiscard]] bool IsDestroyed() const noexcept {
            return hit_points <= 0.0f;
        }
    };

    struct ArmorSegmentsComponent {
        ArmorPart head{50.0f, 50.0f};
        ArmorPart torso{100.0f, 100.0f};
        ArmorPart left_arm{40.0f, 40.0f};
        ArmorPart right_arm{40.0f, 40.0f};
        ArmorPart legs{60.0f, 60.0f};
    };

    struct InventoryComponent {
        std::vector<std::uint32_t> item_ids;
        std::uint16_t capacity = 24;
    };

    struct EquipmentComponent {
        std::uint32_t weapon_item_id = 0;
        std::uint32_t gadget_item_id = 0;
        std::uint8_t selected_slot = 0;
    };

    struct BountyBeaconComponent {
        NetworkEntityId original_owner_player = 0;
        NetworkEntityId current_carrier_player = 0;
        bool on_ground = false;
        bool extracted = false;
    };

    struct BountyBeaconCarrierComponent {
        std::vector<NetworkEntityId> carried_beacons;
    };

    struct CaptureStateComponent {
        NetworkEntityId captor_player = 0;
        float cast_remaining_seconds = 0.0f;
        bool capturable = false;
        bool captured = false;
    };

    struct ExtractionStateComponent {
        ExtractionState state = ExtractionState::Closed;
        float timer_seconds = 0.0f;
        bool public_event_active = false;
    };

    struct TargetAssignmentComponent {
        NetworkEntityId target_player = 0;
        NetworkEntityId hunter_player = 0;
        NetworkEntityId required_beacon = 0;
        TargetObjectiveState state = TargetObjectiveState::HuntAssignedTarget;
    };

    struct AIStateComponent {
        AIState state = AIState::Idle;
        NetworkEntityId target_entity = 0;
    };

    inline constexpr ReplicatedComponentTypeId kNetworkIdentityComponentTypeId = 1;
    inline constexpr ReplicatedComponentTypeId kNetworkTransformComponentTypeId = 2;
    inline constexpr ReplicatedComponentTypeId kPlayerMovementStateComponentTypeId = 3;
    inline constexpr ReplicatedComponentTypeId kHealthComponentTypeId = 4;
    inline constexpr ReplicatedComponentTypeId kArmorSegmentsComponentTypeId = 5;
    inline constexpr ReplicatedComponentTypeId kInventoryComponentTypeId = 6;
    inline constexpr ReplicatedComponentTypeId kEquipmentComponentTypeId = 7;
    inline constexpr ReplicatedComponentTypeId kBountyBeaconCarrierComponentTypeId = 8;
    inline constexpr ReplicatedComponentTypeId kCaptureStateComponentTypeId = 9;
    inline constexpr ReplicatedComponentTypeId kExtractionStateComponentTypeId = 10;
    inline constexpr ReplicatedComponentTypeId kTargetChainComponentTypeId = 11;
    inline constexpr ReplicatedComponentTypeId kAIStateComponentTypeId = 12;
} // namespace CoreEngine
