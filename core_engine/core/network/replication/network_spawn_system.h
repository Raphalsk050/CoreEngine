#pragma once

#include <cstdint>
#include <vector>

#include "core/network/network_message.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    enum class NetworkDespawnReason : std::uint8_t {
        Unknown,
        Destroyed,
        Extracted,
        MatchEnded,
    };

    struct NetworkSpawnEvent {
        NetworkEntityId network_id = 0;
        std::uint32_t archetype_id = 0;
        PeerId owner_peer = kInvalidPeerId;
        std::uint32_t spawn_tick = 0;
        std::uint32_t component_mask = 0;
    };

    struct NetworkDespawnEvent {
        NetworkEntityId network_id = 0;
        std::uint32_t despawn_tick = 0;
        NetworkDespawnReason reason = NetworkDespawnReason::Unknown;
    };

    /**
     * @brief Allocates stable network entity ids and queues reliable spawn events.
     *
     * Responsibility: separate network identity lifecycle from local ECS node
     * lifetime so spawn/despawn can be reliable protocol events.
     */
    class NetworkSpawnSystem {
    public:
        void Reset() noexcept;

        [[nodiscard]] NetworkEntityId AllocateNetworkEntityId() noexcept;

        void QueueSpawn(const NetworkSpawnEvent &event);

        void QueueDespawn(const NetworkDespawnEvent &event);

        [[nodiscard]] const std::vector<NetworkSpawnEvent> &PendingSpawns() const noexcept {
            return pending_spawns_;
        }

        [[nodiscard]] const std::vector<NetworkDespawnEvent> &PendingDespawns() const noexcept {
            return pending_despawns_;
        }

        void ClearPendingEvents() noexcept;

    private:
        NetworkEntityId next_entity_id_ = 1;
        std::vector<NetworkSpawnEvent> pending_spawns_;
        std::vector<NetworkDespawnEvent> pending_despawns_;
    };
} // namespace CoreEngine
