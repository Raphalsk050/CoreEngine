#include "core/network/replication/snapshot_reader.h"

namespace CoreEngine {
    SnapshotReader::SnapshotReader(MessageReader &reader) noexcept
        : reader_(reader) {
    }

    bool SnapshotReader::ReadTransform(NetworkTransformSnapshot &snapshot) noexcept {
        return reader_.ReadUInt64(snapshot.network_id) &&
               reader_.ReadUInt32(snapshot.tick) &&
               reader_.ReadFloat(snapshot.position.x) &&
               reader_.ReadFloat(snapshot.position.y) &&
               reader_.ReadFloat(snapshot.position.z) &&
               reader_.ReadFloat(snapshot.rotation.w) &&
               reader_.ReadFloat(snapshot.rotation.x) &&
               reader_.ReadFloat(snapshot.rotation.y) &&
               reader_.ReadFloat(snapshot.rotation.z) &&
               reader_.ReadFloat(snapshot.scale.x) &&
               reader_.ReadFloat(snapshot.scale.y) &&
               reader_.ReadFloat(snapshot.scale.z) &&
               reader_.ReadUInt32(snapshot.component_mask) &&
               reader_.ReadUInt32(snapshot.last_processed_input_sequence) &&
               reader_.ReadFloat(snapshot.health) &&
               reader_.ReadFloat(snapshot.max_health) &&
               reader_.ReadUInt64(snapshot.beacon_original_owner) &&
               reader_.ReadUInt64(snapshot.beacon_carrier) &&
               reader_.ReadUInt64(snapshot.capture_captor) &&
               reader_.ReadBool(snapshot.alive) &&
               reader_.ReadBool(snapshot.concussed) &&
               reader_.ReadBool(snapshot.beacon_on_ground) &&
               reader_.ReadBool(snapshot.beacon_extracted) &&
               reader_.ReadBool(snapshot.captured);
    }

    bool SnapshotReader::ReadTransformBatch(std::vector<NetworkTransformSnapshot> &snapshots,
                                            std::uint16_t max_transforms) noexcept {
        std::uint16_t count = 0;
        if (!reader_.ReadUInt16(count) || count > max_transforms) {
            return false;
        }

        snapshots.clear();
        snapshots.reserve(count);

        for (std::uint16_t i = 0; i < count; ++i) {
            NetworkTransformSnapshot snapshot;
            if (!ReadTransform(snapshot)) {
                snapshots.clear();
                return false;
            }
            snapshots.push_back(snapshot);
        }

        return true;
    }
} // namespace CoreEngine
