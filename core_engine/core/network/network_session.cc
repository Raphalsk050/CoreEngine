#include "core/network/network_session.h"

#include <algorithm>

namespace CoreEngine {
    const char *ToString(NetworkRole role) noexcept {
        switch (role) {
            case NetworkRole::Offline: return "Offline";
            case NetworkRole::Host:    return "Host";
            case NetworkRole::Client:  return "Client";
        }

        return "Unknown";
    }

    const char *ToString(NetworkSessionState state) noexcept {
        switch (state) {
            case NetworkSessionState::Offline:        return "Offline";
            case NetworkSessionState::CreatingLobby:  return "CreatingLobby";
            case NetworkSessionState::InLobby:        return "InLobby";
            case NetworkSessionState::Connecting:     return "Connecting";
            case NetworkSessionState::Authenticating: return "Authenticating";
            case NetworkSessionState::Connected:      return "Connected";
            case NetworkSessionState::Disconnecting:  return "Disconnecting";
        }

        return "Unknown";
    }

    const char *ToString(NetworkDisconnectReason reason) noexcept {
        switch (reason) {
            case NetworkDisconnectReason::None:                 return "None";
            case NetworkDisconnectReason::LocalShutdown:        return "LocalShutdown";
            case NetworkDisconnectReason::RemoteClosed:         return "RemoteClosed";
            case NetworkDisconnectReason::TimedOut:             return "TimedOut";
            case NetworkDisconnectReason::ProtocolMismatch:     return "ProtocolMismatch";
            case NetworkDisconnectReason::BuildMismatch:        return "BuildMismatch";
            case NetworkDisconnectReason::AuthenticationFailed: return "AuthenticationFailed";
            case NetworkDisconnectReason::LobbyFull:            return "LobbyFull";
            case NetworkDisconnectReason::TransportError:       return "TransportError";
        }

        return "Unknown";
    }

    void NetworkSession::Reset() noexcept {
        role_ = NetworkRole::Offline;
        state_ = NetworkSessionState::Offline;
        last_disconnect_reason_ = NetworkDisconnectReason::None;
        lobby_id_ = 0;
        lobby_owner_steam_id_ = 0;
        local_steam_id_ = 0;
        peers_.clear();
    }

    void NetworkSession::BeginHostLobby(std::uint64_t lobby_id, std::uint64_t local_steam_id) {
        Reset();
        role_ = NetworkRole::Host;
        state_ = NetworkSessionState::InLobby;
        lobby_id_ = lobby_id;
        lobby_owner_steam_id_ = local_steam_id;
        local_steam_id_ = local_steam_id;
    }

    void NetworkSession::BeginClientLobby(std::uint64_t lobby_id, std::uint64_t owner_steam_id,
                                          std::uint64_t local_steam_id) {
        Reset();
        role_ = NetworkRole::Client;
        state_ = NetworkSessionState::Connecting;
        lobby_id_ = lobby_id;
        lobby_owner_steam_id_ = owner_steam_id;
        local_steam_id_ = local_steam_id;
    }

    void NetworkSession::SetState(NetworkSessionState state) noexcept { state_ = state; }

    void NetworkSession::SetDisconnectReason(NetworkDisconnectReason reason) noexcept {
        last_disconnect_reason_ = reason;
    }

    NetworkPeer &NetworkSession::AddOrUpdatePeer(PeerId peer_id, std::uint64_t steam_id, NetworkPeerState state) {
        if (NetworkPeer *peer = FindPeer(peer_id); peer != nullptr) {
            peer->steam_id = steam_id;
            peer->state = state;
            return *peer;
        }

        NetworkPeer &peer = peers_.emplace_back();
        peer.id = peer_id;
        peer.steam_id = steam_id;
        peer.state = state;
        return peer;
    }

    void NetworkSession::RemovePeer(PeerId peer_id) noexcept {
        std::erase_if(peers_, [peer_id](const NetworkPeer &peer) { return peer.id == peer_id; });
    }

    NetworkPeer *NetworkSession::FindPeer(PeerId peer_id) noexcept {
        const auto it = std::ranges::find_if(peers_, [peer_id](const NetworkPeer &peer) { return peer.id == peer_id; });

        return it != peers_.end() ? &(*it) : nullptr;
    }

    const NetworkPeer *NetworkSession::FindPeer(PeerId peer_id) const noexcept {
        const auto it = std::ranges::find_if(peers_, [peer_id](const NetworkPeer &peer) { return peer.id == peer_id; });

        return it != peers_.end() ? &(*it) : nullptr;
    }
} // namespace CoreEngine
