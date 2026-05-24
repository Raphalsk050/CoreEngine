#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/network/network_peer.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    using NetworkGameplayEventTypeId = std::uint32_t;

    inline constexpr std::uint16_t kMaxNetworkGameplayEventPayloadBytes = 96;

    /**
     * @brief Carries a small transient gameplay event over the active network session.
     *
     * Responsibility: replicate lightweight, non-stateful gameplay moments such as
     * fire effects without forcing them into transform snapshots or raw input streams.
     */
    struct NetworkGameplayEvent {
        PeerId peer = kInvalidPeerId;
        NetworkGameplayEventTypeId event_type = 0;
        NetworkEntityId source_network_id = 0;
        std::uint32_t sequence = 0;
        std::uint32_t server_tick = 0;
        std::uint16_t payload_size = 0;
        std::array<std::byte, kMaxNetworkGameplayEventPayloadBytes> payload{};

        [[nodiscard]] std::span<const std::byte> Payload() const noexcept {
            return std::span<const std::byte>{payload.data(), payload_size};
        }

        [[nodiscard]] bool SetPayload(std::span<const std::byte> bytes) noexcept {
            if (bytes.size() > payload.size()) {
                return false;
            }

            std::copy(bytes.begin(), bytes.end(), payload.begin());
            payload_size = static_cast<std::uint16_t>(bytes.size());
            return true;
        }
    };
} // namespace CoreEngine
