#pragma once

#include <string_view>

namespace CoreEngine {
    struct WindowDesc {
        int width = 1280;
        int height = 720;
        std::string title{"CoreEngine"};
        bool resizable = true;
        bool highDpi = true;
        bool decorated = true;
        bool fullscreen = false;
    };
} // namespace CoreEngine
