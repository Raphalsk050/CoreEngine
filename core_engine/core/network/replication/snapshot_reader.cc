#include "core/network/replication/snapshot_reader.h"

#include <utility>

namespace CoreEngine {
    SnapshotReader::SnapshotReader(MessageReader &reader) noexcept
        : reader_(reader) {
    }

    bool SnapshotReader::ReadTransform(NetworkTransformSnapshot &snapshot) noexcept {
        return reader_.ReadUInt64(snapshot.network_id) &&
               reader_.ReadUInt32(snapshot.owner_peer) &&
               reader_.ReadUInt32(snapshot.archetype_id) &&
               reader_.ReadUInt32(snapshot.presentation_id) &&
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
               [&]() {
                   std::uint16_t payload_count = 0;
                   if (!reader_.ReadUInt16(payload_count)) {
                       return false;
                   }

                   snapshot.component_payloads.clear();
                   snapshot.component_payloads.reserve(payload_count);
                   for (std::uint16_t i = 0; i < payload_count; ++i) {
                       ReplicatedComponentPayload payload;
                       std::span<const std::byte> bytes;
                       if (!reader_.ReadUInt16(payload.component_type_id) ||
                           !reader_.ReadUInt16(payload.serialization_version) ||
                           !reader_.ReadSizedBytes(bytes)) {
                           snapshot.component_payloads.clear();
                           return false;
                       }

                       payload.bytes.assign(bytes.begin(), bytes.end());
                       snapshot.component_payloads.push_back(std::move(payload));
                   }

                   return true;
               }();
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
