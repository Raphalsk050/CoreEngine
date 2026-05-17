#pragma once

#include <span>

#include "core/math/math.h"
#include "core/network/message_writer.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
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
        float health = 0.0f;
        float max_health = 0.0f;
        std::uint64_t beacon_original_owner = 0;
        std::uint64_t beacon_carrier = 0;
        std::uint64_t capture_captor = 0;
        bool alive = true;
        bool concussed = false;
        bool beacon_on_ground = false;
        bool beacon_extracted = false;
        bool captured = false;
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
