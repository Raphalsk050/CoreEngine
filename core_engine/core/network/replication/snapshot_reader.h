#pragma once

#include <vector>

#include "core/network/message_reader.h"
#include "core/network/replication/snapshot_writer.h"

namespace CoreEngine {
    /**
     * @brief Deserializes bounded transform snapshot batches.
     *
     * Responsibility: reject malformed or oversized snapshot payloads before
     * they can mutate ECS state.
     */
    class SnapshotReader {
    public:
        explicit SnapshotReader(MessageReader &reader) noexcept;

        bool ReadTransform(NetworkTransformSnapshot &snapshot) noexcept;

        bool ReadTransformBatch(std::vector<NetworkTransformSnapshot> &snapshots,
                                std::uint16_t max_transforms = kMaxSnapshotTransformsPerPacket) noexcept;

    private:
        MessageReader &reader_;
    };
} // namespace CoreEngine
