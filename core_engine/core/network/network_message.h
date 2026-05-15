#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/network/network_peer.h"
#include "core/network/network_protocol.h"

namespace CoreEngine {
    enum class SendMode : std::uint8_t {
        Unreliable,
        UnreliableNoDelay,
        Reliable,
        ReliableNoNagle,
    };

    enum class NetworkEventType : std::uint8_t {
        None,
        LobbyCreated,
        LobbyEntered,
        LobbyJoinRequested,
        LobbyLeft,
        PeerConnecting,
        PeerConnected,
        PeerDisconnected,
        PacketReceived,
        AuthAccepted,
        AuthRejected,
    };

    enum class NetworkDisconnectReason : std::uint16_t {
        None,
        LocalShutdown,
        RemoteClosed,
        TimedOut,
        ProtocolMismatch,
        BuildMismatch,
        AuthenticationFailed,
        LobbyFull,
        TransportError,
    };

    struct NetworkEvent {
        NetworkEventType type = NetworkEventType::None;
        PeerId peer = kInvalidPeerId;
        std::uint64_t remote_steam_id = 0;
        std::uint64_t lobby_id = 0;
        std::uint64_t lobby_owner_id = 0;
        NetMessageType message_type = NetMessageType::Disconnect;
        NetworkDisconnectReason disconnect_reason = NetworkDisconnectReason::None;
        std::uint32_t sequence = 0;
        std::uint32_t ack = 0;
        std::uint32_t tick = 0;
        std::vector<std::byte> payload;
    };

    using NetworkEventQueue = std::vector<NetworkEvent>;
} // namespace CoreEngine
