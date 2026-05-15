#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/network/replication/snapshot_writer.h"

namespace CoreEngine {
    struct NetworkSnapshotBuildDesc {
        std::uint32_t server_tick = 0;
        std::uint32_t snapshot_sequence = 0;
        std::uint32_t last_processed_input_sequence = 0;
    };

    struct NetworkSnapshotBuildResult {
        std::uint32_t server_tick = 0;
        std::uint32_t snapshot_sequence = 0;
        std::uint16_t entity_count = 0;
        std::uint32_t bytes_estimate = 0;
    };

    /**
     * @brief Builds world snapshot payloads from replicated state batches.
     *
     * Responsibility: serialize snapshot metadata and component payloads while
     * keeping packet header creation inside NetworkSystem.
     */
    class NetworkSnapshotBuilder {
    public:
        [[nodiscard]] bool BuildTransformSnapshot(const NetworkSnapshotBuildDesc &desc,
                                                  std::span<const NetworkTransformSnapshot> transforms,
                                                  MessageWriter &writer,
                                                  NetworkSnapshotBuildResult &out_result) const;
    };
} // namespace CoreEngine
