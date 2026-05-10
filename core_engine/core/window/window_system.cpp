#include "core/window/window_system.h"

namespace CoreEngine {
    WindowSystem::WindowSystem(std::unique_ptr<IWindowBackend> backend)
        : backend_(std::move(backend)) {
    }

    bool WindowSystem::Initialize(const WindowDesc &desc) const {
        return backend_ != nullptr && backend_->Initialize(desc);
    }

    void WindowSystem::BeginFrame() {
        events_.Clear();
    }

    void WindowSystem::PollEvents() {
        BeginFrame();

        if (backend_ != nullptr) {
            backend_->PollEvents(events_);
        }
    }

    bool WindowSystem::PushEvent(const WindowEvent &event) {
        return events_.Push(event);
    }

    void WindowSystem::Shutdown() const {
        if (backend_ != nullptr) {
            backend_->Shutdown();
        }
    }

    bool WindowSystem::ShouldClose() const {
        return backend_ == nullptr || backend_->ShouldClose();
    }

    NativeWindowHandle WindowSystem::GetNativeHandle() const {
        return backend_ != nullptr ? backend_->GetNativeHandle() : NativeWindowHandle{};
    }

    const WindowState &WindowSystem::State() const {
        static constexpr WindowState empty_state{};
        return backend_ != nullptr ? backend_->GetState() : empty_state;
    }

    WindowExtent WindowSystem::LogicalSize() const {
        return State().logical_size;
    }

    WindowExtent WindowSystem::PixelSize() const {
        return State().pixel_size;
    }

    std::span<const WindowEvent> WindowSystem::Events() const {
        return events_.Events();
    }

    std::string_view WindowSystem::LastError() const {
        if (backend_ == nullptr) {
            return "Window backend is not available";
        }

        return backend_->LastError();
    }

    bool WindowSystem::SetWindowCursorMode(WindowCursorMode cursor_mode) const {
        return backend_ != nullptr && backend_->SetWindowCursorMode(cursor_mode);
    }
} // namespace CoreEngine
