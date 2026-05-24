#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "core/math/math.h"
#include "core/network/message_writer.h"
#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/replicated_state_types.h"

namespace CoreEngine {
    struct ReplicatedComponentPayload {
        ReplicatedComponentTypeId component_type_id = 0;
        std::uint16_t serialization_version = 1;
        std::vector<std::byte> bytes;
    };

    struct NetworkTransformSnapshot {
        NetworkEntityId network_id = 0;
        PeerId owner_peer = kInvalidPeerId;
        NetworkArchetypeId archetype_id = 0;
        NetworkPresentationId presentation_id = 0;
        Math::Vec3 position{0.0f, 0.0f, 0.0f};
        Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Math::Vec3 scale{1.0f, 1.0f, 1.0f};
        std::uint32_t tick = 0;
        std::uint32_t component_mask = 0;
        std::uint32_t last_processed_input_sequence = 0;
        std::vector<ReplicatedComponentPayload> component_payloads;
    };

    /**
     * @brief Serializes transform snapshots into an existing packet writer.
     *
     * Responsibility: batch fixed-layout snapshot data without exposing packet
     * header mechanics to replication systems.
     */
    class SnapshotWriter {
    public:
        explicit SnapshotWriter(MessageWriter &writer) noexcept;

        bool WriteTransform(const NetworkTransformSnapshot &snapshot);

        bool WriteTransformBatch(std::span<const NetworkTransformSnapshot> snapshots);

    private:
        MessageWriter &writer_;
    };
} // namespace CoreEngine
