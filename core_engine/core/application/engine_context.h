#pragma once

namespace CoreEngine {
    class World;
    class AudioSystem;
    class WindowSystem;

    struct EngineContext {
        World &world;
        AudioSystem &audio_system;
        WindowSystem &window_system;
    };
} // namespace CoreEngine
