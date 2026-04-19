#include "core/window/window_system.h"

namespace CoreEngine {
    WindowSystem::WindowSystem(std::unique_ptr<IWindowBackend> backend)
        : backend_(std::move(backend)) {
    }

    bool WindowSystem::Initialize(const WindowDesc &desc) const {
        return backend_ != nullptr && backend_->Initialize(desc);
    }

    void WindowSystem::PollEvents() {
        events_.Clear();

        if (backend_ != nullptr) {
            backend_->PollEvents(events_);
        }
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

    std::span<const WindowEvent> WindowSystem::Events() const {
        return events_.Events();
    }

    std::string_view WindowSystem::LastError() const {
        return backend_ != nullptr ? backend_->LastError() : "Window backend is not available";
    }
}
