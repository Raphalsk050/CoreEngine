#include "core/network/diagnostics/network_diagnostics_logger.h"

#include <chrono>
#include <ctime>
#include <system_error>

#include "core/network/message_reader.h"

namespace CoreEngine {
    namespace {
        [[nodiscard]] const char *ToString(NetMessageType type) noexcept {
            switch (type) {
                case NetMessageType::ClientHello:
                    return "ClientHello";
                case NetMessageType::ServerHello:
                    return "ServerHello";
                case NetMessageType::AuthTicket:
                    return "AuthTicket";
                case NetMessageType::AuthAccepted:
                    return "AuthAccepted";
                case NetMessageType::AuthRejected:
                    return "AuthRejected";
                case NetMessageType::InputCommand:
                    return "InputCommand";
                case NetMessageType::WorldSnapshot:
                    return "WorldSnapshot";
                case NetMessageType::EntitySpawn:
                    return "EntitySpawn";
                case NetMessageType::EntityDespawn:
                    return "EntityDespawn";
                case NetMessageType::Ping:
                    return "Ping";
                case NetMessageType::Pong:
                    return "Pong";
                case NetMessageType::Disconnect:
                    return "Disconnect";
                case NetMessageType::GameplayEvent:
                    return "GameplayEvent";
            }

            return "Unknown";
        }

        [[nodiscard]] const char *ToString(NetworkEventType type) noexcept {
            switch (type) {
                case NetworkEventType::None:
                    return "None";
                case NetworkEventType::LobbyCreated:
                    return "LobbyCreated";
                case NetworkEventType::LobbyEntered:
                    return "LobbyEntered";
                case NetworkEventType::LobbyJoinRequested:
                    return "LobbyJoinRequested";
                case NetworkEventType::LobbyLeft:
                    return "LobbyLeft";
                case NetworkEventType::PeerConnecting:
                    return "PeerConnecting";
                case NetworkEventType::PeerConnected:
                    return "PeerConnected";
                case NetworkEventType::PeerDisconnected:
                    return "PeerDisconnected";
                case NetworkEventType::LobbyOwnerChanged:
                    return "LobbyOwnerChanged";
                case NetworkEventType::PacketReceived:
                    return "PacketReceived";
                case NetworkEventType::AuthAccepted:
                    return "AuthAccepted";
                case NetworkEventType::AuthRejected:
                    return "AuthRejected";
            }

            return "Unknown";
        }

        [[nodiscard]] const char *ToString(SendMode mode) noexcept {
            switch (mode) {
                case SendMode::Unreliable:
                    return "Unreliable";
                case SendMode::UnreliableNoDelay:
                    return "UnreliableNoDelay";
                case SendMode::Reliable:
                    return "Reliable";
                case SendMode::ReliableNoNagle:
                    return "ReliableNoNagle";
            }

            return "Unknown";
        }

        [[nodiscard]] const char *ToString(ReconciliationAction action) noexcept {
            switch (action) {
                case ReconciliationAction::None:
                    return "None";
                case ReconciliationAction::SmoothCorrection:
                    return "SmoothCorrection";
                case ReconciliationAction::HardSnap:
                    return "HardSnap";
                case ReconciliationAction::MissingPrediction:
                    return "MissingPrediction";
            }

            return "Unknown";
        }

        [[nodiscard]] std::tm LocalTime(std::time_t value) noexcept {
            std::tm out{};
#if defined(_WIN32)
            localtime_s(&out, &value);
#else
            localtime_r(&value, &out);
#endif
            return out;
        }
    } // namespace

    bool NetworkDiagnosticsLogger::Initialize(const std::filesystem::path &path) {
        Shutdown();
        path_ = path;

        std::error_code error;
        if (!path_.parent_path().empty()) {
            std::filesystem::create_directories(path_.parent_path(), error);
        }

        stream_.open(path_, std::ios::out | std::ios::trunc);
        if (!stream_.is_open()) {
            return false;
        }

        line_count_ = 0;
        return true;
    }

    void NetworkDiagnosticsLogger::Shutdown() noexcept {
        if (!stream_.is_open()) {
            return;
        }

        stream_.flush();
        stream_.close();
    }

    void NetworkDiagnosticsLogger::WriteHeader(std::uint64_t local_user_id,
                                               std::string_view local_addresses,
                                               std::uint16_t protocol_version,
                                               std::uint32_t build_hash) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("HEADER");
        stream_ << "local_user=" << local_user_id
                << " local_addresses=\"" << local_addresses << '"'
                << " protocol=" << protocol_version
                << " build_hash=" << build_hash;
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogSessionAction(std::string_view action, const NetworkSession &session) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("SESSION");
        stream_ << "action=" << action
                << " kind=" << ToString(session.Kind())
                << " role=" << ToString(session.Role())
                << " state=" << ToString(session.State())
                << " lobby=" << session.LobbyId()
                << " owner=" << session.LobbyOwnerSteamId()
                << " local=" << session.LocalSteamId()
                << " peers=" << session.Peers().size()
                << " reason=" << ToString(session.LastDisconnectReason());
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogNetworkEvent(const NetworkEvent &event) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("EVENT");
        stream_ << "type=" << ToString(event.type)
                << " peer=" << event.peer
                << " remote_user=" << event.remote_steam_id
                << " lobby=" << event.lobby_id
                << " owner=" << event.lobby_owner_id
                << " message=" << ToString(event.message_type)
                << " seq=" << event.sequence
                << " ack=" << event.ack
                << " tick=" << event.tick
                << " payload=" << event.payload.size()
                << " reason=" << ToString(event.disconnect_reason);
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogMalformedInboundPacket(PeerId peer,
                                                             std::uint64_t remote_user_id,
                                                             std::size_t packet_bytes) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("PACKET_IN_BAD");
        stream_ << "peer=" << peer
                << " remote_user=" << remote_user_id
                << " bytes=" << packet_bytes;
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogInboundPacket(const NetworkEvent &event, std::size_t packet_bytes) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("PACKET_IN");
        stream_ << "peer=" << event.peer
                << " remote_user=" << event.remote_steam_id
                << " message=" << ToString(event.message_type)
                << " seq=" << event.sequence
                << " ack=" << event.ack
                << " tick=" << event.tick
                << " packet_bytes=" << packet_bytes
                << " payload=" << event.payload.size();
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogOutboundPacket(PeerId peer,
                                                     std::span<const std::byte> packet,
                                                     SendMode mode,
                                                     bool sent) {
        if (!stream_.is_open()) {
            return;
        }

        PacketHeader header;
        std::span<const std::byte> payload;
        const bool parsed = ParsePacket(packet, header, payload);

        WritePrefix(sent ? "PACKET_OUT" : "PACKET_OUT_FAIL");
        stream_ << "peer=" << peer
                << " mode=" << ToString(mode)
                << " parsed=" << (parsed ? "yes" : "no")
                << " packet_bytes=" << packet.size();
        if (parsed) {
            stream_ << " message=" << ToString(header.message_type)
                    << " seq=" << header.sequence
                    << " ack=" << header.ack
                    << " tick=" << header.tick
                    << " payload=" << payload.size();
        }
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogSummary(const NetworkSession &session,
                                              const NetworkStats &stats,
                                              const NetworkDiagnosticsRuntimeState &runtime_state) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("SUMMARY");
        stream_ << "tick=" << runtime_state.local_tick
                << " kind=" << ToString(session.Kind())
                << " role=" << ToString(session.Role())
                << " state=" << ToString(session.State())
                << " peers=" << session.Peers().size()
                << " ping=" << stats.ping_ms
                << " jitter=" << stats.jitter_ms
                << " protocol_ping=" << stats.protocol_ping_ms
                << " protocol_jitter=" << stats.protocol_jitter_ms
                << " queue_ms=" << stats.transport_queue_time_ms
                << " pending_unreliable=" << stats.transport_pending_unreliable_bytes
                << " pending_reliable=" << stats.transport_pending_reliable_bytes
                << " send_rate=" << stats.transport_send_rate_bytes_per_second
                << " loss=" << stats.packet_loss
                << " bytes_in=" << stats.bytes_in
                << " bytes_out=" << stats.bytes_out
                << " packets_in=" << stats.packets_in
                << " packets_out=" << stats.packets_out
                << " packets_dropped=" << stats.packets_dropped
                << " packets_send_failed=" << stats.packets_send_failed
                << " input_recv=" << stats.input_commands_received
                << " input_drop=" << stats.input_commands_dropped
                << " input_dup=" << stats.input_commands_duplicated
                << " snapshots_sent=" << stats.snapshots_sent
                << " snapshots_recv=" << stats.snapshots_received
                << " snapshots_drop=" << stats.snapshots_dropped
                << " corrections=" << stats.prediction_corrections
                << " hard_snaps=" << stats.prediction_hard_snaps
                << " avg_snapshot=" << stats.avg_snapshot_size_bytes
                << " last_input_tick=" << stats.last_input_tick
                << " last_snapshot_tick=" << stats.last_snapshot_tick
                << " pending_acks=" << runtime_state.pending_packet_acks
                << " pending_pings=" << runtime_state.pending_protocol_pings
                << " pending_transport_packets=" << runtime_state.pending_transport_packets;
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogConnectionDetails(std::string_view details) {
        if (!stream_.is_open() || details.empty()) {
            return;
        }

        WritePrefix("DETAIL_BEGIN");
        EndLine();
        stream_ << details;
        if (details.back() != '\n') {
            stream_ << '\n';
        }
        WritePrefix("DETAIL_END");
        EndLine();
    }

    void NetworkDiagnosticsLogger::LogPredictionCorrection(ReconciliationAction action,
                                                           float position_error,
                                                           std::uint32_t confirmed_sequence,
                                                           std::uint32_t latest_sequence,
                                                           std::size_t replay_count) {
        if (!stream_.is_open()) {
            return;
        }

        WritePrefix("PREDICTION");
        stream_ << "action=" << ToString(action)
                << " error=" << position_error
                << " confirmed_sequence=" << confirmed_sequence
                << " latest_sequence=" << latest_sequence
                << " replay_count=" << replay_count;
        EndLine();
    }

    void NetworkDiagnosticsLogger::WritePrefix(std::string_view tag) {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        const auto local_time = LocalTime(time);
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch()).count() % 1000;

        char timestamp[32]{};
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
        stream_ << timestamp << '.' << millis << " [" << tag << "] ";
    }

    void NetworkDiagnosticsLogger::EndLine() {
        stream_ << '\n';
        ++line_count_;
        if ((line_count_ % 64u) == 0u) {
            stream_.flush();
        }
    }
} // namespace CoreEngine
