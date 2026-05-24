#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

#include "core/network/network_message.h"
#include "core/network/network_session.h"
#include "core/network/network_stats.h"
#include "core/network/prediction/reconciliation.h"

namespace CoreEngine {
    struct NetworkDiagnosticsRuntimeState {
        std::uint32_t local_tick = 0;
        std::size_t pending_packet_acks = 0;
        std::size_t pending_protocol_pings = 0;
        std::size_t pending_transport_packets = 0;
    };

    /**
     * @brief Writes high-volume network diagnostics to a dedicated file.
     *
     * Responsibility: capture transport, protocol, replication, and prediction
     * evidence without routing packet-level traces through the interactive console.
     */
    class NetworkDiagnosticsLogger final {
    public:
        [[nodiscard]] bool Initialize(const std::filesystem::path &path);

        void Shutdown() noexcept;

        [[nodiscard]] bool IsOpen() const noexcept {
            return stream_.is_open();
        }

        [[nodiscard]] const std::filesystem::path &Path() const noexcept {
            return path_;
        }

        void WriteHeader(std::uint64_t local_user_id,
                         std::string_view local_addresses,
                         std::uint16_t protocol_version,
                         std::uint32_t build_hash);

        void LogSessionAction(std::string_view action, const NetworkSession &session);

        void LogNetworkEvent(const NetworkEvent &event);

        void LogMalformedInboundPacket(PeerId peer,
                                       std::uint64_t remote_user_id,
                                       std::size_t packet_bytes);

        void LogInboundPacket(const NetworkEvent &event, std::size_t packet_bytes);

        void LogOutboundPacket(PeerId peer,
                               std::span<const std::byte> packet,
                               SendMode mode,
                               bool sent);

        void LogSummary(const NetworkSession &session,
                        const NetworkStats &stats,
                        const NetworkDiagnosticsRuntimeState &runtime_state);

        void LogConnectionDetails(std::string_view details);

        void LogPredictionCorrection(ReconciliationAction action,
                                     float position_error,
                                     std::uint32_t confirmed_sequence,
                                     std::uint32_t latest_sequence,
                                     std::size_t replay_count);

    private:
        void WritePrefix(std::string_view tag);

        void EndLine();

        std::filesystem::path path_;
        std::ofstream stream_;
        std::uint64_t line_count_ = 0;
    };
} // namespace CoreEngine
