#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/network/network_message.h"
#include "core/network/network_peer.h"

namespace CoreEngine {
    enum class NetworkRole : std::uint8_t {
        Offline,
        Host,
        Client,
    };

    enum class NetworkSessionState : std::uint8_t {
        Offline,
        CreatingLobby,
        InLobby,
        Connecting,
        Authenticating,
        Connected,
        Disconnecting,
    };

    [[nodiscard]] const char *ToString(NetworkRole role) noexcept;
    [[nodiscard]] const char *ToString(NetworkSessionState state) noexcept;
    [[nodiscard]] const char *ToString(NetworkDisconnectReason reason) noexcept;

    /**
     * @brief Owns replicated session identity and peer state.
     *
     * Responsibility: keep lobby, role, and peer lifecycle state deterministic
     * while transports remain replaceable implementation details.
     */
    class NetworkSession {
    public:
        void Reset() noexcept;

        void BeginHostLobby(std::uint64_t lobby_id, std::uint64_t local_steam_id);

        void BeginClientLobby(std::uint64_t lobby_id, std::uint64_t owner_steam_id, std::uint64_t local_steam_id);

        void SetState(NetworkSessionState state) noexcept;

        void SetDisconnectReason(NetworkDisconnectReason reason) noexcept;

        NetworkPeer &AddOrUpdatePeer(PeerId peer_id, std::uint64_t steam_id, NetworkPeerState state);

        void RemovePeer(PeerId peer_id) noexcept;

        [[nodiscard]] NetworkPeer *FindPeer(PeerId peer_id) noexcept;

        [[nodiscard]] const NetworkPeer *FindPeer(PeerId peer_id) const noexcept;

        [[nodiscard]] std::span<const NetworkPeer> Peers() const noexcept {
            return peers_;
        }

        [[nodiscard]] NetworkRole Role() const noexcept {
            return role_;
        }

        [[nodiscard]] NetworkSessionState State() const noexcept {
            return state_;
        }

        [[nodiscard]] std::uint64_t LobbyId() const noexcept {
            return lobby_id_;
        }

        [[nodiscard]] std::uint64_t LobbyOwnerSteamId() const noexcept {
            return lobby_owner_steam_id_;
        }

        [[nodiscard]] std::uint64_t LocalSteamId() const noexcept {
            return local_steam_id_;
        }

        [[nodiscard]] NetworkDisconnectReason LastDisconnectReason() const noexcept {
            return last_disconnect_reason_;
        }

    private:
        NetworkRole role_ = NetworkRole::Offline;
        NetworkSessionState state_ = NetworkSessionState::Offline;
        NetworkDisconnectReason last_disconnect_reason_ = NetworkDisconnectReason::None;
        std::uint64_t lobby_id_ = 0;
        std::uint64_t lobby_owner_steam_id_ = 0;
        std::uint64_t local_steam_id_ = 0;
        std::vector<NetworkPeer> peers_;
    };
} // namespace CoreEngine
