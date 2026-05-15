#include "core/network/replication/network_replicator.h"

#include "core/simulation/simulation_frame.h"

namespace CoreEngine {
    void NetworkReplicator::Initialize(NetworkSystem &network_system) {
        network_system_ = &network_system;
        registry_.RegisterDefaultComponents();
        spawn_system_.Reset();
        stats_ = {};
    }

    void NetworkReplicator::Shutdown() noexcept {
        network_system_ = nullptr;
        spawn_system_.Reset();
        stats_ = {};
    }

    void NetworkReplicator::BeginSimulationTick(const SimulationFrame &frame) noexcept {
        (void) frame;
    }

    void NetworkReplicator::EndSimulationTick(const SimulationFrame &frame) noexcept {
        (void) frame;
        spawn_system_.ClearPendingEvents();
    }
} // namespace CoreEngine
