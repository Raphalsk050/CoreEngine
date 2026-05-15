#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/network/network_protocol.h"

namespace CoreEngine {
    /**
     * @brief Reads bounded protocol payloads from immutable packet memory.
     *
     * Responsibility: validate wire sizes before gameplay or auth code sees
     * external data.
     */
    class MessageReader {
    public:
        explicit MessageReader(std::span<const std::byte> bytes) noexcept;

        bool ReadUInt8(std::uint8_t &value) noexcept;
        bool ReadUInt16(std::uint16_t &value) noexcept;
        bool ReadUInt32(std::uint32_t &value) noexcept;
        bool ReadUInt64(std::uint64_t &value) noexcept;
        bool ReadFloat(float &value) noexcept;
        bool ReadBool(bool &value) noexcept;
        bool ReadBytes(std::span<std::byte> out_bytes) noexcept;
        bool ReadSizedBytes(std::span<const std::byte> &out_bytes) noexcept;

        [[nodiscard]] std::size_t Remaining() const noexcept;

    private:
        [[nodiscard]] bool CanRead(std::size_t bytes) const noexcept;

        std::span<const std::byte> bytes_;
        std::size_t offset_ = 0;
    };

    [[nodiscard]] bool ParsePacket(std::span<const std::byte> bytes,
                                   PacketHeader &out_header,
                                   std::span<const std::byte> &out_payload) noexcept;
} // namespace CoreEngine
