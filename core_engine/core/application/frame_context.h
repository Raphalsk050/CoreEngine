#pragma once

namespace CoreEngine {
    class World;
    class AudioSystem;
    class WindowSystem;

    struct FrameContext {
        float delta_time = 0.0f;
        World &world;
        AudioSystem &audio_system;
        WindowSystem &window_system;
    };
} // namespace CoreEngine
