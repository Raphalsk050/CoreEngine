#include "core/network/message_reader.h"

#include <bit>

namespace CoreEngine {
    namespace {
        [[nodiscard]] std::uint8_t ToUInt8(std::byte value) noexcept { return static_cast<std::uint8_t>(value); }
    } // namespace

    MessageReader::MessageReader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    bool MessageReader::ReadUInt8(std::uint8_t &value) noexcept {
        if (!CanRead(sizeof(value))) {
            return false;
        }

        value = ToUInt8(bytes_[offset_++]);
        return true;
    }

    bool MessageReader::ReadUInt16(std::uint16_t &value) noexcept {
        if (!CanRead(sizeof(value))) {
            return false;
        }

        value = static_cast<std::uint16_t>(ToUInt8(bytes_[offset_])) |
                static_cast<std::uint16_t>(ToUInt8(bytes_[offset_ + 1]) << 8u);
        offset_ += sizeof(value);
        return true;
    }

    bool MessageReader::ReadUInt32(std::uint32_t &value) noexcept {
        if (!CanRead(sizeof(value))) {
            return false;
        }

        value = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            value |= static_cast<std::uint32_t>(ToUInt8(bytes_[offset_++])) << shift;
        }
        return true;
    }

    bool MessageReader::ReadUInt64(std::uint64_t &value) noexcept {
        if (!CanRead(sizeof(value))) {
            return false;
        }

        value = 0;
        for (int shift = 0; shift < 64; shift += 8) {
            value |= static_cast<std::uint64_t>(ToUInt8(bytes_[offset_++])) << shift;
        }
        return true;
    }

    bool MessageReader::ReadFloat(float &value) noexcept {
        std::uint32_t bits = 0;
        if (!ReadUInt32(bits)) {
            return false;
        }

        value = std::bit_cast<float>(bits);
        return true;
    }

    bool MessageReader::ReadBool(bool &value) noexcept {
        std::uint8_t raw = 0;
        if (!ReadUInt8(raw)) {
            return false;
        }

        value = raw != 0;
        return true;
    }

    bool MessageReader::ReadBytes(std::span<std::byte> out_bytes) noexcept {
        if (!CanRead(out_bytes.size())) {
            return false;
        }

        for (std::byte &value: out_bytes) {
            value = bytes_[offset_++];
        }
        return true;
    }

    bool MessageReader::ReadSizedBytes(std::span<const std::byte> &out_bytes) noexcept {
        std::uint16_t size = 0;
        if (!ReadUInt16(size) || !CanRead(size)) {
            return false;
        }

        out_bytes = bytes_.subspan(offset_, size);
        offset_ += size;
        return true;
    }

    std::size_t MessageReader::Remaining() const noexcept {
        return offset_ <= bytes_.size() ? bytes_.size() - offset_ : 0;
    }

    bool MessageReader::CanRead(std::size_t bytes) const noexcept {
        return offset_ <= bytes_.size() && bytes <= bytes_.size() - offset_;
    }

    bool ParsePacket(std::span<const std::byte> bytes, PacketHeader &out_header,
                     std::span<const std::byte> &out_payload) noexcept {
        if (bytes.size() < kPacketHeaderWireSize) {
            return false;
        }

        MessageReader reader(bytes);
        std::uint16_t message_type = 0;
        if (!reader.ReadUInt32(out_header.magic) || !reader.ReadUInt16(out_header.protocol_version) ||
            !reader.ReadUInt16(message_type) || !reader.ReadUInt32(out_header.sequence) ||
            !reader.ReadUInt32(out_header.ack) || !reader.ReadUInt32(out_header.tick) ||
            !reader.ReadUInt16(out_header.payload_size)) {
            return false;
        }

        if (out_header.magic != kNetworkMagic || out_header.protocol_version != kNetworkProtocolVersion ||
            out_header.payload_size > kMaxPacketPayloadBytes ||
            bytes.size() != kPacketHeaderWireSize + out_header.payload_size) {
            return false;
        }

        out_header.message_type = static_cast<NetMessageType>(message_type);
        out_payload = bytes.subspan(kPacketHeaderWireSize, out_header.payload_size);
        return true;
    }
} // namespace CoreEngine
