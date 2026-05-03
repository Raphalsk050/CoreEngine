#pragma once

#include <cstdint>
#include <tsl/robin_map.h>

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

        void RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                const DepthVisualizationDesc &desc) override;

        void CompositeFrameBuffer(FrameBufferHandle source) override;

        [[nodiscard]] MeshHandle UploadMesh(const MeshDesc &desc) override;

        void DestroyMesh(MeshHandle handle) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        [[nodiscard]] ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) override;

        void DestroyShaderProgram(ShaderProgramHandle handle) override;

        void UseShaderProgram(ShaderProgramHandle handle) override;

        void BindShaderTexture(std::string_view name, FrameBufferColorView view) override;

        void BindShaderTexture(std::string_view name, FrameBufferDepthView view) override;

        void BindShaderUniform(std::string_view name, std::span<const std::uint8_t> data) override;

        void SetPerFrameProps(PerFrameProps props) override;

        void SubmitBatch(const RenderBatch &batch) override;

        void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        tsl::robin_map<uint32_t, uint32_t> frame_buffers_;
        tsl::robin_map<uint32_t, uint32_t> shader_programs_;
        uint32_t next_frame_buffer_id_ = 1;
        uint32_t next_frame_buffer_generation_ = 1;
        uint32_t next_shader_program_id_ = 1;
        uint32_t next_shader_program_generation_ = 1;
    };
} // namespace CoreEngine
