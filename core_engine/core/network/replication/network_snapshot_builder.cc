#include "core/network/replication/network_snapshot_builder.h"

namespace CoreEngine {
    bool NetworkSnapshotBuilder::BuildTransformSnapshot(const NetworkSnapshotBuildDesc &desc,
                                                        std::span<const NetworkTransformSnapshot> transforms,
                                                        MessageWriter &writer,
                                                        NetworkSnapshotBuildResult &out_result) const {
        SnapshotWriter snapshot_writer(writer);
        if (!writer.WriteUInt32(desc.server_tick) ||
            !writer.WriteUInt32(desc.snapshot_sequence) ||
            !writer.WriteUInt32(desc.last_processed_input_sequence) ||
            !snapshot_writer.WriteTransformBatch(transforms)) {
            return false;
        }

        out_result = NetworkSnapshotBuildResult{
            .server_tick = desc.server_tick,
            .snapshot_sequence = desc.snapshot_sequence,
            .entity_count = static_cast<std::uint16_t>(transforms.size()),
            .bytes_estimate = static_cast<std::uint32_t>(writer.Bytes().size()),
        };
        return true;
    }
} // namespace CoreEngine
