#include "core/network/replication/network_snapshot_applier.h"

namespace CoreEngine {
    bool NetworkSnapshotApplier::ReadTransformSnapshot(MessageReader &reader,
                                                       std::vector<NetworkTransformSnapshot> &out_transforms,
                                                       NetworkSnapshotApplyResult &out_result) const {
        SnapshotReader snapshot_reader(reader);
        if (!reader.ReadUInt32(out_result.server_tick) ||
            !reader.ReadUInt32(out_result.snapshot_sequence) ||
            !reader.ReadUInt32(out_result.last_processed_input_sequence) ||
            !snapshot_reader.ReadTransformBatch(out_transforms)) {
            out_transforms.clear();
            return false;
        }

        out_result.entity_count = static_cast<std::uint16_t>(out_transforms.size());
        return true;
    }
} // namespace CoreEngine
