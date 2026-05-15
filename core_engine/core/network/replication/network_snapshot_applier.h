#pragma once

#include <cstdint>
#include <vector>

#include "core/network/message_reader.h"
#include "core/network/replication/snapshot_reader.h"

namespace CoreEngine {
    struct NetworkSnapshotApplyResult {
        std::uint32_t server_tick = 0;
        std::uint32_t snapshot_sequence = 0;
        std::uint32_t last_processed_input_sequence = 0;
        std::uint16_t entity_count = 0;
    };

    /**
     * @brief Parses inbound world snapshots into bounded state batches.
     *
     * Responsibility: validate snapshot metadata and expose decoded component
     * state to interpolation/reconciliation systems.
     */
    class NetworkSnapshotApplier {
    public:
        [[nodiscard]] bool ReadTransformSnapshot(MessageReader &reader,
                                                 std::vector<NetworkTransformSnapshot> &out_transforms,
                                                 NetworkSnapshotApplyResult &out_result) const;
    };
} // namespace CoreEngine
