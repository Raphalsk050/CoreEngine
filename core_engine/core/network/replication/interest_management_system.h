#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/math/math.h"
#include "core/network/network_peer.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    enum class ReplicationPriority : std::uint8_t {
        Low,
        Medium,
        High,
        Critical,
    };

    struct InterestEntity {
        NetworkEntityId network_id = 0;
        Math::Vec3 position{0.0f, 0.0f, 0.0f};
        ReplicationPriority priority = ReplicationPriority::Medium;
        float relevance_radius = 60.0f;
        bool force_relevant = false;
    };

    struct PeerInterestView {
        PeerId peer = kInvalidPeerId;
        Math::Vec3 observer_position{0.0f, 0.0f, 0.0f};
        std::size_t max_entities = 128;
    };

    /**
     * @brief Selects relevant replicated entities under a per-peer budget.
     *
     * Responsibility: apply distance, forced relevance, and priority filtering
     * before snapshot builders spend bandwidth.
     */
    class InterestManagementSystem {
    public:
        void BuildInterestList(const PeerInterestView &view,
                               std::span<const InterestEntity> entities,
                               std::vector<NetworkEntityId> &out_entities) const;
    };
} // namespace CoreEngine
