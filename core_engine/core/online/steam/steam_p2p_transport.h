#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>

#include "core/network/network_message.h"
#include "core/online/steam/steam_config.h"

#if CORE_ENGINE_ENABLE_STEAM
#include "steam/isteamnetworkingsockets.h"
#include "steam/steam_api.h"
#endif

namespace CoreEngine {
    class SteamOnlineSystem;

    /**
     * @brief Implements Steam Datagram Relay P2P message transport.
     *
     * Responsibility: own SteamNetworkingSockets listen/connect handles, accept
     * only valid session peers, and preserve message boundaries for protocol code.
     */
    class SteamP2PTransport {
    public:
        explicit SteamP2PTransport(SteamOnlineSystem &online_system);

        ~SteamP2PTransport();

        bool StartHost(std::uint16_t virtual_port = 0, std::uint32_t max_peers = 8);

        bool ConnectToHost(std::uint64_t host_steam_id, std::uint16_t virtual_port = 0);

        void Shutdown();

        void PollEvents(NetworkEventQueue &out_events);

        bool Send(PeerId peer, std::span<const std::byte> payload, SendMode mode);

        [[nodiscard]] bool IsHost() const noexcept { return is_host_; }

        [[nodiscard]] std::string DetailedConnectionStatus(PeerId peer) const;

    private:
        void QueueEvent(NetworkEvent event);

        [[nodiscard]] bool CanAcceptMorePeers() const noexcept;

        PeerId RegisterConnection(std::uint64_t remote_steam_id,
#if CORE_ENGINE_ENABLE_STEAM
                                  HSteamNetConnection connection,
#else
                                  std::uint32_t connection,
#endif
                                  NetworkPeerState state);

        void DisconnectConnection(
#if CORE_ENGINE_ENABLE_STEAM
                HSteamNetConnection connection,
#else
                std::uint32_t connection,
#endif
                NetworkDisconnectReason reason);

#if CORE_ENGINE_ENABLE_STEAM
        STEAM_CALLBACK(SteamP2PTransport, OnConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t,
                       connection_status_callback_);
#endif

        SteamOnlineSystem &online_system_;
        bool is_host_ = false;
        std::uint32_t max_peers_ = 0;
        PeerId next_peer_id_ = kHostPeerId;
        NetworkEventQueue pending_events_;

#if CORE_ENGINE_ENABLE_STEAM
        HSteamListenSocket listen_socket_ = k_HSteamListenSocket_Invalid;
        HSteamNetPollGroup poll_group_ = k_HSteamNetPollGroup_Invalid;
        std::unordered_map<HSteamNetConnection, PeerId> conn_to_peer_;
        std::unordered_map<PeerId, HSteamNetConnection> peer_to_conn_;
        std::unordered_map<HSteamNetConnection, std::uint64_t> conn_to_steam_id_;
#endif
    };
} // namespace CoreEngine
