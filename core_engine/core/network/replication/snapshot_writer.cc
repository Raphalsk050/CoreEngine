#include "core/network/replication/snapshot_writer.h"

#include "core/network/network_protocol.h"

#include <limits>

namespace CoreEngine {
    SnapshotWriter::SnapshotWriter(MessageWriter &writer) noexcept
        : writer_(writer) {
    }

    bool SnapshotWriter::WriteTransform(const NetworkTransformSnapshot &snapshot) {
        if (snapshot.component_payloads.size() > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }

        return writer_.WriteUInt64(snapshot.network_id) &&
               writer_.WriteUInt32(snapshot.owner_peer) &&
               writer_.WriteUInt32(snapshot.archetype_id) &&
               writer_.WriteUInt32(snapshot.presentation_id) &&
               writer_.WriteUInt32(snapshot.tick) &&
               writer_.WriteFloat(snapshot.position.x) &&
               writer_.WriteFloat(snapshot.position.y) &&
               writer_.WriteFloat(snapshot.position.z) &&
               writer_.WriteFloat(snapshot.rotation.w) &&
               writer_.WriteFloat(snapshot.rotation.x) &&
               writer_.WriteFloat(snapshot.rotation.y) &&
               writer_.WriteFloat(snapshot.rotation.z) &&
               writer_.WriteFloat(snapshot.scale.x) &&
               writer_.WriteFloat(snapshot.scale.y) &&
               writer_.WriteFloat(snapshot.scale.z) &&
               writer_.WriteUInt32(snapshot.component_mask) &&
               writer_.WriteUInt32(snapshot.last_processed_input_sequence) &&
               writer_.WriteUInt16(static_cast<std::uint16_t>(snapshot.component_payloads.size())) &&
               [&]() {
                   for (const ReplicatedComponentPayload &payload: snapshot.component_payloads) {
                       if (!writer_.WriteUInt16(payload.component_type_id) ||
                           !writer_.WriteUInt16(payload.serialization_version) ||
                           !writer_.WriteSizedBytes(payload.bytes)) {
                           return false;
                       }
                   }

                   return true;
               }();
    }

    bool SnapshotWriter::WriteTransformBatch(std::span<const NetworkTransformSnapshot> snapshots) {
        if (snapshots.size() > kMaxSnapshotTransformsPerPacket) {
            return false;
        }

        if (!writer_.WriteUInt16(static_cast<std::uint16_t>(snapshots.size()))) {
            return false;
        }

        for (const NetworkTransformSnapshot &snapshot: snapshots) {
            if (!WriteTransform(snapshot)) {
                return false;
            }
        }

        return true;
    }
} // namespace CoreEngine
