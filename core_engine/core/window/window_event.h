#pragma once

namespace CoreEngine {
    enum class WindowEventType {
        CloseRequested,
        Resized,
        PixelSizeChanged,
        FocusGained,
        FocusLost,
        Minimized,
        Restored
    };

    struct WindowEvent {
        WindowEventType type = WindowEventType::CloseRequested;
        int width = 0;
        int height = 0;
    };
} // namespace CoreEngine
