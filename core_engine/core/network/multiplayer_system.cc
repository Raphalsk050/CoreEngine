#include "core/network/multiplayer_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_transform_component.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/simulation/simulation_scheduler.h"

#include <algorithm>

namespace CoreEngine {
    namespace {
        const NetworkStats kEmptyNetworkStats{};

        [[nodiscard]] std::uint16_t QuantizeSubTick(float alpha) noexcept {
            return static_cast<std::uint16_t>(std::clamp(alpha, 0.0f, 1.0f) * 65535.0f);
        }
    }

    bool MultiplayerSystem::Initialize(NetworkSystem &network_system, World &world) {
        Shutdown();
        network_system_ = &network_system;
        replicator_.Initialize(network_system, world);
        return true;
    }

    void MultiplayerSystem::Shutdown() noexcept {
        replicator_.Shutdown();
        prediction_system_.Reset();
        network_system_ = nullptr;
        latest_local_input_stamp_ = {};
    }

    void MultiplayerSystem::BeginSimulationTick(const SimulationFrame &frame) noexcept {
        replicator_.BeginSimulationTick(frame);
    }

    void MultiplayerSystem::EndSimulationTick(const SimulationFrame &frame) noexcept {
        replicator_.EndSimulationTick(frame);
    }

    void MultiplayerSystem::UpdatePresentation(float delta_seconds) noexcept {
        replicator_.UpdatePresentation(delta_seconds);
    }

    NetworkRole MultiplayerSystem::Role() const noexcept {
        return network_system_ != nullptr ? network_system_->Session().Role() : NetworkRole::Offline;
    }

    NetworkSessionState MultiplayerSystem::SessionState() const noexcept {
        return network_system_ != nullptr ? network_system_->Session().State() : NetworkSessionState::Offline;
    }

    std::span<const NetworkPeer> MultiplayerSystem::Peers() const noexcept {
        return network_system_ != nullptr ? network_system_->Session().Peers() : std::span<const NetworkPeer>{};
    }

    const NetworkStats &MultiplayerSystem::Stats() const noexcept {
        return network_system_ != nullptr ? network_system_->Stats() : kEmptyNetworkStats;
    }

    std::span<const QueuedPlayerInputCommand> MultiplayerSystem::InputCommands() const noexcept {
        return network_system_ != nullptr ? network_system_->InputCommands() : std::span<const QueuedPlayerInputCommand>{};
    }

    NetworkEntityId MultiplayerSystem::RegisterEntity(Node node,
                                                      const NetworkEntityRegistrationDesc &desc) {
        return replicator_.RegisterEntity(node, desc);
    }

    NetworkEntityId MultiplayerSystem::RegisterEntity(Node node,
                                                      NetworkEntityId network_id,
                                                      PeerId owner_peer,
                                                      bool local_authority) {
        return replicator_.RegisterEntity(node, network_id, owner_peer, local_authority);
    }

    NetworkEntityId MultiplayerSystem::RegisterEntity(Node node, PeerId owner_peer, bool local_authority) {
        return replicator_.RegisterEntity(node, owner_peer, local_authority);
    }

    void MultiplayerSystem::UnregisterEntity(NetworkEntityId network_id) noexcept {
        replicator_.UnregisterEntity(network_id);
    }

    Node MultiplayerSystem::FindNode(NetworkEntityId network_id) const noexcept {
        return replicator_.FindNode(network_id);
    }

    NetworkEntityId MultiplayerSystem::GetNetworkId(Node node) const noexcept {
        return replicator_.GetNetworkId(node);
    }

    bool MultiplayerSystem::HasAuthority(Node node) const noexcept {
        return replicator_.HasAuthority(node);
    }

    bool MultiplayerSystem::IsOwningClient(Node node, PeerId peer) const noexcept {
        return replicator_.IsOwningClient(node, peer);
    }

    bool MultiplayerSystem::RegisterArchetype(const NetworkArchetypeDesc &desc) noexcept {
        return replicator_.RegisterArchetype(desc);
    }

    void MultiplayerSystem::UnregisterArchetype(NetworkArchetypeId archetype_id) noexcept {
        replicator_.UnregisterArchetype(archetype_id);
    }

    const NetworkReplicatorStats &MultiplayerSystem::ReplicationStats() const noexcept {
        return replicator_.Stats();
    }

    void MultiplayerSystem::CaptureLocalInputSample(const SimulationScheduler &scheduler) noexcept {
        latest_local_input_stamp_ = LocalInputSampleStamp{
            .tick = scheduler.Clock().CurrentTick(),
            .sub_tick = QuantizeSubTick(scheduler.Clock().InterpolationAlpha()),
            .valid = true,
        };
    }

    PlayerInputCommand MultiplayerSystem::BuildLocalPlayerInputCommand(
        const LocalPlayerInputDesc &input) noexcept {
        const LocalInputSampleStamp stamp =
            latest_local_input_stamp_.valid
                ? latest_local_input_stamp_
                : LocalInputSampleStamp{};

        return PlayerInputCommand{
            .client_tick = stamp.tick,
            .sub_tick = stamp.sub_tick,
            .sequence = prediction_system_.NextSequence(),
            .last_received_server_snapshot_tick = 0,
            .move_x = input.move_x,
            .move_y = input.move_y,
            .look_yaw = input.look_yaw,
            .look_pitch = input.look_pitch,
            .buttons = input.buttons,
            .selected_slot = input.selected_slot,
        };
    }

    bool MultiplayerSystem::SubmitLocalPlayerInput(NetworkEntityId local_entity_id,
                                                   const PlayerInputCommand &command,
                                                   const PredictedMovementState &predicted_state) {
        if (network_system_ == nullptr) {
            return false;
        }

        if (Role() == NetworkRole::Client) {
            prediction_system_.RecordPrediction(command, predicted_state);
            return network_system_->SendPlayerInputCommands(
                prediction_system_.BuildRedundantCommandBatch(command));
        }

        return network_system_->SubmitLocalPlayerInputCommand(local_entity_id, command);
    }

    LocalPlayerReconciliationPlan MultiplayerSystem::BuildLocalPlayerReconciliationPlan(Node node) noexcept {
        if (Role() != NetworkRole::Client || !node.IsValid()) {
            return {};
        }

        const auto *movement = node.TryGetComponent<PlayerMovementStateComponent>();
        const auto *network_transform = node.TryGetComponent<NetworkTransformComponent>();
        if (movement == nullptr || network_transform == nullptr) {
            return {};
        }
        if (network_transform->last_snapshot_tick == 0u) {
            return {};
        }

        const PredictedMovementState authoritative_state{
            .position = network_transform->authoritative_position,
            .rotation = network_transform->authoritative_rotation,
            .velocity = movement->velocity,
            .movement_flags = 0,
        };

        return prediction_system_.BuildReconciliationPlan(
            authoritative_state,
            movement->last_processed_input_sequence);
    }
} // namespace CoreEngine
