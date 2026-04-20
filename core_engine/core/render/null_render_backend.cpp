#include "core/render/null_render_backend.h"

namespace CoreEngine {
    bool NullRenderBackend::Initialize(const RenderDesc &, NativeWindowHandle) { return true; }
    void NullRenderBackend::BeginFrame() {}
    void NullRenderBackend::Clear(const RenderClearColor &) {}
    void NullRenderBackend::EndFrame() {}
    void NullRenderBackend::Resize(int, int) {}
    void NullRenderBackend::Shutdown() {}
    MeshHandle NullRenderBackend::UploadMesh(const MeshDesc &) { return {}; }
    void NullRenderBackend::DestroyMesh(MeshHandle) {}
    MaterialHandle NullRenderBackend::ResolveMaterial(const MaterialDesc &) { return {}; }
    void NullRenderBackend::SetCamera(const CameraData &) {}
    void NullRenderBackend::SubmitBatch(const RenderBatch &) {}
    std::string_view NullRenderBackend::LastError() const { return {}; }
} // namespace CoreEngine