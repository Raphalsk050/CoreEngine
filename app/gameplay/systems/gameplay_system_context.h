#pragma once

#include "core/network/multiplayer_system.h"
#include "core/simulation/simulation_frame.h"

namespace CoreEngine {
    class World;
}

namespace Game {
    struct GameplaySystemContext {
        CoreEngine::World &world;
        CoreEngine::MultiplayerSystem &multiplayer;
        const CoreEngine::SimulationFrame &frame;
    };
} // namespace Game
