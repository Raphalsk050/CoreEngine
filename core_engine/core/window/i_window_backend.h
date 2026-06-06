#pragma once

#include <string_view>

#include "core/window/native_window_handle.h"
#include "core/window/window_desc.h"
#include "window_state.h"

namespace CoreEngine {
    class WindowEventQueue;

    class IWindowBackend {
    public:
        virtual ~IWindowBackend() = default;

        [[nodiscard]] virtual bool Initialize(const WindowDesc &desc) = 0;

        virtual void PollEvents(WindowEventQueue &queue) = 0;

        virtual void Shutdown() = 0;

        [[nodiscard]] virtual bool ShouldClose() const = 0;

        [[nodiscard]] virtual NativeWindowHandle GetNativeHandle() const = 0;

        [[nodiscard]] virtual const WindowState &GetState() const = 0;

        [[nodiscard]] virtual std::string_view LastError() const = 0;

        [[nodiscard]] virtual bool SetWindowCursorMode(WindowCursorMode cursor_mode) = 0;
    };
} // namespace CoreEngine
