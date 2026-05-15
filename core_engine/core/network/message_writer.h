#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/network/network_protocol.h"

namespace CoreEngine {
    /**
     * @brief Serializes network packets using explicit little-endian wire layout.
     *
     * Responsibility: keep protocol writes bounded and independent from native
     * struct padding.
     */
    class MessageWriter {
    public:
        explicit MessageWriter(std::size_t reserve_bytes = 256);

        void Reset();

        bool Begin(NetMessageType message_type,
                   std::uint32_t sequence,
                   std::uint32_t ack,
                   std::uint32_t tick);

        bool Finalize();

        bool WriteUInt8(std::uint8_t value);
        bool WriteUInt16(std::uint16_t value);
        bool WriteUInt32(std::uint32_t value);
        bool WriteUInt64(std::uint64_t value);
        bool WriteFloat(float value);
        bool WriteBool(bool value);
        bool WriteBytes(std::span<const std::byte> bytes);
        bool WriteSizedBytes(std::span<const std::byte> bytes);

        [[nodiscard]] std::span<const std::byte> Bytes() const noexcept {
            return buffer_;
        }

        [[nodiscard]] std::vector<std::byte> TakeBytes() {
            return std::move(buffer_);
        }

    private:
        bool CanWrite(std::size_t bytes) const noexcept;

        void PatchUInt16(std::size_t offset, std::uint16_t value) noexcept;

        std::vector<std::byte> buffer_;
    };
} // namespace CoreEngine
