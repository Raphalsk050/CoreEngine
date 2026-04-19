#include "core/render/null_render_backend.h"

namespace CoreEngine {
    bool NullRenderBackend::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        (void) desc;
        (void) native_window;
        return true;
    }

    void NullRenderBackend::BeginFrame() {
    }

    void NullRenderBackend::Clear(const RenderClearColor &clear_color) {
        (void) clear_color;
    }

    void NullRenderBackend::EndFrame() {
    }

    void NullRenderBackend::Resize(int width, int height) {
        (void) width;
        (void) height;
    }

    void NullRenderBackend::Shutdown() {
    }

    std::string_view NullRenderBackend::LastError() const {
        return {};
    }
} // namespace CoreEngine
