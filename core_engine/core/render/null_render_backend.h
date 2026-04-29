#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/render/i_render_backend.h"

namespace CoreEngine {
    class NullRenderBackend final : public IRenderBackend {
    public:
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
        std::unordered_map<uint32_t, uint32_t> frame_buffers_;
        uint32_t next_frame_buffer_id_ = 1;
        uint32_t next_frame_buffer_generation_ = 1;
    };
} // namespace CoreEngine
