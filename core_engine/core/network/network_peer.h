#pragma once

#include <cstdint>

namespace CoreEngine {
    using PeerId = std::uint32_t;

    inline constexpr PeerId kInvalidPeerId = 0;
    inline constexpr PeerId kHostPeerId = 1;

    enum class NetworkPeerState : std::uint8_t {
        Disconnected,
        Connecting,
        Authenticating,
        Connected,
        Closing,
    };

    /**
     * @brief Tracks the protocol-facing state for one remote peer.
     *
     * Responsibility: keep stable peer identity, transport identity, and
     * sequence state out of gameplay code.
     */
    struct NetworkPeer {
        PeerId id = kInvalidPeerId;
        std::uint64_t steam_id = 0;
        NetworkPeerState state = NetworkPeerState::Disconnected;
        std::uint32_t last_received_sequence = 0;
        std::uint32_t last_sent_sequence = 0;

        [[nodiscard]] bool IsValid() const noexcept { return id != kInvalidPeerId; }
    };
} // namespace CoreEngine
