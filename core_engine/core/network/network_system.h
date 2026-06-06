#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "core/network/network_message.h"
#include "core/network/network_session.h"
#include "core/network/network_stats.h"
#include "core/online/steam/steam_types.h"

namespace CoreEngine {
    class SteamAuthService;
    class SteamLobbyService;
    class SteamOnlineSystem;
    class SteamP2PTransport;

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

        void DumpConnectionStatus() const;

        [[nodiscard]] const NetworkEventQueue &Events() const noexcept { return current_events_; }

        [[nodiscard]] NetworkSession &Session() noexcept { return session_; }

        [[nodiscard]] const NetworkSession &Session() const noexcept { return session_; }

        [[nodiscard]] const NetworkStats &Stats() const noexcept { return stats_; }

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

        [[nodiscard]] std::uint32_t NextSequence() noexcept { return next_sequence_++; }

        SteamOnlineSystem &online_system_;
        std::unique_ptr<SteamLobbyService> lobby_service_;
        std::unique_ptr<SteamP2PTransport> transport_;
        std::unique_ptr<SteamAuthService> auth_service_;
        NetworkSession session_;
        NetworkStats stats_;
        NetworkEventQueue current_events_;
        bool initialized_ = false;
        int requested_max_players_ = 8;
        std::uint32_t next_sequence_ = 1;
        std::uint32_t local_tick_ = 0;
    };
} // namespace CoreEngine
