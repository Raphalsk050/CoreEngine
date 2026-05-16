#include "core/network/replication/network_replicator.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/network/message_reader.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_transform_component.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/simulation/simulation_frame.h"

#include <chrono>

namespace CoreEngine {
    namespace {
        [[nodiscard]] float DurationMs(std::chrono::steady_clock::duration duration) noexcept {
            return static_cast<float>(
                std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(duration).count());
        }

        [[nodiscard]] constexpr std::uint32_t ComponentBit(ReplicatedComponentTypeId type_id) noexcept {
            return 1u << type_id;
        }
    }

    void NetworkReplicator::Initialize(NetworkSystem &network_system, World &world) {
        network_system_ = &network_system;
        world_ = &world;
        registry_.RegisterDefaultComponents();
        spawn_system_.Reset();
        entity_by_network_id_.clear();
        entity_by_network_id_.reserve(256);
        snapshot_scratch_.clear();
        snapshot_scratch_.reserve(256);
        inbound_snapshot_scratch_.clear();
        inbound_snapshot_scratch_.reserve(256);
        snapshot_sequence_ = 0;
        ticks_until_next_snapshot_ = 0;
        stats_ = {};
    }

    void NetworkReplicator::Shutdown() noexcept {
        network_system_ = nullptr;
        world_ = nullptr;
        spawn_system_.Reset();
        entity_by_network_id_.clear();
        snapshot_scratch_.clear();
        inbound_snapshot_scratch_.clear();
        snapshot_sequence_ = 0;
        ticks_until_next_snapshot_ = 0;
        stats_ = {};
    }

    void NetworkReplicator::BeginSimulationTick(const SimulationFrame &frame) noexcept {
        ApplyInboundSnapshots(frame);
    }

    void NetworkReplicator::EndSimulationTick(const SimulationFrame &frame) noexcept {
        SendOutboundSnapshots(frame);
        spawn_system_.ClearPendingEvents();
    }

    NetworkEntityId NetworkReplicator::RegisterEntity(Node node,
                                                      NetworkEntityId network_id,
                                                      PeerId owner_peer,
                                                      bool local_authority) {
        if (!node.IsValid()) {
            return 0;
        }

        if (network_id == 0) {
            network_id = spawn_system_.AllocateNetworkEntityId();
        }

        auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
        if (identity == nullptr) {
            identity = &node.AddComponent<NetworkIdentityComponent>();
        }

        identity->network_id = network_id;
        identity->owner_peer = owner_peer;
        identity->local_authority = local_authority;
        identity->replicated = true;

        if (node.TryGetComponent<NetworkTransformComponent>() == nullptr) {
            node.AddComponent<NetworkTransformComponent>();
        }

        entity_by_network_id_[network_id] = node.Handle();
        return network_id;
    }

    NetworkEntityId NetworkReplicator::RegisterEntity(Node node, PeerId owner_peer, bool local_authority) {
        return RegisterEntity(node, spawn_system_.AllocateNetworkEntityId(), owner_peer, local_authority);
    }

    void NetworkReplicator::UnregisterEntity(NetworkEntityId network_id) noexcept {
        entity_by_network_id_.erase(network_id);
    }

    Node NetworkReplicator::FindNode(NetworkEntityId network_id) const noexcept {
        if (world_ == nullptr || network_id == 0) {
            return {};
        }

        const auto it = entity_by_network_id_.find(network_id);
        if (it == entity_by_network_id_.end() || !world_->Registry().valid(it->second)) {
            return {};
        }

        return Node{it->second, world_};
    }

    bool NetworkReplicator::HasAuthority(Node node) const noexcept {
        if (!node.IsValid()) {
            return false;
        }

        const auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
        return identity != nullptr && identity->local_authority;
    }

    bool NetworkReplicator::IsOwningClient(Node node, PeerId peer) const noexcept {
        if (!node.IsValid()) {
            return false;
        }

        const auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
        return identity != nullptr && identity->owner_peer == peer;
    }

    NetworkEntityId NetworkReplicator::GetNetworkId(Node node) const noexcept {
        if (!node.IsValid()) {
            return 0;
        }

        const auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
        return identity != nullptr ? identity->network_id : 0;
    }

    void NetworkReplicator::ApplyInboundSnapshots(const SimulationFrame &frame) noexcept {
        if (network_system_ == nullptr || world_ == nullptr) {
            return;
        }

        const auto start = std::chrono::steady_clock::now();

        for (const NetworkEvent &event: network_system_->Events()) {
            if (event.type != NetworkEventType::PacketReceived ||
                event.message_type != NetMessageType::WorldSnapshot) {
                continue;
            }

            MessageReader reader(event.payload);
            NetworkSnapshotApplyResult apply_result;
            if (!snapshot_applier_.ReadTransformSnapshot(reader, inbound_snapshot_scratch_, apply_result) ||
                reader.Remaining() != 0u) {
                continue;
            }

            ApplyTransformSnapshots(inbound_snapshot_scratch_, frame);
            ++stats_.snapshots_applied;
            stats_.last_snapshot_sequence = apply_result.snapshot_sequence;
        }

        stats_.snapshot_apply_time_ms = DurationMs(std::chrono::steady_clock::now() - start);
    }

    void NetworkReplicator::SendOutboundSnapshots(const SimulationFrame &frame) noexcept {
        if (network_system_ == nullptr || world_ == nullptr ||
            network_system_->Session().Role() != NetworkRole::Host ||
            !ShouldSendSnapshot()) {
            return;
        }

        const auto start = std::chrono::steady_clock::now();
        CollectTransformSnapshots(snapshot_scratch_, frame.tick);
        if (snapshot_scratch_.empty()) {
            return;
        }

        ++snapshot_sequence_;
        for (const NetworkPeer &peer: network_system_->Session().Peers()) {
            if (peer.state != NetworkPeerState::Connected) {
                continue;
            }

            const std::uint32_t confirmed_input = network_system_->LastProcessedInputSequence(peer.id);
            network_system_->SendWorldSnapshot(peer.id,
                                               snapshot_scratch_,
                                               frame.tick,
                                               snapshot_sequence_,
                                               confirmed_input);
        }

        ++stats_.snapshots_built;
        stats_.entities_replicated += snapshot_scratch_.size();
        stats_.last_snapshot_sequence = snapshot_sequence_;
        stats_.snapshot_build_time_ms = DurationMs(std::chrono::steady_clock::now() - start);
    }

    void NetworkReplicator::CollectTransformSnapshots(std::vector<NetworkTransformSnapshot> &out_snapshots,
                                                      std::uint32_t server_tick) const {
        out_snapshots.clear();
        if (world_ == nullptr) {
            return;
        }

        auto view = world_->View<NetworkIdentityComponent, TransformComponent>();
        for (const entt::entity entity: view) {
            if (out_snapshots.size() >= kMaxSnapshotTransformsPerPacket) {
                break;
            }

            const auto &identity = view.get<NetworkIdentityComponent>(entity);
            if (!identity.IsNetworked()) {
                continue;
            }

            const auto &transform = view.get<TransformComponent>(entity);
            NetworkTransformSnapshot snapshot{
                .network_id = identity.network_id,
                .position = transform.Position(),
                .rotation = transform.Rotation(),
                .scale = transform.Scale(),
                .tick = server_tick,
            };

            if (const auto *movement = world_->TryGetComponent<PlayerMovementStateComponent>(entity);
                movement != nullptr) {
                snapshot.component_mask |= ComponentBit(kPlayerMovementStateComponentTypeId);
                snapshot.last_processed_input_sequence = movement->last_processed_input_sequence;
            }

            if (const auto *health = world_->TryGetComponent<HealthComponent>(entity); health != nullptr) {
                snapshot.component_mask |= ComponentBit(kHealthComponentTypeId);
                snapshot.health = health->health;
                snapshot.max_health = health->max_health;
                snapshot.alive = health->alive;
                snapshot.concussed = health->concussed;
            }

            if (const auto *beacon = world_->TryGetComponent<BountyBeaconComponent>(entity); beacon != nullptr) {
                snapshot.component_mask |= ComponentBit(kBountyBeaconCarrierComponentTypeId);
                snapshot.beacon_original_owner = beacon->original_owner_player;
                snapshot.beacon_carrier = beacon->current_carrier_player;
                snapshot.beacon_on_ground = beacon->on_ground;
                snapshot.beacon_extracted = beacon->extracted;
            }

            if (const auto *capture = world_->TryGetComponent<CaptureStateComponent>(entity); capture != nullptr) {
                snapshot.component_mask |= ComponentBit(kCaptureStateComponentTypeId);
                snapshot.capture_captor = capture->captor_player;
                snapshot.captured = capture->captured;
            }

            out_snapshots.push_back(snapshot);
        }
    }

    void NetworkReplicator::ApplyTransformSnapshots(std::span<const NetworkTransformSnapshot> snapshots,
                                                    const SimulationFrame &frame) noexcept {
        if (world_ == nullptr) {
            return;
        }

        const double fixed_dt = frame.fixed_delta_time > 0.0f ? frame.fixed_delta_time : (1.0 / 60.0);
        for (const NetworkTransformSnapshot &snapshot: snapshots) {
            Node node = FindNode(snapshot.network_id);
            if (!node.IsValid()) {
                node = world_->CreateNode("RemoteNetworkEntity");
                (void) RegisterEntity(node, snapshot.network_id, kInvalidPeerId, false);
            }

            auto *network_transform = node.TryGetComponent<NetworkTransformComponent>();
            auto *transform = node.TryGetComponent<TransformComponent>();
            const auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
            if (network_transform == nullptr || transform == nullptr || identity == nullptr) {
                continue;
            }

            network_transform->authoritative_position = snapshot.position;
            network_transform->authoritative_rotation = snapshot.rotation;
            network_transform->authoritative_scale = snapshot.scale;
            network_transform->last_snapshot_tick = snapshot.tick;
            network_transform->interpolation_buffer.Push(SnapshotSample{
                .server_tick = snapshot.tick,
                .server_time = static_cast<double>(snapshot.tick) * fixed_dt,
                .position = snapshot.position,
                .rotation = snapshot.rotation,
                .scale = snapshot.scale,
                .linear_velocity = {},
                .component_mask = 0,
            });

            if (!identity->local_authority) {
                transform->SetPosition(snapshot.position);
                transform->SetRotation(snapshot.rotation);
                transform->SetScale(snapshot.scale);
            }

            if ((snapshot.component_mask & ComponentBit(kPlayerMovementStateComponentTypeId)) != 0u) {
                auto *movement = node.TryGetComponent<PlayerMovementStateComponent>();
                if (movement == nullptr) {
                    movement = &node.AddComponent<PlayerMovementStateComponent>();
                }
                movement->last_processed_input_sequence = snapshot.last_processed_input_sequence;
            }

            if ((snapshot.component_mask & ComponentBit(kHealthComponentTypeId)) != 0u) {
                auto *health = node.TryGetComponent<HealthComponent>();
                if (health == nullptr) {
                    health = &node.AddComponent<HealthComponent>();
                }
                health->health = snapshot.health;
                health->max_health = snapshot.max_health;
                health->alive = snapshot.alive;
                health->concussed = snapshot.concussed;
            }

            if ((snapshot.component_mask & ComponentBit(kBountyBeaconCarrierComponentTypeId)) != 0u) {
                auto *beacon = node.TryGetComponent<BountyBeaconComponent>();
                if (beacon == nullptr) {
                    beacon = &node.AddComponent<BountyBeaconComponent>();
                }
                beacon->original_owner_player = snapshot.beacon_original_owner;
                beacon->current_carrier_player = snapshot.beacon_carrier;
                beacon->on_ground = snapshot.beacon_on_ground;
                beacon->extracted = snapshot.beacon_extracted;
            }

            if ((snapshot.component_mask & ComponentBit(kCaptureStateComponentTypeId)) != 0u) {
                auto *capture = node.TryGetComponent<CaptureStateComponent>();
                if (capture == nullptr) {
                    capture = &node.AddComponent<CaptureStateComponent>();
                }
                capture->captor_player = snapshot.capture_captor;
                capture->captured = snapshot.captured;
                capture->capturable = snapshot.captured;
                capture->cast_remaining_seconds = 0.0f;
            }
        }
    }

    bool NetworkReplicator::ShouldSendSnapshot() noexcept {
        if (ticks_until_next_snapshot_ > 0) {
            --ticks_until_next_snapshot_;
            return false;
        }

        ticks_until_next_snapshot_ = snapshot_interval_ticks_ > 0 ? snapshot_interval_ticks_ - 1u : 0u;
        return true;
    }
} // namespace CoreEngine
