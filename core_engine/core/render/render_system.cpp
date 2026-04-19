#include "core/render/render_system.h"

#include <utility>

namespace CoreEngine {
    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend)
        : backend_(std::move(backend)) {
    }

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_ = desc;
        initialized_ = backend_ != nullptr && backend_->Initialize(desc, native_window);
        return initialized_;
    }

    void RenderSystem::RenderFrame() {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }

        backend_->BeginFrame();
        backend_->Clear(desc_.clear_color);
        backend_->EndFrame();
    }

    void RenderSystem::Resize(int width, int height) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }

        backend_->Resize(width, height);
    }

    void RenderSystem::Shutdown() {
        if (backend_ != nullptr) {
            backend_->Shutdown();
        }

        initialized_ = false;
    }

    bool RenderSystem::IsInitialized() const {
        return initialized_;
    }

    std::string_view RenderSystem::LastError() const {
        return backend_ != nullptr ? backend_->LastError() : "Render backend is not available";
    }
} // namespace CoreEngine