#pragma once

#include "core/render/render_backend_type.h"
#include "core/render/render_clear_color.h"

namespace CoreEngine {
    struct RenderDesc {
        RenderBackendType backend = RenderBackendType::None;
        RenderClearColor clear_color{};
        bool vsync = true;
        bool enable_imgui = true;
        // Explicit swapchain dimensions.
        // Must match the window size at initialization time.
        // Passing 0 lets the backend infer from the HWND (may fail on some drivers).
        int width  = 0;
        int height = 0;
    };
} // namespace CoreEngine
