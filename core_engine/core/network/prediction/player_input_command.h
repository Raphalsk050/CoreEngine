#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "core/network/message_reader.h"
#include "core/network/message_writer.h"

namespace CoreEngine {
    enum class PlayerInputButton : std::uint32_t {
        Jump = 1u << 0u,
        Crouch = 1u << 1u,
        Sprint = 1u << 2u,
        Fire = 1u << 3u,
        AltFire = 1u << 4u,
        Reload = 1u << 5u,
        Interact = 1u << 6u,
        UseGadget = 1u << 7u,
        Capture = 1u << 8u,
        OpenInventory = 1u << 9u,
    };

    struct PlayerInputCommand {
        std::uint32_t client_tick = 0;
        std::uint16_t sub_tick = 0;
        std::uint32_t sequence = 0;
        std::uint32_t last_received_server_snapshot_tick = 0;
        float move_x = 0.0f;
        float move_y = 0.0f;
        float look_yaw = 0.0f;
        float look_pitch = 0.0f;
        std::uint32_t buttons = 0;
        std::uint8_t selected_slot = 0;

        [[nodiscard]] bool IsButtonDown(PlayerInputButton button) const noexcept {
            return (buttons & static_cast<std::uint32_t>(button)) != 0;
        }

        [[nodiscard]] float SubTickAlpha() const noexcept {
            return static_cast<float>(sub_tick) / 65535.0f;
        }
    };

    /**
     * @brief Carries gameplay input before network timing and sequencing are applied.
     *
     * Responsibility: let application code describe player intent while the
     * multiplayer layer owns protocol stamps such as tick, sub-tick, and sequence.
     */
    struct LocalPlayerInputDesc {
        float move_x = 0.0f;
        float move_y = 0.0f;
        float look_yaw = 0.0f;
        float look_pitch = 0.0f;
        std::uint32_t buttons = 0;
        std::uint8_t selected_slot = 0;
    };

    inline constexpr std::uint8_t kMaxInputCommandsPerPacket = 8;

    struct PlayerInputCommandBatch {
        std::array<PlayerInputCommand, kMaxInputCommandsPerPacket> commands{};
        std::uint8_t count = 0;
    };

    [[nodiscard]] inline bool WritePlayerInputCommand(MessageWriter &writer,
                                                      const PlayerInputCommand &command) {
        return writer.WriteUInt32(command.client_tick) &&
               writer.WriteUInt16(command.sub_tick) &&
               writer.WriteUInt32(command.sequence) &&
               writer.WriteUInt32(command.last_received_server_snapshot_tick) &&
               writer.WriteFloat(command.move_x) &&
               writer.WriteFloat(command.move_y) &&
               writer.WriteFloat(command.look_yaw) &&
               writer.WriteFloat(command.look_pitch) &&
               writer.WriteUInt32(command.buttons) &&
               writer.WriteUInt8(command.selected_slot);
    }

    [[nodiscard]] inline bool ReadPlayerInputCommand(MessageReader &reader,
                                                     PlayerInputCommand &command) noexcept {
        return reader.ReadUInt32(command.client_tick) &&
               reader.ReadUInt16(command.sub_tick) &&
               reader.ReadUInt32(command.sequence) &&
               reader.ReadUInt32(command.last_received_server_snapshot_tick) &&
               reader.ReadFloat(command.move_x) &&
               reader.ReadFloat(command.move_y) &&
               reader.ReadFloat(command.look_yaw) &&
               reader.ReadFloat(command.look_pitch) &&
               reader.ReadUInt32(command.buttons) &&
               reader.ReadUInt8(command.selected_slot);
    }

    [[nodiscard]] inline bool WritePlayerInputCommandBatch(MessageWriter &writer,
                                                           std::span<const PlayerInputCommand> commands) {
        if (commands.size() > kMaxInputCommandsPerPacket || !writer.WriteUInt8(static_cast<std::uint8_t>(commands.size()))) {
            return false;
        }

        for (const PlayerInputCommand &command: commands) {
            if (!WritePlayerInputCommand(writer, command)) {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] inline bool ReadPlayerInputCommandBatch(MessageReader &reader,
                                                          PlayerInputCommandBatch &batch) noexcept {
        std::uint8_t count = 0;
        if (!reader.ReadUInt8(count) || count > kMaxInputCommandsPerPacket) {
            return false;
        }

        batch.count = count;
        for (std::uint8_t i = 0; i < count; ++i) {
            if (!ReadPlayerInputCommand(reader, batch.commands[i])) {
                batch.count = 0;
                return false;
            }
        }

        return true;
    }
} // namespace CoreEngine
