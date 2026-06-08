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
        [[nodiscard]] TextureHandle LoadTexture2D(const TextureLoadDesc &desc) override;

        [[nodiscard]] TextureHandle LoadTexture2DAsync(const TextureLoadDesc &desc) override;

        [[nodiscard]] TextureLoadState GetTextureLoadState(TextureHandle handle) const override;

        [[nodiscard]] bool SaveTextureAsDds(TextureHandle handle, std::string_view path) override;

        void DestroyTexture(TextureHandle handle) override;

        [[nodiscard]] TextureHandle CreateTexture(const TextureDesc &desc) override;

        [[nodiscard]] TextureViewHandle CreateTextureView(const TextureViewDesc &desc) override;

        void DestroyTextureView(TextureViewHandle handle) override;

        void BindShaderTexture(std::string_view name, TextureHandle handle) override;

        void RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                const DepthVisualizationDesc &desc) override;

        struct Impl;

        explicit DiligentRenderBackend(DiligentRenderBackendApi api);

        ~DiligentRenderBackend() override;

        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window) override;

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

        void SetRenderTargets(TextureViewHandle color_view, TextureViewHandle depth_view) override;

        [[nodiscard]] FrameBufferColorView GetFrameBufferColorView(FrameBufferHandle handle) const override;

        [[nodiscard]] FrameBufferDepthView GetFrameBufferDepthView(FrameBufferHandle handle) const override;

        void CompositeFrameBuffer(FrameBufferHandle source, const PostProcessDesc &post_process) override;

        [[nodiscard]] MeshHandle UploadMesh(const MeshDesc &desc) override;

        void DestroyMesh(MeshHandle handle) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        [[nodiscard]] ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) override;

        void DestroyShaderProgram(ShaderProgramHandle handle) override;

        void UseShaderProgram(ShaderProgramHandle handle) override;

        void BindShaderTexture(std::string_view name, FrameBufferColorView view) override;

        void BindShaderTexture(std::string_view name, FrameBufferDepthView view) override;

        void BindShaderTexture(std::string_view name, TextureViewHandle view) override;

        void BindShaderUniform(std::string_view name, std::span<const std::uint8_t> data) override;

        void SetPerFrameProps(PerFrameProps props) override;

        void SetPbrGlobalResources(const PbrGlobalResources &resources) override;

        void SubmitBatch(const RenderBatch &batch) override;

        void SubmitGeometryBatch(const GeometryBatch &batch) override;

        void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        std::unique_ptr<Impl> impl_;
    };
} // namespace CoreEngine
