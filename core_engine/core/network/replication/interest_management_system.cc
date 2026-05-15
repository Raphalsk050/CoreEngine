#include "core/network/replication/interest_management_system.h"

#include "core/math/math.h"

namespace CoreEngine {
    void InterestManagementSystem::BuildInterestList(const PeerInterestView &view,
                                                     std::span<const InterestEntity> entities,
                                                     std::vector<NetworkEntityId> &out_entities) const {
        out_entities.clear();
        out_entities.reserve(view.max_entities);

        for (const InterestEntity &entity: entities) {
            if (out_entities.size() >= view.max_entities) {
                break;
            }

            const float radius_squared = entity.relevance_radius * entity.relevance_radius;
            const bool relevant = entity.force_relevant ||
                                  Math::LengthSquared(entity.position - view.observer_position) <= radius_squared;
            if (relevant) {
                out_entities.push_back(entity.network_id);
            }
        }
    }
} // namespace CoreEngine
