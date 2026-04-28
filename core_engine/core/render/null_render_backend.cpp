#include "core/render/null_render_backend.h"

namespace CoreEngine {
    bool NullRenderBackend::Initialize(const RenderDesc &, NativeWindowHandle) { return true; }

    void NullRenderBackend::BeginFrame() {
    }

    void NullRenderBackend::Clear(const RenderClearColor &) {
    }

    void NullRenderBackend::BeginImGuiFrame() {
    }

    void NullRenderBackend::RenderImGui() {
    }

    void NullRenderBackend::EndFrame() {
    }

    void NullRenderBackend::Resize(int, int) {
    }

    void NullRenderBackend::Shutdown() {
    }

    FrameBufferHandle NullRenderBackend::CreateFrameBuffer(const FrameBufferDesc &) { return {.id = 1, .generation = 1}; }

    void NullRenderBackend::DestroyFrameBuffer(FrameBufferHandle) {
    }

    void NullRenderBackend::SetFrameBuffer(FrameBufferHandle) {
    }

    void NullRenderBackend::SetSwapChainFrameBuffer() {
    }

    void NullRenderBackend::CompositeFrameBuffer(FrameBufferHandle) {
    }

    MeshHandle NullRenderBackend::UploadMesh(const MeshDesc &) { return {}; }

    void NullRenderBackend::DestroyMesh(MeshHandle) {
    }

    MaterialHandle NullRenderBackend::ResolveMaterial(const MaterialDesc &) { return {}; }

    void NullRenderBackend::SetPerFrameProps(PerFrameProps props) {
    }

    void NullRenderBackend::SubmitBatch(const RenderBatch &) {
    }

    std::string_view NullRenderBackend::LastError() const { return {}; }
} // namespace CoreEngine
