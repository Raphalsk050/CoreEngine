#pragma once

namespace CoreEngine {
    class World;
    class AudioSystem;
    class InputSystem;
    class RenderSystem;
    class WindowSystem;
    class IOnlineSystem;
    class AbilitySystem;

    struct EngineContext {
        World &world;
        AudioSystem &audio_system;
        InputSystem &input_system;
        IOnlineSystem &online_system;
        WindowSystem &window_system;
        RenderSystem &render_system;
        AbilitySystem &ability_system;
    };
} // namespace CoreEngine
