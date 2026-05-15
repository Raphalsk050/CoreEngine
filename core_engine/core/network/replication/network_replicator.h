#pragma once

#include <cstdint>

#include "core/network/replication/interest_management_system.h"
#include "core/network/replication/network_snapshot_applier.h"
#include "core/network/replication/network_snapshot_builder.h"
#include "core/network/replication/network_spawn_system.h"
#include "core/network/replication/replicated_component_registry.h"

namespace CoreEngine {
    class NetworkSystem;
    struct SimulationFrame;

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
        void Initialize(NetworkSystem &network_system);

        void Shutdown() noexcept;

        void BeginSimulationTick(const SimulationFrame &frame) noexcept;

        void EndSimulationTick(const SimulationFrame &frame) noexcept;

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
        NetworkSystem *network_system_ = nullptr;
        ReplicatedComponentRegistry registry_;
        NetworkSpawnSystem spawn_system_;
        InterestManagementSystem interest_management_;
        NetworkSnapshotBuilder snapshot_builder_;
        NetworkSnapshotApplier snapshot_applier_;
        NetworkReplicatorStats stats_;
    };
} // namespace CoreEngine
