#include "core/network/replication/network_spawn_system.h"

namespace CoreEngine {
    void NetworkSpawnSystem::Reset() noexcept {
        next_entity_id_ = 1;
        pending_spawns_.clear();
        pending_despawns_.clear();
    }

    NetworkEntityId NetworkSpawnSystem::AllocateNetworkEntityId() noexcept {
        return next_entity_id_++;
    }

    void NetworkSpawnSystem::QueueSpawn(const NetworkSpawnEvent &event) {
        pending_spawns_.push_back(event);
    }

    void NetworkSpawnSystem::QueueDespawn(const NetworkDespawnEvent &event) {
        pending_despawns_.push_back(event);
    }

    void NetworkSpawnSystem::ClearPendingEvents() noexcept {
        pending_spawns_.clear();
        pending_despawns_.clear();
    }
} // namespace CoreEngine
