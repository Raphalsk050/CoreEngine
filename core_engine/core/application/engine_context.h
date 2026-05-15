#pragma once

#include "core/network/replication/network_replicator.h"
#include "core/network/prediction/network_prediction_system.h"
#include "core/online/i_online_system.h"
#include "core/simulation/simulation_scheduler.h"

namespace CoreEngine {
    class World;
    class AudioSystem;
    class InputSystem;
    class NetworkSystem;
    class RenderSystem;
    class WindowSystem;

    struct EngineContext {
        World &world;
        AudioSystem &audio_system;
        InputSystem &input_system;
        IOnlineSystem &online_system;
        NetworkSystem &network_system;
        SimulationScheduler &simulation_scheduler;
        NetworkReplicator &network_replicator;
        NetworkPredictionSystem &prediction_system;
        WindowSystem &window_system;
        RenderSystem &render_system;
    };
} // namespace CoreEngine
