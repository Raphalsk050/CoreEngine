#pragma once

#include <memory>

#include "core/render/i_render_backend.h"

namespace CoreEngine {
    enum class DiligentRenderBackendApi {
        D3D11,
        D3D12,
        Vulkan,
        Metal,
    };

    class DiligentRenderBackend final : public IRenderBackend {
    public:
        void RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                const DepthVisualizationDesc &desc) override;

        struct Impl;

        explicit DiligentRenderBackend(DiligentRenderBackendApi api);

        ~DiligentRenderBackend() override;

        [[nodiscard]] bool Initialize(const RenderDesc &desc,
                                      NativeWindowHandle native_window) override;

        void BeginFrame() override;

        void Clear(const RenderClearColor &clear_color) override;

        void BeginImGuiFrame() override;

        void RenderImGui() override;

        void EndFrame() override;

        void Resize(int width, int height) override;

        void Shutdown() override;

        [[nodiscard]] FrameBufferHandle CreateFrameBuffer(const FrameBufferDesc &desc) override;

        void DestroyFrameBuffer(FrameBufferHandle handle) override;

        void SetFrameBuffer(FrameBufferHandle handle) override;

        void SetSwapChainFrameBuffer() override;

        [[nodiscard]] FrameBufferColorView GetFrameBufferColorView(FrameBufferHandle handle) const override;

        [[nodiscard]] FrameBufferDepthView GetFrameBufferDepthView(FrameBufferHandle handle) const override;

        void CompositeFrameBuffer(FrameBufferHandle source) override;

        [[nodiscard]] MeshHandle UploadMesh(const MeshDesc &desc) override;

        void DestroyMesh(MeshHandle handle) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        void SetPerFrameProps(PerFrameProps props) override;

        void SubmitBatch(const RenderBatch &batch) override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        std::unique_ptr<Impl> impl_;
    };
} // namespace CoreEngine
