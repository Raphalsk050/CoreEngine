#include "core/network/replication/network_replicator.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/network/message_reader.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_transform_component.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/simulation/simulation_frame.h"

#include <algorithm>
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
        archetype_registry_.Reset();
        spawn_system_.Reset();
        lag_history_.Reset();
        entity_by_network_id_.clear();
        entity_by_network_id_.reserve(256);
        snapshot_scratch_.clear();
        snapshot_scratch_.reserve(256);
        inbound_snapshot_scratch_.clear();
        inbound_snapshot_scratch_.reserve(256);
        entity_destruction_scratch_.clear();
        entity_destruction_scratch_.reserve(64);
        snapshot_sequence_ = 0;
        ticks_until_next_snapshot_ = 0;
        presentation_server_time_ = 0.0;
        newest_snapshot_server_time_ = 0.0;
        presentation_time_initialized_ = false;
        last_session_role_ = network_system.Session().Role();
        last_session_state_ = network_system.Session().State();
        last_lobby_id_ = network_system.Session().LobbyId();
        stats_ = {};
    }

    void NetworkReplicator::Shutdown() noexcept {
        network_system_ = nullptr;
        world_ = nullptr;
        archetype_registry_.Reset();
        spawn_system_.Reset();
        lag_history_.Reset();
        entity_by_network_id_.clear();
        snapshot_scratch_.clear();
        inbound_snapshot_scratch_.clear();
        entity_destruction_scratch_.clear();
        snapshot_sequence_ = 0;
        ticks_until_next_snapshot_ = 0;
        presentation_server_time_ = 0.0;
        newest_snapshot_server_time_ = 0.0;
        presentation_time_initialized_ = false;
        last_session_role_ = NetworkRole::Offline;
        last_session_state_ = NetworkSessionState::Offline;
        last_lobby_id_ = 0;
        stats_ = {};
    }

    void NetworkReplicator::BeginSimulationTick(const SimulationFrame &frame) noexcept {
        ProcessSessionLifecycleEvents();
        ApplyInboundSnapshots(frame);
    }

    void NetworkReplicator::EndSimulationTick(const SimulationFrame &frame) noexcept {
        StoreLagCompensationSamples(frame);
        SendOutboundSnapshots(frame);
        spawn_system_.ClearPendingEvents();
    }

    void NetworkReplicator::UpdatePresentation(float delta_seconds) noexcept {
        if (world_ == nullptr || delta_seconds <= 0.0f || !presentation_time_initialized_) {
            return;
        }

        presentation_server_time_ += static_cast<double>(delta_seconds);
        const double max_extrapolated_time = newest_snapshot_server_time_ + 0.25;
        if (presentation_server_time_ > max_extrapolated_time) {
            presentation_server_time_ = max_extrapolated_time;
        }

        auto view = world_->View<NetworkIdentityComponent, NetworkTransformComponent, TransformComponent>();
        for (const entt::entity entity: view) {
            const auto &identity = view.get<NetworkIdentityComponent>(entity);
            if (!identity.IsNetworked() || identity.local_authority) {
                continue;
            }

            auto &network_transform = view.get<NetworkTransformComponent>(entity);
            if (!network_transform.interpolation_enabled || network_transform.interpolation_buffer.Count() == 0u) {
                continue;
            }

            SnapshotSample sample;
            const double render_time =
                presentation_server_time_ - static_cast<double>(network_transform.interpolation_delay_seconds);
            if (!network_transform.interpolation_buffer.Sample(render_time, sample)) {
                continue;
            }

            auto &transform = view.get<TransformComponent>(entity);
            transform.SetPosition(sample.position);
            transform.SetRotation(sample.rotation);
            transform.SetScale(sample.scale);
        }
    }

    NetworkEntityId NetworkReplicator::RegisterEntity(Node node,
                                                      const NetworkEntityRegistrationDesc &desc) {
        if (!node.IsValid()) {
            return 0;
        }

        NetworkEntityId network_id = desc.network_id;
        if (network_id == 0) {
            network_id = spawn_system_.AllocateNetworkEntityId();
        }

        auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
        if (identity == nullptr) {
            identity = &node.AddComponent<NetworkIdentityComponent>();
        }

        identity->network_id = network_id;
        identity->owner_peer = desc.owner_peer;
        identity->archetype_id = desc.archetype_id;
        identity->presentation_id = desc.presentation_id;
        identity->local_authority = desc.local_authority;
        identity->replicated = true;

        if (node.TryGetComponent<NetworkTransformComponent>() == nullptr) {
            node.AddComponent<NetworkTransformComponent>();
        }

        entity_by_network_id_[network_id] = node.Handle();
        return network_id;
    }

    NetworkEntityId NetworkReplicator::RegisterEntity(Node node,
                                                      NetworkEntityId network_id,
                                                      PeerId owner_peer,
                                                      bool local_authority) {
        return RegisterEntity(node,
                              NetworkEntityRegistrationDesc{
                                  .network_id = network_id,
                                  .owner_peer = owner_peer,
                                  .archetype_id = 0,
                                  .presentation_id = 0,
                                  .local_authority = local_authority,
                              });
    }

    NetworkEntityId NetworkReplicator::RegisterEntity(Node node, PeerId owner_peer, bool local_authority) {
        return RegisterEntity(node, spawn_system_.AllocateNetworkEntityId(), owner_peer, local_authority);
    }

    void NetworkReplicator::UnregisterEntity(NetworkEntityId network_id) noexcept {
        entity_by_network_id_.erase(network_id);
    }

    void NetworkReplicator::DestroyEntitiesOwnedByPeer(PeerId peer) noexcept {
        if (world_ == nullptr || peer == kInvalidPeerId) {
            return;
        }

        entity_destruction_scratch_.clear();
        auto view = world_->View<NetworkIdentityComponent>();
        for (const entt::entity entity: view) {
            const auto &identity = view.get<NetworkIdentityComponent>(entity);
            if (identity.IsNetworked() && identity.owner_peer == peer) {
                entity_destruction_scratch_.push_back(identity.network_id);
            }
        }

        for (const NetworkEntityId network_id: entity_destruction_scratch_) {
            Node node = FindNode(network_id);
            entity_by_network_id_.erase(network_id);
            if (node.IsValid()) {
                world_->DestroyNode(node);
            }
        }
        entity_destruction_scratch_.clear();
    }

    void NetworkReplicator::DestroySessionEntities() noexcept {
        if (world_ == nullptr) {
            entity_by_network_id_.clear();
            ResetSessionScopedState();
            return;
        }

        entity_destruction_scratch_.clear();
        auto view = world_->View<NetworkIdentityComponent>();
        for (const entt::entity entity: view) {
            const auto &identity = view.get<NetworkIdentityComponent>(entity);
            if (!identity.IsNetworked()) {
                continue;
            }

            const bool session_owned = identity.owner_peer != kInvalidPeerId || !identity.local_authority;
            if (session_owned) {
                entity_destruction_scratch_.push_back(identity.network_id);
            }
        }

        for (const NetworkEntityId network_id: entity_destruction_scratch_) {
            Node node = FindNode(network_id);
            entity_by_network_id_.erase(network_id);
            if (node.IsValid()) {
                world_->DestroyNode(node);
            }
        }
        entity_destruction_scratch_.clear();
        ResetSessionScopedState();
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

    void NetworkReplicator::ProcessSessionLifecycleEvents() noexcept {
        if (network_system_ == nullptr) {
            return;
        }

        const NetworkSession &session = network_system_->Session();
        const bool session_changed =
            session.Role() != last_session_role_ ||
            session.State() != last_session_state_ ||
            session.LobbyId() != last_lobby_id_;

        if (session_changed &&
            (session.State() == NetworkSessionState::Offline ||
             session.State() == NetworkSessionState::Disconnecting ||
             session.Role() == NetworkRole::Offline ||
             session.LobbyId() != last_lobby_id_ ||
             session.Role() != last_session_role_)) {
            DestroySessionEntities();
        }

        for (const NetworkEvent &event: network_system_->Events()) {
            if (event.type == NetworkEventType::PeerDisconnected) {
                DestroyEntitiesOwnedByPeer(event.peer);
            }
        }

        last_session_role_ = session.Role();
        last_session_state_ = session.State();
        last_lobby_id_ = session.LobbyId();
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
                .owner_peer = identity.owner_peer,
                .archetype_id = identity.archetype_id,
                .presentation_id = identity.presentation_id,
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

        const double fixed_dt = frame.fixed_delta_time > 0.0f ? frame.fixed_delta_time : (1.0 / 128.0);
        for (const NetworkTransformSnapshot &snapshot: snapshots) {
            Node node = FindNode(snapshot.network_id);
            if (!node.IsValid()) {
                const NetworkArchetypeDesc *archetype = archetype_registry_.Find(snapshot.archetype_id);
                const char *debug_name =
                    archetype != nullptr && archetype->debug_name != nullptr
                        ? archetype->debug_name
                        : "RemoteNetworkEntity";
                node = world_->CreateNode(debug_name);
                (void) RegisterEntity(node,
                                      NetworkEntityRegistrationDesc{
                                          .network_id = snapshot.network_id,
                                          .owner_peer = snapshot.owner_peer,
                                          .archetype_id = snapshot.archetype_id,
                                          .presentation_id = snapshot.presentation_id,
                                          .local_authority = false,
                                      });
                if (archetype != nullptr && archetype->initialize_remote_entity != nullptr) {
                    NetworkArchetypeSpawnContext spawn_context{
                        .world = *world_,
                        .node = node,
                        .network_id = snapshot.network_id,
                        .owner_peer = snapshot.owner_peer,
                        .archetype_id = snapshot.archetype_id,
                        .presentation_id = snapshot.presentation_id,
                        .local_authority = false,
                    };
                    archetype->initialize_remote_entity(spawn_context, archetype->user_data);
                }
            }

            auto *network_transform = node.TryGetComponent<NetworkTransformComponent>();
            auto *transform = node.TryGetComponent<TransformComponent>();
            auto *identity = node.TryGetComponent<NetworkIdentityComponent>();
            if (network_transform == nullptr || transform == nullptr || identity == nullptr) {
                continue;
            }

            identity->owner_peer = snapshot.owner_peer;
            identity->archetype_id = snapshot.archetype_id;
            identity->presentation_id = snapshot.presentation_id;

            const double snapshot_time = static_cast<double>(snapshot.tick) * fixed_dt;
            Math::Vec3 linear_velocity{};
            if (network_transform->last_snapshot_tick != 0u &&
                snapshot.tick > network_transform->last_snapshot_tick) {
                const double previous_time =
                    static_cast<double>(network_transform->last_snapshot_tick) * fixed_dt;
                const double elapsed = snapshot_time - previous_time;
                if (elapsed > 1.0e-6) {
                    linear_velocity =
                        (snapshot.position - network_transform->authoritative_position) /
                        static_cast<float>(elapsed);
                }
            }

            network_transform->authoritative_position = snapshot.position;
            network_transform->authoritative_rotation = snapshot.rotation;
            network_transform->authoritative_scale = snapshot.scale;
            network_transform->last_snapshot_tick = snapshot.tick;
            network_transform->interpolation_buffer.Push(SnapshotSample{
                .server_tick = snapshot.tick,
                .server_time = snapshot_time,
                .position = snapshot.position,
                .rotation = snapshot.rotation,
                .scale = snapshot.scale,
                .linear_velocity = linear_velocity,
                .component_mask = 0,
            });
            newest_snapshot_server_time_ = std::max(newest_snapshot_server_time_, snapshot_time);
            if (!presentation_time_initialized_) {
                presentation_server_time_ = newest_snapshot_server_time_;
                presentation_time_initialized_ = true;
            }

            if (!identity->local_authority &&
                (!network_transform->interpolation_enabled ||
                 network_transform->interpolation_buffer.Count() < 2u)) {
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

    void NetworkReplicator::ResetSessionScopedState() noexcept {
        snapshot_sequence_ = 0;
        ticks_until_next_snapshot_ = 0;
        presentation_server_time_ = 0.0;
        newest_snapshot_server_time_ = 0.0;
        presentation_time_initialized_ = false;
        inbound_snapshot_scratch_.clear();
        snapshot_scratch_.clear();
        lag_history_.Reset();
    }

    void NetworkReplicator::StoreLagCompensationSamples(const SimulationFrame &frame) noexcept {
        if (world_ == nullptr ||
            (network_system_ != nullptr && network_system_->Session().Role() == NetworkRole::Client)) {
            return;
        }

        const double fixed_dt = frame.fixed_delta_time > 0.0f ? frame.fixed_delta_time : (1.0 / 128.0);
        auto view = world_->View<NetworkIdentityComponent, TransformComponent>();
        for (const entt::entity entity: view) {
            const auto &identity = view.get<NetworkIdentityComponent>(entity);
            if (!identity.IsNetworked()) {
                continue;
            }

            const auto &transform = view.get<TransformComponent>(entity);
            const auto *health = world_->TryGetComponent<HealthComponent>(entity);
            const auto *capture = world_->TryGetComponent<CaptureStateComponent>(entity);
            lag_history_.Store(LagCompensationSample{
                .entity_id = identity.network_id,
                .server_tick = frame.tick,
                .server_time = static_cast<double>(frame.tick) * fixed_dt,
                .position = transform.Position(),
                .rotation = transform.Rotation(),
                .half_extents = {0.5f, 0.9f, 0.5f},
                .alive = health == nullptr || health->alive,
                .captured = capture != nullptr && capture->captured,
            });
        }
    }
} // namespace CoreEngine
