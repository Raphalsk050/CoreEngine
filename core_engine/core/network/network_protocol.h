#pragma once

#include <cstddef>
#include <cstdint>

namespace CoreEngine {
    inline constexpr std::uint32_t kNetworkMagic = 0x43454E47u; // "CENG"
    inline constexpr std::uint16_t kNetworkProtocolVersion = 1;
    inline constexpr std::size_t kPacketHeaderWireSize = 22;
    inline constexpr std::uint16_t kMaxPacketPayloadBytes = 16u * 1024u;
    inline constexpr std::uint16_t kMaxSnapshotTransformsPerPacket = 256;

    enum class NetMessageType : std::uint16_t {
        ClientHello = 1,
        ServerHello = 2,
        AuthTicket = 3,
        AuthAccepted = 4,
        AuthRejected = 5,
        InputCommand = 6,
        WorldSnapshot = 7,
        EntitySpawn = 8,
        EntityDespawn = 9,
        Ping = 10,
        Pong = 11,
        Disconnect = 12,
    };

    struct PacketHeader {
        std::uint32_t magic = kNetworkMagic;
        std::uint16_t protocol_version = kNetworkProtocolVersion;
        NetMessageType message_type = NetMessageType::Disconnect;
        std::uint32_t sequence = 0;
        std::uint32_t ack = 0;
        std::uint32_t tick = 0;
        std::uint16_t payload_size = 0;
    };

    [[nodiscard]] constexpr bool IsGameplayMessage(NetMessageType type) noexcept {
        return type == NetMessageType::InputCommand ||
               type == NetMessageType::WorldSnapshot ||
               type == NetMessageType::EntitySpawn ||
               type == NetMessageType::EntityDespawn;
    }
} // namespace CoreEngine
