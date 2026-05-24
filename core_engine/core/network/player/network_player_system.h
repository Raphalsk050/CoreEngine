#pragma once

#include <cstdint>
#include <vector>

#include "core/ecs/node.h"
#include "core/math/math.h"
#include "core/network/network_input_command_queue.h"
#include "core/network/network_peer.h"
#include "core/network/player/network_player_ids.h"
#include "core/network/player/networked_player_movement.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/replication/network_archetype_registry.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    class MultiplayerSystem;
    class World;
    struct SimulationFrame;

    struct NetworkPlayerInputState {
        Math::Vec2 movement{};
        float look_yaw = 0.0f;
        float look_pitch = 0.0f;
        std::uint8_t selected_slot = 0;

        void SetAction(PlayerCommandActionId action, bool down) noexcept {
            const std::uint64_t bit = PlayerCommandActionBit(action);
            if (bit == 0u) {
                return;
            }

            if (down) {
                action_bits_ |= bit;
            } else {
                action_bits_ &= ~bit;
            }
        }

        [[nodiscard]] bool IsActionDown(PlayerCommandActionId action) const noexcept {
            return (action_bits_ & PlayerCommandActionBit(action)) != 0u;
        }

        [[nodiscard]] std::uint64_t ActionBits() const noexcept {
            return action_bits_;
        }

    private:
        std::uint64_t action_bits_ = 0;
    };

    struct NetworkPlayerEntityInitContext {
        World &world;
        Node node;
        NetworkEntityId network_id = 0;
        PeerId owner_peer = kInvalidPeerId;
        NetworkArchetypeId archetype_id = kDefaultNetworkPlayerArchetypeId;
        NetworkPresentationId presentation_id = kDefaultNetworkPlayerPresentationId;
        bool local_player = false;
        bool local_authority = false;
    };

    using NetworkPlayerEntityInitFn = void (*)(NetworkPlayerEntityInitContext &context, void *user_data);

    struct NetworkPlayerSystemDesc {
        NetworkArchetypeId archetype_id = kDefaultNetworkPlayerArchetypeId;
        NetworkPresentationId presentation_id = kDefaultNetworkPlayerPresentationId;
        NetworkedPlayerMovementComponent movement{};
        NetworkPlayerEntityInitFn initialize_player_entity = nullptr;
        void *user_data = nullptr;
    };

    struct LocalNetworkPlayerDesc {
        Node node;
        std::uint64_t local_user_id = 0;
        NetworkPresentationId presentation_id = kDefaultNetworkPlayerPresentationId;
        NetworkedPlayerMovementComponent movement{};
    };

    /**
     * @brief Owns default replicated player spawning, input submission, and reconciliation.
     *
     * Responsibility: let applications provide presentation and gameplay-specific
     * components while the engine owns network command construction, host peer
     * spawning, fixed-tick movement prediction, and client correction replay.
     */
    class NetworkPlayerSystem final {
    public:
        bool Initialize(MultiplayerSystem &multiplayer, World &world);

        void Shutdown() noexcept;

        bool Configure(const NetworkPlayerSystemDesc &desc) noexcept;

        [[nodiscard]] NetworkEntityId RegisterLocalPlayer(const LocalNetworkPlayerDesc &desc);

        void ClearLocalPlayer() noexcept;

        void SetLocalInput(const NetworkPlayerInputState &input) noexcept;

        void FixedUpdate(const SimulationFrame &frame) noexcept;

        [[nodiscard]] Node LocalPlayerNode() const noexcept {
            return local_player_.node;
        }

        [[nodiscard]] NetworkEntityId LocalPlayerNetworkId() const noexcept {
            return local_player_.network_id;
        }

        [[nodiscard]] Node FindPeerPlayerNode(PeerId peer) const noexcept;

    private:
        struct PlayerRecord {
            PeerId peer = kInvalidPeerId;
            std::uint64_t user_id = 0;
            NetworkEntityId network_id = 0;
            Node node;
            NetworkedPlayerMovementComponent movement{};
            std::uint32_t last_input_sequence = 0;
            PlayerInputCommand movement_command{};
            std::uint32_t ticks_since_movement_command = 0;
            bool has_movement_command = false;
            bool received_movement_command_this_tick = false;
        };

        struct LocalPlayerRecord {
            NetworkEntityId network_id = 0;
            Node node;
            NetworkedPlayerMovementComponent movement{};
            bool registered = false;
        };

        void EnsureHostPeerPlayers() noexcept;

        void PruneDisconnectedPeerPlayers() noexcept;

        void ApplyLocalPlayerSimulation(const SimulationFrame &frame) noexcept;

        void ApplyLocalPlayerReconciliation(const SimulationFrame &frame) noexcept;

        void ApplyHostInputCommands(const SimulationFrame &frame) noexcept;

        [[nodiscard]] Node SpawnPeerPlayer(PeerId peer,
                                           std::uint64_t user_id,
                                           std::uint32_t spawn_index) noexcept;

        void InitializePlayerNode(Node node,
                                  NetworkEntityId network_id,
                                  PeerId owner_peer,
                                  NetworkPresentationId presentation_id,
                                  bool local_player,
                                  bool local_authority) noexcept;

        static void InitializeRemotePlayerArchetype(NetworkArchetypeSpawnContext &context, void *user_data);

        [[nodiscard]] PlayerRecord *FindRecord(PeerId peer) noexcept;

        [[nodiscard]] const PlayerRecord *FindRecord(PeerId peer) const noexcept;

        [[nodiscard]] bool IsConnectedPeer(PeerId peer) const noexcept;

        MultiplayerSystem *multiplayer_ = nullptr;
        World *world_ = nullptr;
        NetworkPlayerSystemDesc desc_{};
        LocalPlayerRecord local_player_{};
        NetworkPlayerInputState local_input_{};
        std::vector<PlayerRecord> players_;
        bool initialized_ = false;
        bool archetype_registered_ = false;
    };
} // namespace CoreEngine
