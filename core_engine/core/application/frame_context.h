#pragma once

namespace CoreEngine {
    class World;
    class AudioSystem;
    class InputSystem;
    class RenderSystem;
    class WindowSystem;

    struct FrameContext {
        float delta_time = 0.0f;
        World &world;
        AudioSystem &audio_system;
        const InputSystem &input_system;
        WindowSystem &window_system;
        RenderSystem &render_system;
    };
} // namespace CoreEngine