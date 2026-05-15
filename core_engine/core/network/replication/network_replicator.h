#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "core/ecs/node.h"
#include "core/network/replication/interest_management_system.h"
#include "core/network/replication/network_snapshot_applier.h"
#include "core/network/replication/network_snapshot_builder.h"
#include "core/network/replication/network_spawn_system.h"
#include "core/network/replication/replicated_component_registry.h"

namespace CoreEngine {
    class NetworkSystem;
    struct SimulationFrame;
    class World;

    struct NetworkReplicatorStats {
        std::uint64_t snapshots_built = 0;
        std::uint64_t snapshots_applied = 0;
        std::uint64_t entities_replicated = 0;
        std::uint32_t last_snapshot_sequence = 0;
        float snapshot_build_time_ms = 0.0f;
        float snapshot_apply_time_ms = 0.0f;
    };

    /**
     * @brief Coordinates replication registry, spawn lifecycle, and snapshots.
     *
     * Responsibility: keep replication policy and snapshot payload work outside
     * NetworkSystem so transports and gameplay can evolve independently.
     */
    class NetworkReplicator {
    public:
        void Initialize(NetworkSystem &network_system, World &world);

        void Shutdown() noexcept;

        void BeginSimulationTick(const SimulationFrame &frame) noexcept;

        void EndSimulationTick(const SimulationFrame &frame) noexcept;

        [[nodiscard]] NetworkEntityId RegisterEntity(Node node,
                                                     NetworkEntityId network_id,
                                                     PeerId owner_peer,
                                                     bool local_authority);

        [[nodiscard]] NetworkEntityId RegisterEntity(Node node,
                                                     PeerId owner_peer,
                                                     bool local_authority);

        void UnregisterEntity(NetworkEntityId network_id) noexcept;

        [[nodiscard]] Node FindNode(NetworkEntityId network_id) const noexcept;

        [[nodiscard]] bool HasAuthority(Node node) const noexcept;

        [[nodiscard]] bool IsOwningClient(Node node, PeerId peer) const noexcept;

        [[nodiscard]] NetworkEntityId GetNetworkId(Node node) const noexcept;

        [[nodiscard]] ReplicatedComponentRegistry &Registry() noexcept {
            return registry_;
        }

        [[nodiscard]] NetworkSpawnSystem &SpawnSystem() noexcept {
            return spawn_system_;
        }

        [[nodiscard]] InterestManagementSystem &InterestManagement() noexcept {
            return interest_management_;
        }

        [[nodiscard]] NetworkSnapshotBuilder &SnapshotBuilder() noexcept {
            return snapshot_builder_;
        }

        [[nodiscard]] NetworkSnapshotApplier &SnapshotApplier() noexcept {
            return snapshot_applier_;
        }

        [[nodiscard]] const NetworkReplicatorStats &Stats() const noexcept {
            return stats_;
        }

    private:
        void ApplyInboundSnapshots(const SimulationFrame &frame) noexcept;

        void SendOutboundSnapshots(const SimulationFrame &frame) noexcept;

        void CollectTransformSnapshots(std::vector<NetworkTransformSnapshot> &out_snapshots,
                                       std::uint32_t server_tick) const;

        void ApplyTransformSnapshots(std::span<const NetworkTransformSnapshot> snapshots,
                                     const SimulationFrame &frame) noexcept;

        [[nodiscard]] bool ShouldSendSnapshot() noexcept;

        NetworkSystem *network_system_ = nullptr;
        World *world_ = nullptr;
        ReplicatedComponentRegistry registry_;
        NetworkSpawnSystem spawn_system_;
        InterestManagementSystem interest_management_;
        NetworkSnapshotBuilder snapshot_builder_;
        NetworkSnapshotApplier snapshot_applier_;
        NetworkReplicatorStats stats_;
        std::unordered_map<NetworkEntityId, entt::entity> entity_by_network_id_;
        std::vector<NetworkTransformSnapshot> snapshot_scratch_;
        std::vector<NetworkTransformSnapshot> inbound_snapshot_scratch_;
        std::uint32_t snapshot_sequence_ = 0;
        std::uint32_t ticks_until_next_snapshot_ = 0;
        std::uint32_t snapshot_interval_ticks_ = 3;
    };
} // namespace CoreEngine
