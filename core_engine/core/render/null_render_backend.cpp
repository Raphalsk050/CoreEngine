#include "core/render/null_render_backend.h"

namespace CoreEngine {
    bool NullRenderBackend::Initialize(const RenderDesc &, NativeWindowHandle) { return true; }

    TextureHandle NullRenderBackend::LoadTexture2D(const TextureLoadDesc &desc) {
        if (!desc.IsValid()) {
            return {};
        }

        const TextureHandle handle{
            .id = next_texture_id_++,
            .generation = next_texture_generation_++,
        };

        textures_[handle.id] = handle.generation;
        return handle;
    }

    TextureHandle NullRenderBackend::LoadTexture2DAsync(const TextureLoadDesc &desc) {
        return LoadTexture2D(desc);
    }

    TextureLoadState NullRenderBackend::GetTextureLoadState(TextureHandle handle) const {
        const auto it = textures_.find(handle.id);
        if (it == textures_.end() || it.value() != handle.generation) {
            return TextureLoadState::Invalid;
        }

        return TextureLoadState::Ready;
    }

    void NullRenderBackend::DestroyTexture(TextureHandle handle) {
        const auto it = textures_.find(handle.id);
        if (it != textures_.end() && it.value() == handle.generation) {
            textures_.erase(it);
        }
    }

    void NullRenderBackend::BindShaderTexture(std::string_view, TextureHandle) {
    }

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

    FrameBufferHandle NullRenderBackend::CreateFrameBuffer(const FrameBufferDesc &desc) {
        if (!desc.IsValid()) {
            return {};
        }

        const FrameBufferHandle handle{
            .id = next_frame_buffer_id_++,
            .generation = next_frame_buffer_generation_++,
        };

        frame_buffers_[handle.id] = handle.generation;
        return handle;
    }

    void NullRenderBackend::DestroyFrameBuffer(FrameBufferHandle handle) {
        const auto it = frame_buffers_.find(handle.id);
        if (it != frame_buffers_.end() && it.value() == handle.generation) {
            frame_buffers_.erase(it);
        }
    }

    void NullRenderBackend::SetFrameBuffer(FrameBufferHandle) {
    }

    void NullRenderBackend::SetSwapChainFrameBuffer() {
    }

    FrameBufferColorView NullRenderBackend::GetFrameBufferColorView(FrameBufferHandle) const {
        return {};
    }

    FrameBufferDepthView NullRenderBackend::GetFrameBufferDepthView(FrameBufferHandle) const {
        return {};
    }

    void NullRenderBackend::RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                               const DepthVisualizationDesc &desc) {
    }

    void NullRenderBackend::CompositeFrameBuffer(FrameBufferHandle) {
    }

    MeshHandle NullRenderBackend::UploadMesh(const MeshDesc &) { return {}; }

    void NullRenderBackend::DestroyMesh(MeshHandle) {
    }

    MaterialHandle NullRenderBackend::ResolveMaterial(const MaterialDesc &) { return {}; }

    ShaderProgramHandle NullRenderBackend::CreateShaderProgram(const ShaderProgramDesc &desc) {
        if (!desc.IsValid()) {
            return {};
        }

        const ShaderProgramHandle handle{
            .id = next_shader_program_id_++,
            .generation = next_shader_program_generation_++,
        };
        shader_programs_[handle.id] = handle.generation;
        return handle;
    }

    void NullRenderBackend::DestroyShaderProgram(ShaderProgramHandle handle) {
        const auto it = shader_programs_.find(handle.id);
        if (it != shader_programs_.end() && it.value() == handle.generation) {
            shader_programs_.erase(it);
        }
    }

    void NullRenderBackend::UseShaderProgram(ShaderProgramHandle) {
    }

    void NullRenderBackend::BindShaderTexture(std::string_view, FrameBufferColorView) {
    }

    void NullRenderBackend::BindShaderTexture(std::string_view, FrameBufferDepthView) {
    }

    void NullRenderBackend::BindShaderUniform(std::string_view, std::span<const std::uint8_t>) {
    }

    void NullRenderBackend::SetPerFrameProps(PerFrameProps props) {
    }

    void NullRenderBackend::SubmitBatch(const RenderBatch &) {
    }

    void NullRenderBackend::Draw(std::uint32_t, std::uint32_t) {
    }

    std::string_view NullRenderBackend::LastError() const { return {}; }
} // namespace CoreEngine
