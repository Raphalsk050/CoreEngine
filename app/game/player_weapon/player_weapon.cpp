#include "player_weapon.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>

#include "core/debug/debug_draw.h"
#include "core/network/network_gameplay_event.h"
#include "core/network/player/network_player_system.h"
#include "game/camera/third_person_camera_controller.h"
#include "game/input/player_input_bindings.h"

namespace Game {
    namespace {
        constexpr float kFireTraceDistance = 100.0f;
        constexpr float kFireIntervalSeconds = 0.12f;
        constexpr CoreEngine::Math::Vec3 kRemoteWeaponOffset{0.0f, 1.35f, 0.0f};
        constexpr CoreEngine::NetworkGameplayEventTypeId kFireTraceGameplayEvent = 0xB0070001u;
        constexpr std::size_t kFireTracePayloadBytes = 24;

        [[nodiscard]] CoreEngine::Math::Vec3 DirectionFromCommand(
            const CoreEngine::PlayerInputCommand &command) noexcept {
            const float cos_pitch = std::cos(command.look_pitch);
            return CoreEngine::Math::Normalize(CoreEngine::Math::Vec3{
                cos_pitch * std::sin(command.look_yaw),
                std::sin(command.look_pitch),
                cos_pitch * std::cos(command.look_yaw),
            });
        }

        void WriteFloat(std::array<std::byte, kFireTracePayloadBytes> &payload,
                        std::size_t &offset,
                        float value) noexcept {
            const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            for (int shift = 0; shift < 32; shift += 8) {
                payload[offset++] = static_cast<std::byte>((bits >> shift) & 0xffu);
            }
        }

        [[nodiscard]] bool ReadFloat(std::span<const std::byte> payload,
                                     std::size_t &offset,
                                     float &value) noexcept {
            if (payload.size() - offset < sizeof(std::uint32_t)) {
                return false;
            }

            std::uint32_t bits = 0;
            for (int shift = 0; shift < 32; shift += 8) {
                bits |= static_cast<std::uint32_t>(payload[offset++]) << shift;
            }
            value = std::bit_cast<float>(bits);
            return true;
        }

        [[nodiscard]] std::array<std::byte, kFireTracePayloadBytes> BuildFireTracePayload(
            const CoreEngine::Math::Vec3 &origin,
            const CoreEngine::Math::Vec3 &direction) noexcept {
            std::array<std::byte, kFireTracePayloadBytes> payload{};
            std::size_t offset = 0;
            WriteFloat(payload, offset, origin.x);
            WriteFloat(payload, offset, origin.y);
            WriteFloat(payload, offset, origin.z);
            WriteFloat(payload, offset, direction.x);
            WriteFloat(payload, offset, direction.y);
            WriteFloat(payload, offset, direction.z);
            return payload;
        }

        [[nodiscard]] bool ReadFireTracePayload(std::span<const std::byte> payload,
                                                CoreEngine::Math::Vec3 &origin,
                                                CoreEngine::Math::Vec3 &direction) noexcept {
            if (payload.size() != kFireTracePayloadBytes) {
                return false;
            }

            std::size_t offset = 0;
            return ReadFloat(payload, offset, origin.x) &&
                   ReadFloat(payload, offset, origin.y) &&
                   ReadFloat(payload, offset, origin.z) &&
                   ReadFloat(payload, offset, direction.x) &&
                   ReadFloat(payload, offset, direction.y) &&
                   ReadFloat(payload, offset, direction.z);
        }
    } // namespace

    void PlayerWeapon::AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept {
        camera_controller_ = &camera_controller;
    }

    void PlayerWeapon::DetachCameraController() noexcept {
        camera_controller_ = nullptr;
        fire_cursors_ = {};
    }

    void PlayerWeapon::FixedUpdate(const CoreEngine::FixedFrameContext &frame) noexcept {
        ProcessReplicatedFireTraces(frame);

        const CoreEngine::NetworkEntityId local_network_id = frame.network_players.LocalPlayerNetworkId();

        for (const CoreEngine::QueuedPlayerInputCommand &queued: frame.multiplayer.InputCommands()) {
            if (!queued.command.IsActionDown(PlayerCommandActions::Fire) ||
                !TryConsumeFire(queued, frame.simulation.simulation_time)) {
                continue;
            }

            const bool local_player = queued.player_network_id == local_network_id;
            const CoreEngine::Node shooter = frame.multiplayer.FindNode(queued.player_network_id);
            const CoreEngine::Math::Vec3 origin = ResolveFireOrigin(shooter, local_player);
            const CoreEngine::Math::Vec3 direction = ResolveFireDirection(queued, local_player);
            DrawFireTrace(frame.debug_draw, origin, direction, false);
            BroadcastFireTrace(frame, queued, origin, direction);
        }
    }

    void PlayerWeapon::Update(const CoreEngine::FrameContext &frame) noexcept {
        if (frame.multiplayer.Role() != CoreEngine::NetworkRole::Client ||
            camera_controller_ == nullptr ||
            !frame.input_system.WasActionPressed(PlayerInputActions::Fire)) {
            return;
        }

        const CoreEngine::Node camera_node = camera_controller_->GetCameraNode();
        const CoreEngine::Math::Vec3 origin =
                camera_node.IsValid() ? camera_node.GetWorldPosition() : CoreEngine::Math::Vec3{};
        DrawFireTrace(frame.debug_draw, origin, camera_controller_->Forward(), true);
    }

    bool PlayerWeapon::TryConsumeFire(const CoreEngine::QueuedPlayerInputCommand &queued,
                                      double simulation_time) noexcept {
        if (queued.player_network_id == 0 || queued.command.sequence == 0) {
            return false;
        }

        FireCursor *empty_cursor = nullptr;
        FireCursor *oldest_cursor = &fire_cursors_.front();
        for (FireCursor &cursor: fire_cursors_) {
            if (cursor.network_id == queued.player_network_id) {
                if (queued.command.sequence <= cursor.last_sequence ||
                    simulation_time < cursor.next_fire_time) {
                    return false;
                }

                cursor.last_sequence = queued.command.sequence;
                cursor.next_fire_time = simulation_time + kFireIntervalSeconds;
                return true;
            }

            if (cursor.network_id == 0 && empty_cursor == nullptr) {
                empty_cursor = &cursor;
            }
            if (cursor.next_fire_time < oldest_cursor->next_fire_time) {
                oldest_cursor = &cursor;
            }
        }

        FireCursor &cursor = empty_cursor != nullptr ? *empty_cursor : *oldest_cursor;
        cursor = FireCursor{
            .network_id = queued.player_network_id,
            .last_sequence = queued.command.sequence,
            .next_fire_time = simulation_time + kFireIntervalSeconds,
        };
        return true;
    }

    CoreEngine::Math::Vec3 PlayerWeapon::ResolveFireOrigin(CoreEngine::Node shooter,
                                                           bool local_player) const noexcept {
        if (local_player && camera_controller_ != nullptr) {
            const CoreEngine::Node camera_node = camera_controller_->GetCameraNode();
            if (camera_node.IsValid()) {
                return camera_node.GetWorldPosition();
            }
        }

        if (!shooter.IsValid()) {
            return {};
        }

        return shooter.GetWorldPosition() + shooter.GetWorldRotation() * kRemoteWeaponOffset;
    }

    CoreEngine::Math::Vec3 PlayerWeapon::ResolveFireDirection(
        const CoreEngine::QueuedPlayerInputCommand &queued,
        bool local_player) const noexcept {
        if (local_player && camera_controller_ != nullptr) {
            return camera_controller_->Forward();
        }

        CoreEngine::Math::Vec3 direction = DirectionFromCommand(queued.command);
        if (CoreEngine::Math::LengthSquared(direction) <= CoreEngine::Math::Epsilon) {
            return {0.0f, 0.0f, 1.0f};
        }

        return direction;
    }

    void PlayerWeapon::DrawFireTrace(CoreEngine::DebugDrawSystem &debug_draw,
                                     const CoreEngine::Math::Vec3 &origin,
                                     const CoreEngine::Math::Vec3 &direction,
                                     bool predicted) const {
        if (CoreEngine::Math::LengthSquared(direction) <= CoreEngine::Math::Epsilon) {
            return;
        }

        debug_draw.DrawLineTrace(origin,
                                 origin + CoreEngine::Math::Normalize(direction) * kFireTraceDistance,
                                 CoreEngine::DebugTraceResult{},
                                 CoreEngine::DebugDrawStyle{
                                     .color = predicted
                                                  ? CoreEngine::Math::Vec4{1.0f, 0.75f, 0.15f, 1.0f}
                                                  : CoreEngine::Math::Vec4{0.1f, 0.85f, 1.0f, 1.0f},
                                     .duration_seconds = predicted ? 0.08f : 0.16f,
                                     .depth_test = false,
                                 });
    }

    void PlayerWeapon::ProcessReplicatedFireTraces(
        const CoreEngine::FixedFrameContext &frame) const noexcept {
        const CoreEngine::NetworkEntityId local_network_id = frame.network_players.LocalPlayerNetworkId();
        for (const CoreEngine::NetworkGameplayEvent &event: frame.multiplayer.GameplayEvents()) {
            if (event.event_type != kFireTraceGameplayEvent ||
                event.source_network_id == local_network_id) {
                continue;
            }

            CoreEngine::Math::Vec3 origin{};
            CoreEngine::Math::Vec3 direction{};
            if (!ReadFireTracePayload(event.Payload(), origin, direction)) {
                continue;
            }

            DrawFireTrace(frame.debug_draw, origin, direction, false);
        }
    }

    void PlayerWeapon::BroadcastFireTrace(const CoreEngine::FixedFrameContext &frame,
                                          const CoreEngine::QueuedPlayerInputCommand &queued,
                                          const CoreEngine::Math::Vec3 &origin,
                                          const CoreEngine::Math::Vec3 &direction) const noexcept {
        if (frame.multiplayer.Role() != CoreEngine::NetworkRole::Host ||
            frame.multiplayer.SessionState() != CoreEngine::NetworkSessionState::Connected) {
            return;
        }

        const std::array<std::byte, kFireTracePayloadBytes> payload = BuildFireTracePayload(origin, direction);
        CoreEngine::NetworkGameplayEvent event{
            .event_type = kFireTraceGameplayEvent,
            .source_network_id = queued.player_network_id,
            .sequence = queued.command.sequence,
            .server_tick = frame.simulation.tick,
        };
        if (!event.SetPayload(payload)) {
            return;
        }

        (void) frame.multiplayer.BroadcastGameplayEvent(event, queued.peer);
    }
} // Game
