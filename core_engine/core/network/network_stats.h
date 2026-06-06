#pragma once

#include <cstdint>

namespace CoreEngine {
    struct NetworkStats {
        std::uint64_t bytes_in = 0;
        std::uint64_t bytes_out = 0;
        std::uint64_t packets_in = 0;
        std::uint64_t packets_out = 0;
        std::uint64_t packets_dropped = 0;
        std::uint32_t last_snapshot_tick = 0;
        std::uint32_t last_input_tick = 0;
        int ping_ms = -1;
        float packet_loss = 0.0f;

        void Reset() noexcept { *this = NetworkStats{}; }
    };
} // namespace CoreEngine
