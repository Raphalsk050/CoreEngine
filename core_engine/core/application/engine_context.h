#pragma once

#include "core/network/multiplayer_system.h"
#include "core/online/i_online_system.h"
#include "core/simulation/simulation_scheduler.h"

namespace CoreEngine {
    class World;
    class AudioSystem;
    class InputSystem;
    class RenderSystem;
    class WindowSystem;
    class DebugDrawSystem;
    class NetworkPlayerSystem;

    struct EngineContext {
        World &world;
        DebugDrawSystem &debug_draw;
        AudioSystem &audio_system;
        InputSystem &input_system;
        IOnlineSystem &online_system;
        MultiplayerSystem &multiplayer;
        NetworkPlayerSystem &network_players;
        SimulationScheduler &simulation_scheduler;
        WindowSystem &window_system;
        RenderSystem &render_system;
    };
} // namespace CoreEngine
