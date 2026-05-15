#include "gameplay/systems/network_player_system.h"

#include "gameplay/systems/gameplay_system_context.h"
#include "player_movement_simulation.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_replicator.h"
#include "core/network/replication/network_transform_component.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/render/material.h"
#include "core/render/primitive_type.h"
#include "core/render/render_system.h"

namespace Game {
    namespace {
        [[nodiscard]] MovementComponent MakeNetworkMovement() noexcept {
            return MovementComponent{
                .crouch_speed = 1.5f,
                .walk_speed = 3.0f,
                .run_speed = 6.0f,
                .default_movement_type = MovementType::Walk,
            };
        }

        [[nodiscard]] CoreEngine::NetworkEntityId NetworkIdForUser(CoreEngine::PeerId peer,
                                                                   std::uint64_t user_id) noexcept {
            return user_id != 0u ? user_id : (0x10000000ull + peer);
        }
    }

    void NetworkPlayerSystem::Initialize(CoreEngine::RenderSystem &render_system) {
        player_mesh_ = render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        remote_material_ = CoreEngine::Material::Unlit(CoreEngine::UnlitProps{
            .color = {0.15f, 0.65f, 1.0f, 1.0f},
        }).Resolve(render_system);
        players_.reserve(8);
        initialized_ = true;
    }

    void NetworkPlayerSystem::Shutdown() noexcept {
        players_.clear();
        player_mesh_ = {};
        remote_material_ = {};
        initialized_ = false;
    }

    void NetworkPlayerSystem::FixedUpdate(const GameplaySystemContext &context) {
        if (!initialized_) {
            return;
        }

        EnsureHostPeerPlayers(context);
        ApplyHostInputCommands(context);
        EnsureRemoteRenderers(context);
    }

    CoreEngine::Node NetworkPlayerSystem::FindPlayerNode(CoreEngine::PeerId peer) const noexcept {
        const PlayerRecord *record = FindRecord(peer);
        return record != nullptr ? record->node : CoreEngine::Node{};
    }

    void NetworkPlayerSystem::EnsureHostPeerPlayers(const GameplaySystemContext &context) {
        if (context.network_system.Session().Role() != CoreEngine::NetworkRole::Host) {
            return;
        }

        std::uint32_t spawn_index = static_cast<std::uint32_t>(players_.size());
        for (const CoreEngine::NetworkPeer &peer: context.network_system.Session().Peers()) {
            if (peer.state != CoreEngine::NetworkPeerState::Connected || FindRecord(peer.id) != nullptr) {
                continue;
            }

            CoreEngine::Node node = SpawnPeerPlayer(context, peer.id, peer.steam_id, spawn_index++);
            if (!node.IsValid()) {
                continue;
            }

            players_.push_back(PlayerRecord{
                .peer = peer.id,
                .user_id = peer.steam_id,
                .network_id = context.network_replicator.GetNetworkId(node),
                .node = node,
                .last_input_sequence = 0,
            });
        }
    }

    void NetworkPlayerSystem::ApplyHostInputCommands(const GameplaySystemContext &context) {
        if (context.network_system.Session().Role() != CoreEngine::NetworkRole::Host) {
            return;
        }

        const MovementComponent movement = MakeNetworkMovement();
        for (const CoreEngine::QueuedPlayerInputCommand &queued: context.network_system.InputCommands()) {
            PlayerRecord *record = FindRecord(queued.peer);
            if (record == nullptr || queued.command.sequence <= record->last_input_sequence) {
                continue;
            }

            auto *transform = record->node.TryGetComponent<CoreEngine::TransformComponent>();
            auto *movement_state = record->node.TryGetComponent<CoreEngine::PlayerMovementStateComponent>();
            if (transform == nullptr || movement_state == nullptr) {
                continue;
            }

            PlayerMovementSimulation::ApplyInputCommand(*transform,
                                                        movement,
                                                        queued.command,
                                                        context.frame.fixed_delta_time);
            movement_state->last_processed_input_sequence = queued.command.sequence;
            movement_state->sprinting = queued.command.IsButtonDown(CoreEngine::PlayerInputButton::Sprint);
            record->last_input_sequence = queued.command.sequence;
        }
    }

    void NetworkPlayerSystem::EnsureRemoteRenderers(const GameplaySystemContext &context) {
        if (!player_mesh_.IsValid() || !remote_material_.IsValid()) {
            return;
        }

        auto view = context.world.View<CoreEngine::NetworkIdentityComponent, CoreEngine::NetworkTransformComponent>();
        for (const entt::entity entity: view) {
            auto &identity = view.get<CoreEngine::NetworkIdentityComponent>(entity);
            if (!identity.IsNetworked()) {
                continue;
            }

            const bool should_render_placeholder =
                !identity.local_authority || identity.owner_peer != CoreEngine::kInvalidPeerId;
            if (!should_render_placeholder ||
                context.world.HasComponent<CoreEngine::MeshRendererComponent>(entity)) {
                continue;
            }

            context.world.Emplace<CoreEngine::MeshRendererComponent>(
                entity,
                CoreEngine::MeshRendererComponent{
                    .mesh = player_mesh_,
                    .material = remote_material_,
                    .visible = true,
                    .cast_shadows = true,
                });
        }
    }

    CoreEngine::Node NetworkPlayerSystem::SpawnPeerPlayer(const GameplaySystemContext &context,
                                                          CoreEngine::PeerId peer,
                                                          std::uint64_t user_id,
                                                          std::uint32_t spawn_index) {
        CoreEngine::Node node = context.world.CreateNode("RemotePlayer");
        node.SetPosition({-2.0f + static_cast<float>(spawn_index) * 1.5f, 0.0f, 1.5f});
        node.SetScale({0.65f, 1.8f, 0.65f});

        const CoreEngine::NetworkEntityId network_id = NetworkIdForUser(peer, user_id);
        (void) context.network_replicator.RegisterEntity(node, network_id, peer, true);

        if (node.TryGetComponent<CoreEngine::PlayerMovementStateComponent>() == nullptr) {
            node.AddComponent<CoreEngine::PlayerMovementStateComponent>();
        }
        if (node.TryGetComponent<CoreEngine::HealthComponent>() == nullptr) {
            node.AddComponent<CoreEngine::HealthComponent>();
        }
        if (node.TryGetComponent<CoreEngine::ArmorSegmentsComponent>() == nullptr) {
            node.AddComponent<CoreEngine::ArmorSegmentsComponent>();
        }
        if (node.TryGetComponent<CoreEngine::InventoryComponent>() == nullptr) {
            node.AddComponent<CoreEngine::InventoryComponent>();
        }
        if (node.TryGetComponent<CoreEngine::EquipmentComponent>() == nullptr) {
            node.AddComponent<CoreEngine::EquipmentComponent>();
        }

        return node;
    }

    NetworkPlayerSystem::PlayerRecord *NetworkPlayerSystem::FindRecord(CoreEngine::PeerId peer) noexcept {
        for (PlayerRecord &record: players_) {
            if (record.peer == peer) {
                return &record;
            }
        }
        return nullptr;
    }

    const NetworkPlayerSystem::PlayerRecord *NetworkPlayerSystem::FindRecord(CoreEngine::PeerId peer) const noexcept {
        for (const PlayerRecord &record: players_) {
            if (record.peer == peer) {
                return &record;
            }
        }
        return nullptr;
    }
} // namespace Game
