#pragma once

#include <span>

#include "core/math/math.h"
#include "core/network/message_writer.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    struct NetworkTransformSnapshot {
        NetworkEntityId network_id = 0;
        Math::Vec3 position{0.0f, 0.0f, 0.0f};
        Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Math::Vec3 scale{1.0f, 1.0f, 1.0f};
        std::uint32_t tick = 0;
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
