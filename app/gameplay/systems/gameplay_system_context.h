#pragma once

#include "core/network/network_system.h"
#include "core/network/replication/network_replicator.h"
#include "core/simulation/simulation_frame.h"

namespace CoreEngine {
    class World;
}

namespace Game {
    struct GameplaySystemContext {
        CoreEngine::World &world;
        CoreEngine::NetworkSystem &network_system;
        CoreEngine::NetworkReplicator &network_replicator;
        const CoreEngine::SimulationFrame &frame;
    };
} // namespace Game
