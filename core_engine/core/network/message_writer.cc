#include "core/network/message_writer.h"

#include <bit>

namespace CoreEngine {
    namespace {
        constexpr std::size_t kPayloadSizeOffset = 20;
    }

    MessageWriter::MessageWriter(std::size_t reserve_bytes) { buffer_.reserve(reserve_bytes); }

    void MessageWriter::Reset() { buffer_.clear(); }

    bool MessageWriter::Begin(NetMessageType message_type, std::uint32_t sequence, std::uint32_t ack,
                              std::uint32_t tick) {
        Reset();
        return WriteUInt32(kNetworkMagic) && WriteUInt16(kNetworkProtocolVersion) &&
               WriteUInt16(static_cast<std::uint16_t>(message_type)) && WriteUInt32(sequence) && WriteUInt32(ack) &&
               WriteUInt32(tick) && WriteUInt16(0);
    }

    bool MessageWriter::Finalize() {
        if (buffer_.size() < kPacketHeaderWireSize) {
            return false;
        }

        const std::size_t payload_size = buffer_.size() - kPacketHeaderWireSize;
        if (payload_size > kMaxPacketPayloadBytes) {
            return false;
        }

        PatchUInt16(kPayloadSizeOffset, static_cast<std::uint16_t>(payload_size));
        return true;
    }

    bool MessageWriter::WriteUInt8(std::uint8_t value) {
        if (!CanWrite(sizeof(value))) {
            return false;
        }

        buffer_.push_back(static_cast<std::byte>(value));
        return true;
    }

    bool MessageWriter::WriteUInt16(std::uint16_t value) {
        if (!CanWrite(sizeof(value))) {
            return false;
        }

        buffer_.push_back(static_cast<std::byte>(value & 0xffu));
        buffer_.push_back(static_cast<std::byte>((value >> 8u) & 0xffu));
        return true;
    }

    bool MessageWriter::WriteUInt32(std::uint32_t value) {
        if (!CanWrite(sizeof(value))) {
            return false;
        }

        for (int shift = 0; shift < 32; shift += 8) {
            buffer_.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
        }
        return true;
    }

    bool MessageWriter::WriteUInt64(std::uint64_t value) {
        if (!CanWrite(sizeof(value))) {
            return false;
        }

        for (int shift = 0; shift < 64; shift += 8) {
            buffer_.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
        }
        return true;
    }

    bool MessageWriter::WriteFloat(float value) { return WriteUInt32(std::bit_cast<std::uint32_t>(value)); }

    bool MessageWriter::WriteBool(bool value) { return WriteUInt8(value ? 1u : 0u); }

    bool MessageWriter::WriteBytes(std::span<const std::byte> bytes) {
        if (!CanWrite(bytes.size())) {
            return false;
        }

        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
        return true;
    }

    bool MessageWriter::WriteSizedBytes(std::span<const std::byte> bytes) {
        if (bytes.size() > UINT16_MAX) {
            return false;
        }

        return WriteUInt16(static_cast<std::uint16_t>(bytes.size())) && WriteBytes(bytes);
    }

    bool MessageWriter::CanWrite(std::size_t bytes) const noexcept {
        if (buffer_.size() + bytes < buffer_.size()) {
            return false;
        }

        if (buffer_.size() < kPacketHeaderWireSize) {
            return buffer_.size() + bytes <= kPacketHeaderWireSize;
        }

        return buffer_.size() + bytes - kPacketHeaderWireSize <= kMaxPacketPayloadBytes;
    }

    void MessageWriter::PatchUInt16(std::size_t offset, std::uint16_t value) noexcept {
        buffer_[offset] = static_cast<std::byte>(value & 0xffu);
        buffer_[offset + 1] = static_cast<std::byte>((value >> 8u) & 0xffu);
    }
} // namespace CoreEngine
