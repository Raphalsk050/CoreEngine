#include "gameplay/systems/bounty_beacon_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/math/math.h"
#include "core/network/multiplayer_system.h"
#include "core/network/replication/network_identity_component.h"

#include <algorithm>

namespace Game {
    namespace {
        constexpr float kBeaconPickupRadius = 2.5f;

    }

    void BountyBeaconSystem::Reset() {
        beacons_.clear();
    }

    void BountyBeaconSystem::RegisterBeacon(const CoreEngine::BountyBeaconComponent &beacon) {
        beacons_.push_back(beacon);
    }

    void BountyBeaconSystem::DropBeacon(CoreEngine::NetworkEntityId original_owner,
                                        CoreEngine::NetworkEntityId carrier) noexcept {
        for (CoreEngine::BountyBeaconComponent &beacon: beacons_) {
            if (beacon.original_owner_player == original_owner && beacon.current_carrier_player == carrier) {
                beacon.current_carrier_player = 0;
                beacon.on_ground = true;
                beacon.extracted = false;
                return;
            }
        }
    }

    void BountyBeaconSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (context.multiplayer.Role() == CoreEngine::NetworkRole::Client) {
            return;
        }

        auto player_view = context.world.View<CoreEngine::NetworkIdentityComponent,
                                              CoreEngine::HealthComponent,
                                              CoreEngine::TransformComponent>();
        for (const entt::entity entity: player_view) {
            const auto &identity = player_view.get<CoreEngine::NetworkIdentityComponent>(entity);
            if (!identity.IsNetworked()) {
                continue;
            }

            if (!context.world.HasComponent<CoreEngine::BountyBeaconComponent>(entity)) {
                context.world.Emplace<CoreEngine::BountyBeaconComponent>(
                    entity,
                    CoreEngine::BountyBeaconComponent{
                        .original_owner_player = identity.network_id,
                        .current_carrier_player = identity.network_id,
                        .on_ground = false,
                        .extracted = false,
                    });
            }
            if (!context.world.HasComponent<CoreEngine::BountyBeaconCarrierComponent>(entity)) {
                context.world.Emplace<CoreEngine::BountyBeaconCarrierComponent>(entity);
            }

            auto &health = player_view.get<CoreEngine::HealthComponent>(entity);
            auto &beacon = context.world.GetComponent<CoreEngine::BountyBeaconComponent>(entity);
            if (!health.alive && !beacon.on_ground && !beacon.extracted) {
                beacon.current_carrier_player = 0;
                beacon.on_ground = true;
            }
        }

        for (const CoreEngine::QueuedPlayerInputCommand &queued: context.multiplayer.InputCommands()) {
            if (!queued.command.IsButtonDown(CoreEngine::PlayerInputButton::Interact)) {
                continue;
            }

            CoreEngine::Node player = context.multiplayer.FindNode(queued.player_network_id);
            if (!player.IsValid()) {
                continue;
            }

            const auto *player_transform = player.TryGetComponent<CoreEngine::TransformComponent>();
            const auto *player_identity = player.TryGetComponent<CoreEngine::NetworkIdentityComponent>();
            auto *carrier = player.TryGetComponent<CoreEngine::BountyBeaconCarrierComponent>();
            if (player_transform == nullptr || player_identity == nullptr || carrier == nullptr) {
                continue;
            }

            entt::entity best_beacon_entity = entt::null;
            float best_distance_squared = kBeaconPickupRadius * kBeaconPickupRadius;
            auto beacon_view = context.world.View<CoreEngine::BountyBeaconComponent, CoreEngine::TransformComponent>();
            for (const entt::entity entity: beacon_view) {
                const auto &beacon = beacon_view.get<CoreEngine::BountyBeaconComponent>(entity);
                if (!beacon.on_ground || beacon.extracted) {
                    continue;
                }

                const auto &beacon_transform = beacon_view.get<CoreEngine::TransformComponent>(entity);
                const float distance_squared =
                    CoreEngine::Math::LengthSquared(beacon_transform.Position() - player_transform->Position());
                if (distance_squared < best_distance_squared) {
                    best_distance_squared = distance_squared;
                    best_beacon_entity = entity;
                }
            }

            if (best_beacon_entity == entt::null) {
                continue;
            }

            auto &beacon = context.world.GetComponent<CoreEngine::BountyBeaconComponent>(best_beacon_entity);
            beacon.current_carrier_player = player_identity->network_id;
            beacon.on_ground = false;
            if (std::ranges::find(carrier->carried_beacons, beacon.original_owner_player) ==
                carrier->carried_beacons.end()) {
                carrier->carried_beacons.push_back(beacon.original_owner_player);
            }
        }
    }
} // namespace Game
