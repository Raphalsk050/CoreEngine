#include "core/render/null_render_backend.h"

namespace CoreEngine {
    bool NullRenderBackend::Initialize(const RenderDesc &, NativeWindowHandle) { return true; }
    void NullRenderBackend::BeginFrame() {}
    void NullRenderBackend::Clear(const RenderClearColor &) {}
    void NullRenderBackend::EndFrame() {}
    void NullRenderBackend::Resize(int, int) {}
    void NullRenderBackend::Shutdown() {}
    std::string_view NullRenderBackend::LastError() const { return {}; }
    MeshHandle NullRenderBackend::GetOrCreatePrimitive(PrimitiveType) { return {}; }
    MeshHandle NullRenderBackend::CreateMesh(std::span<const Vertex>, std::span<const uint16_t>) { return {}; }
    MaterialHandle NullRenderBackend::ResolveMaterial(const MaterialDesc &) { return {}; }
    void NullRenderBackend::SetCamera(const CameraData &) {}
    void NullRenderBackend::SubmitBatch(const RenderBatch &) {}
} // namespace CoreEngine
