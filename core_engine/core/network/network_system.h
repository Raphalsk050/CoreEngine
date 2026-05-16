#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include "core/network/network_message.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/replication/snapshot_writer.h"
#include "core/network/network_session.h"
#include "core/network/network_stats.h"
#include "core/online/steam/steam_types.h"

namespace CoreEngine {
    class INetworkTransport;
    class SteamAuthService;
    class SteamLobbyService;
    class SteamOnlineSystem;

    struct QueuedPlayerInputCommand {
        PeerId peer = kInvalidPeerId;
        std::uint64_t remote_user_id = 0;
        PlayerInputCommand command{};
    };

    /**
     * @brief Orchestrates online session state, protocol handshakes, and network frame flow.
     *
     * Responsibility: keep gameplay-facing network state transport-neutral while
     * the current MVP uses Steam lobbies, Steam auth tickets, and Steam P2P relay.
     */
    class NetworkSystem {
    public:
        explicit NetworkSystem(SteamOnlineSystem &online_system);

        ~NetworkSystem();

        bool Initialize();

        void Shutdown();

        void BeginFrame();

        void EndFrame();

        bool CreateFriendsLobby(int max_players);

        bool JoinLobbyById(std::uint64_t lobby_id);

        bool OpenInviteOverlay();

        void LeaveLobby();

        bool Send(PeerId peer, std::span<const std::byte> payload, SendMode mode);

        bool SendPlayerInputCommands(std::span<const PlayerInputCommand> commands);

        bool SubmitLocalPlayerInputCommand(NetworkEntityId local_entity_id,
                                           const PlayerInputCommand &command);

        bool SendWorldSnapshot(PeerId peer,
                               std::span<const NetworkTransformSnapshot> transforms,
                               std::uint32_t server_tick,
                               std::uint32_t snapshot_sequence,
                               std::uint32_t last_processed_input_sequence);

        void DumpConnectionStatus() const;

        [[nodiscard]] const NetworkEventQueue &Events() const noexcept {
            return current_events_;
        }

        [[nodiscard]] NetworkSession &Session() noexcept {
            return session_;
        }

        [[nodiscard]] const NetworkSession &Session() const noexcept {
            return session_;
        }

        [[nodiscard]] const NetworkStats &Stats() const noexcept {
            return stats_;
        }

        [[nodiscard]] std::span<const QueuedPlayerInputCommand> InputCommands() const noexcept {
            return input_commands_;
        }

        [[nodiscard]] std::uint32_t LastProcessedInputSequence(PeerId peer) const noexcept;

        [[nodiscard]] std::span<const SteamLobbyMember> LobbyMembers() const noexcept;

        [[nodiscard]] std::string DetailedConnectionStatus(PeerId peer) const;

    private:
        void HandleEvent(NetworkEvent &event);

        bool HandlePacketEvent(NetworkEvent &event);

        void HandleProtocolMessage(const NetworkEvent &event);

        bool SendEmptyMessage(PeerId peer, NetMessageType type, SendMode mode);

        bool SendHello(PeerId peer, NetMessageType type);

        bool SendAuthTicket(PeerId peer);

        bool SendAuthAccepted(PeerId peer);

        bool SendAuthRejected(PeerId peer, NetworkDisconnectReason reason);

        void HandleInputCommandMessage(const NetworkEvent &event);

        [[nodiscard]] std::uint32_t NextSequence() noexcept {
            return next_sequence_++;
        }

        SteamOnlineSystem &online_system_;
        std::unique_ptr<SteamLobbyService> lobby_service_;
        std::unique_ptr<INetworkTransport> transport_;
        std::unique_ptr<SteamAuthService> auth_service_;
        NetworkSession session_;
        NetworkStats stats_;
        NetworkEventQueue current_events_;
        std::vector<QueuedPlayerInputCommand> input_commands_;
        std::unordered_map<PeerId, std::uint32_t> last_input_sequence_by_peer_;
        bool initialized_ = false;
        int requested_max_players_ = 8;
        std::uint32_t next_sequence_ = 1;
        std::uint32_t local_tick_ = 0;
        std::uint64_t handshake_nonce_ = 0;
    };
} // namespace CoreEngine
