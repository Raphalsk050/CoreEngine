#pragma once

#include <string>

namespace CoreEngine {
    enum class WindowSurfaceType {
        Default,
        Metal
    };

    struct WindowDesc {
        int width = 1280;
        int height = 720;
        std::string title{"CoreEngine"};
        bool resizable = true;
        bool highDpi = true;
        bool decorated = true;
        bool fullscreen = false;
        WindowSurfaceType surface_type = WindowSurfaceType::Default;
    };
} // namespace CoreEngine
