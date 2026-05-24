#pragma once

#include <cstdint>
#include <vector>

#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/replicated_state_types.h"

namespace Game {
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
        CoreEngine::NetworkEntityId original_owner_player = 0;
        CoreEngine::NetworkEntityId current_carrier_player = 0;
        bool on_ground = false;
        bool extracted = false;
    };

    struct BountyBeaconCarrierComponent {
        std::vector<CoreEngine::NetworkEntityId> carried_beacons;
    };

    struct CaptureStateComponent {
        CoreEngine::NetworkEntityId captor_player = 0;
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
        CoreEngine::NetworkEntityId target_player = 0;
        CoreEngine::NetworkEntityId hunter_player = 0;
        CoreEngine::NetworkEntityId required_beacon = 0;
        TargetObjectiveState state = TargetObjectiveState::HuntAssignedTarget;
    };

    struct AIStateComponent {
        AIState state = AIState::Idle;
        CoreEngine::NetworkEntityId target_entity = 0;
    };

    inline constexpr CoreEngine::ReplicatedComponentTypeId kHealthComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 0;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kArmorSegmentsComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 1;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kInventoryComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 2;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kEquipmentComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 3;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kBountyBeaconComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 4;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kBountyBeaconCarrierComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 5;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kCaptureStateComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 6;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kExtractionStateComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 7;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kTargetAssignmentComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 8;
    inline constexpr CoreEngine::ReplicatedComponentTypeId kAIStateComponentTypeId =
        CoreEngine::kFirstGameReplicatedComponentTypeId + 9;
} // namespace Game
