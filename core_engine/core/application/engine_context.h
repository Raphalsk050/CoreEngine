#pragma once

#include "core/online/i_online_system.h"

namespace CoreEngine {
    class World;
    class AudioSystem;
    class InputSystem;
    class RenderSystem;
    class WindowSystem;

    struct EngineContext {
        World &world;
        AudioSystem &audio_system;
        InputSystem &input_system;
        IOnlineSystem &online_system;
        WindowSystem &window_system;
        RenderSystem &render_system;
    };
} // namespace CoreEngine
