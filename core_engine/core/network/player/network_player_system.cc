#include "core/network/player/network_player_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log.h"
#include "core/network/multiplayer_system.h"
#include "core/network/replication/network_transform_component.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/simulation/simulation_frame.h"

#include <algorithm>

namespace CoreEngine {
    namespace {
        constexpr float kMaxServerInputHoldSeconds = 0.25f;

        [[nodiscard]] NetworkEntityId NetworkIdForUser(PeerId peer, std::uint64_t user_id) noexcept {
            return MakeNetworkPlayerEntityId(peer, user_id);
        }
    }

    bool NetworkPlayerSystem::Initialize(MultiplayerSystem &multiplayer, World &world) {
        Shutdown();
        multiplayer_ = &multiplayer;
        world_ = &world;
        players_.reserve(8);
        initialized_ = true;
        return Configure(desc_);
    }

    void NetworkPlayerSystem::Shutdown() noexcept {
        ClearLocalPlayer();

        if (multiplayer_ != nullptr && archetype_registered_) {
            multiplayer_->UnregisterArchetype(desc_.archetype_id);
        }

        players_.clear();
        local_input_ = {};
        multiplayer_ = nullptr;
        world_ = nullptr;
        initialized_ = false;
        archetype_registered_ = false;
    }

    bool NetworkPlayerSystem::Configure(const NetworkPlayerSystemDesc &desc) noexcept {
        if (desc.archetype_id == 0u) {
            return false;
        }

        if (multiplayer_ != nullptr && archetype_registered_) {
            multiplayer_->UnregisterArchetype(desc_.archetype_id);
            archetype_registered_ = false;
        }

        desc_ = desc;

        if (multiplayer_ == nullptr) {
            return true;
        }

        archetype_registered_ = multiplayer_->RegisterArchetype(NetworkArchetypeDesc{
            .archetype_id = desc_.archetype_id,
            .debug_name = "NetworkPlayer",
            .initialize_remote_entity = &NetworkPlayerSystem::InitializeRemotePlayerArchetype,
            .user_data = this,
        });

        if (!archetype_registered_) {
            Log::Error("Network", "Failed to register default network player archetype");
        }

        return archetype_registered_;
    }

    NetworkEntityId NetworkPlayerSystem::RegisterLocalPlayer(const LocalNetworkPlayerDesc &desc) {
        if (!initialized_ || multiplayer_ == nullptr || !desc.node.IsValid()) {
            return 0;
        }

        ClearLocalPlayer();

        const NetworkEntityId network_id = desc.local_user_id != 0u ? desc.local_user_id : 1u;
        const NetworkEntityId registered_id = multiplayer_->RegisterEntity(
            desc.node,
            NetworkEntityRegistrationDesc{
                .network_id = network_id,
                .owner_peer = kInvalidPeerId,
                .archetype_id = desc_.archetype_id,
                .presentation_id = desc.presentation_id,
                .local_authority = true,
            });

        local_player_ = LocalPlayerRecord{
            .network_id = registered_id,
            .node = desc.node,
            .movement = desc.movement,
            .registered = registered_id != 0u,
        };

        InitializePlayerNode(desc.node,
                             registered_id,
                             kInvalidPeerId,
                             desc.presentation_id,
                             true,
                             true);
        return registered_id;
    }

    void NetworkPlayerSystem::ClearLocalPlayer() noexcept {
        if (multiplayer_ != nullptr && local_player_.network_id != 0u) {
            multiplayer_->UnregisterEntity(local_player_.network_id);
        }

        local_player_ = {};
    }

    void NetworkPlayerSystem::SetLocalInput(const NetworkPlayerInputState &input) noexcept {
        local_input_ = input;
    }

    void NetworkPlayerSystem::FixedUpdate(const SimulationFrame &frame) noexcept {
        if (!initialized_) {
            return;
        }

        PruneDisconnectedPeerPlayers();
        EnsureHostPeerPlayers();
        ApplyLocalPlayerSimulation(frame);
        ApplyLocalPlayerReconciliation(frame);
        ApplyHostInputCommands(frame);
    }

    Node NetworkPlayerSystem::FindPeerPlayerNode(PeerId peer) const noexcept {
        const PlayerRecord *record = FindRecord(peer);
        return record != nullptr ? record->node : Node{};
    }

    void NetworkPlayerSystem::EnsureHostPeerPlayers() noexcept {
        if (multiplayer_ == nullptr || world_ == nullptr || multiplayer_->Role() != NetworkRole::Host) {
            return;
        }

        std::uint32_t spawn_index = static_cast<std::uint32_t>(players_.size());
        for (const NetworkPeer &peer: multiplayer_->Peers()) {
            if (peer.state != NetworkPeerState::Connected || FindRecord(peer.id) != nullptr) {
                continue;
            }

            Node node = SpawnPeerPlayer(peer.id, peer.steam_id, spawn_index++);
            if (!node.IsValid()) {
                continue;
            }

            players_.push_back(PlayerRecord{
                .peer = peer.id,
                .user_id = peer.steam_id,
                .network_id = multiplayer_->GetNetworkId(node),
                .node = node,
                .movement = desc_.movement,
            });
        }
    }

    void NetworkPlayerSystem::PruneDisconnectedPeerPlayers() noexcept {
        const bool is_host = multiplayer_ != nullptr && multiplayer_->Role() == NetworkRole::Host;

        for (auto it = players_.begin(); it != players_.end();) {
            if (is_host && it->node.IsValid() && IsConnectedPeer(it->peer)) {
                ++it;
                continue;
            }

            if (multiplayer_ != nullptr && it->network_id != 0u) {
                multiplayer_->UnregisterEntity(it->network_id);
            }
            if (it->node.IsValid()) {
                it->node.Destroy();
            }
            it = players_.erase(it);
        }
    }

    void NetworkPlayerSystem::ApplyLocalPlayerSimulation(const SimulationFrame &frame) noexcept {
        if (multiplayer_ == nullptr || !local_player_.registered || !local_player_.node.IsValid()) {
            return;
        }

        auto *transform = local_player_.node.TryGetComponent<TransformComponent>();
        if (transform == nullptr) {
            return;
        }

        if (multiplayer_->Role() == NetworkRole::Client) {
            const auto *network_transform = local_player_.node.TryGetComponent<NetworkTransformComponent>();
            if (multiplayer_->SessionState() != NetworkSessionState::Connected ||
                network_transform == nullptr ||
                network_transform->last_snapshot_tick == 0u) {
                return;
            }
        }

        PlayerInputCommand command = multiplayer_->BuildLocalPlayerInputCommand(LocalPlayerInputDesc{
            .move_x = local_input_.movement.x,
            .move_y = local_input_.movement.y,
            .look_yaw = local_input_.look_yaw,
            .look_pitch = local_input_.look_pitch,
            .action_bits = local_input_.ActionBits(),
            .selected_slot = local_input_.selected_slot,
        });

        NetworkedPlayerMovementSimulation::ApplyInputCommand(*transform,
                                                             local_player_.movement,
                                                             command,
                                                             frame.fixed_delta_time);
        PredictedMovementState predicted_state =
            NetworkedPlayerMovementSimulation::BuildMovementState(*transform);
        (void) multiplayer_->SubmitLocalPlayerInput(local_player_.network_id, command, predicted_state);
    }

    void NetworkPlayerSystem::ApplyLocalPlayerReconciliation(const SimulationFrame &frame) noexcept {
        if (multiplayer_ == nullptr || !local_player_.registered || !local_player_.node.IsValid()) {
            return;
        }

        const LocalPlayerReconciliationPlan plan =
            multiplayer_->BuildLocalPlayerReconciliationPlan(local_player_.node);
        if (!plan.ShouldApplyAuthoritativeState()) {
            return;
        }

        auto *transform = local_player_.node.TryGetComponent<TransformComponent>();
        if (transform == nullptr) {
            return;
        }

        transform->SetPosition(plan.authoritative_state.position);
        transform->SetRotation(plan.authoritative_state.rotation);
        multiplayer_->ReplayLocalPlayerInputCommands(
            plan,
            [this, transform, &frame](const PlayerInputCommand &command) {
                NetworkedPlayerMovementSimulation::ApplyInputCommand(*transform,
                                                                     local_player_.movement,
                                                                     command,
                                                                     frame.fixed_delta_time);
                return NetworkedPlayerMovementSimulation::BuildMovementState(*transform);
            });
    }

    void NetworkPlayerSystem::ApplyHostInputCommands(const SimulationFrame &frame) noexcept {
        if (multiplayer_ == nullptr || multiplayer_->Role() != NetworkRole::Host) {
            return;
        }

        for (PlayerRecord &record: players_) {
            record.received_movement_command_this_tick = false;
        }

        for (const QueuedPlayerInputCommand &queued: multiplayer_->InputCommands()) {
            PlayerRecord *record = FindRecord(queued.peer);
            if (record == nullptr || queued.command.sequence <= record->last_input_sequence) {
                continue;
            }

            auto *transform = record->node.TryGetComponent<TransformComponent>();
            auto *movement_state = record->node.TryGetComponent<PlayerMovementStateComponent>();
            if (transform == nullptr || movement_state == nullptr) {
                continue;
            }

            NetworkedPlayerMovementSimulation::ApplyInputCommand(*transform,
                                                                 record->movement,
                                                                 queued.command,
                                                                 frame.fixed_delta_time);
            movement_state->last_processed_input_sequence = queued.command.sequence;
            movement_state->sprinting = record->movement.sprint_action.IsValid() &&
                                        queued.command.IsActionDown(record->movement.sprint_action);

            record->movement_command = queued.command;
            record->last_input_sequence = queued.command.sequence;
            record->ticks_since_movement_command = 0;
            record->has_movement_command = true;
            record->received_movement_command_this_tick = true;
        }

        const std::uint32_t max_hold_ticks =
            std::max(1u, static_cast<std::uint32_t>(kMaxServerInputHoldSeconds / frame.fixed_delta_time));

        for (PlayerRecord &record: players_) {
            if (!record.has_movement_command ||
                record.received_movement_command_this_tick ||
                record.ticks_since_movement_command > max_hold_ticks) {
                continue;
            }

            auto *transform = record.node.TryGetComponent<TransformComponent>();
            auto *movement_state = record.node.TryGetComponent<PlayerMovementStateComponent>();
            if (transform == nullptr || movement_state == nullptr) {
                continue;
            }

            NetworkedPlayerMovementSimulation::ApplyInputCommand(*transform,
                                                                 record.movement,
                                                                 record.movement_command,
                                                                 frame.fixed_delta_time);
            movement_state->last_processed_input_sequence = record.last_input_sequence;
            movement_state->sprinting = record.movement.sprint_action.IsValid() &&
                                        record.movement_command.IsActionDown(record.movement.sprint_action);
            ++record.ticks_since_movement_command;
        }
    }

    Node NetworkPlayerSystem::SpawnPeerPlayer(PeerId peer,
                                              std::uint64_t user_id,
                                              std::uint32_t spawn_index) noexcept {
        if (world_ == nullptr || multiplayer_ == nullptr) {
            return {};
        }

        Node node = world_->CreateNode("RemotePlayer");
        node.SetPosition({-2.0f + static_cast<float>(spawn_index) * 1.5f, 0.0f, 1.5f});

        const NetworkEntityId network_id = NetworkIdForUser(peer, user_id);
        const NetworkEntityId registered_id = multiplayer_->RegisterEntity(
            node,
            NetworkEntityRegistrationDesc{
                .network_id = network_id,
                .owner_peer = peer,
                .archetype_id = desc_.archetype_id,
                .presentation_id = desc_.presentation_id,
                .local_authority = true,
            });

        InitializePlayerNode(node,
                             registered_id,
                             peer,
                             desc_.presentation_id,
                             false,
                             true);
        return node;
    }

    void NetworkPlayerSystem::InitializePlayerNode(Node node,
                                                   NetworkEntityId network_id,
                                                   PeerId owner_peer,
                                                   NetworkPresentationId presentation_id,
                                                   bool local_player,
                                                   bool local_authority) noexcept {
        if (world_ == nullptr || !node.IsValid()) {
            return;
        }

        if (node.TryGetComponent<PlayerMovementStateComponent>() == nullptr) {
            node.AddComponent<PlayerMovementStateComponent>();
        }

        if (desc_.initialize_player_entity != nullptr) {
            NetworkPlayerEntityInitContext context{
                .world = *world_,
                .node = node,
                .network_id = network_id,
                .owner_peer = owner_peer,
                .archetype_id = desc_.archetype_id,
                .presentation_id = presentation_id,
                .local_player = local_player,
                .local_authority = local_authority,
            };
            desc_.initialize_player_entity(context, desc_.user_data);
        }
    }

    void NetworkPlayerSystem::InitializeRemotePlayerArchetype(NetworkArchetypeSpawnContext &context,
                                                              void *user_data) {
        auto *system = static_cast<NetworkPlayerSystem *>(user_data);
        if (system == nullptr) {
            return;
        }

        system->InitializePlayerNode(context.node,
                                     context.network_id,
                                     context.owner_peer,
                                     context.presentation_id,
                                     false,
                                     context.local_authority);
    }

    NetworkPlayerSystem::PlayerRecord *NetworkPlayerSystem::FindRecord(PeerId peer) noexcept {
        for (PlayerRecord &record: players_) {
            if (record.peer == peer) {
                return &record;
            }
        }
        return nullptr;
    }

    const NetworkPlayerSystem::PlayerRecord *NetworkPlayerSystem::FindRecord(PeerId peer) const noexcept {
        for (const PlayerRecord &record: players_) {
            if (record.peer == peer) {
                return &record;
            }
        }
        return nullptr;
    }

    bool NetworkPlayerSystem::IsConnectedPeer(PeerId peer) const noexcept {
        if (multiplayer_ == nullptr) {
            return false;
        }

        for (const NetworkPeer &candidate: multiplayer_->Peers()) {
            if (candidate.id == peer && candidate.state == NetworkPeerState::Connected) {
                return true;
            }
        }

        return false;
    }
} // namespace CoreEngine
