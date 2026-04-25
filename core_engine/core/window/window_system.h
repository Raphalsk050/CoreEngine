// core/window/window_system.h
#pragma once

#include <memory>

#include "core/window/i_window_backend.h"
#include "core/window/window_event_queue.h"

namespace CoreEngine {
    class WindowSystem final {
    public:
        explicit WindowSystem(std::unique_ptr<IWindowBackend> backend);

        [[nodiscard]] bool Initialize(const WindowDesc &desc) const;

        void BeginFrame();

        void PollEvents();

        [[nodiscard]] bool PushEvent(const WindowEvent &event);

        void Shutdown() const;

        [[nodiscard]] std::span<const WindowEvent> Events() const;

        [[nodiscard]] bool ShouldClose() const;

        [[nodiscard]] NativeWindowHandle GetNativeHandle() const;

        [[nodiscard]] const WindowState &State() const;

        [[nodiscard]] WindowExtent LogicalSize() const;

        [[nodiscard]] WindowExtent PixelSize() const;

        [[nodiscard]] std::string_view LastError() const;

        void SetWindowCursorMode(WindowCursorMode cursor_mode) const;

    private:
        std::unique_ptr<IWindowBackend> backend_;
        WindowEventQueue events_;
    };
} // namespace CoreEngine
