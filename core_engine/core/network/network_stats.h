#pragma once

#include <cstdint>

namespace CoreEngine {
    struct NetworkConnectionMetrics {
        int ping_ms = -1;
        int jitter_ms = 0;
        int queue_time_ms = 0;
        std::uint32_t pending_unreliable_bytes = 0;
        std::uint32_t pending_reliable_bytes = 0;
        std::uint32_t send_rate_bytes_per_second = 0;
        float packet_loss = 0.0f;
        bool valid = false;
    };

    struct NetworkStats {
        std::uint64_t bytes_in = 0;
        std::uint64_t bytes_out = 0;
        std::uint64_t packets_in = 0;
        std::uint64_t packets_out = 0;
        std::uint64_t packets_dropped = 0;
        std::uint64_t input_commands_received = 0;
        std::uint64_t input_commands_dropped = 0;
        std::uint64_t input_commands_duplicated = 0;
        std::uint64_t snapshots_sent = 0;
        std::uint64_t snapshots_received = 0;
        std::uint64_t snapshots_dropped = 0;
        std::uint64_t prediction_corrections = 0;
        std::uint64_t prediction_hard_snaps = 0;
        std::uint32_t avg_snapshot_size_bytes = 0;
        int jitter_ms = 0;
        std::uint32_t last_snapshot_tick = 0;
        std::uint32_t last_input_tick = 0;
        int ping_ms = -1;
        int protocol_ping_ms = -1;
        int protocol_jitter_ms = 0;
        int transport_queue_time_ms = 0;
        std::uint32_t transport_pending_unreliable_bytes = 0;
        std::uint32_t transport_pending_reliable_bytes = 0;
        std::uint32_t transport_send_rate_bytes_per_second = 0;
        float packet_loss = 0.0f;

        void Reset() noexcept {
            *this = NetworkStats{};
        }
    };
} // namespace CoreEngine
