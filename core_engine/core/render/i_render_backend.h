#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "core/render/camera_data.h"
#include "core/render/frame_buffer.h"
#include "core/render/material_desc.h"
#include "core/render/mesh_desc.h"
#include "core/render/render_batch.h"
#include "core/render/render_desc.h"
#include "core/window/native_window_handle.h"
#include "debug/depth_visualization.h"

namespace CoreEngine {
    struct PerFrameProps {
        const CameraData &camera;
        Math::Vec4 frame_clock;
    };

    class IRenderBackend {
    public:
        virtual ~IRenderBackend() = default;

        [[nodiscard]] virtual bool Initialize(const RenderDesc &desc,
                                              NativeWindowHandle native_window) = 0;

        virtual void BeginFrame() = 0;

        virtual void Clear(const RenderClearColor &clear_color) = 0;

        virtual void BeginImGuiFrame() = 0;

        virtual void RenderImGui() = 0;

        virtual void EndFrame() = 0;

        virtual void Resize(int width, int height) = 0;

        virtual void Shutdown() = 0;

        [[nodiscard]] virtual FrameBufferHandle CreateFrameBuffer(const FrameBufferDesc &desc) = 0;

        virtual void DestroyFrameBuffer(FrameBufferHandle handle) = 0;

        virtual void SetFrameBuffer(FrameBufferHandle handle) = 0;

        virtual void SetSwapChainFrameBuffer() = 0;

        [[nodiscard]] virtual FrameBufferColorView GetFrameBufferColorView(FrameBufferHandle handle) const = 0;

        [[nodiscard]] virtual FrameBufferDepthView GetFrameBufferDepthView(FrameBufferHandle handle) const = 0;

        virtual void RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                        const DepthVisualizationDesc &desc) = 0;

        virtual void CompositeFrameBuffer(FrameBufferHandle source) = 0;

        [[nodiscard]] virtual MeshHandle UploadMesh(const MeshDesc &desc) = 0;

        virtual void DestroyMesh(MeshHandle handle) = 0;

        [[nodiscard]] virtual MaterialHandle ResolveMaterial(const MaterialDesc &desc) = 0;

        [[nodiscard]] virtual ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) = 0;

        virtual void DestroyShaderProgram(ShaderProgramHandle handle) = 0;

        virtual void UseShaderProgram(ShaderProgramHandle handle) = 0;

        virtual void BindShaderTexture(std::string_view name, FrameBufferColorView view) = 0;

        virtual void BindShaderTexture(std::string_view name, FrameBufferDepthView view) = 0;

        virtual void BindShaderUniform(std::string_view name, std::span<const std::uint8_t> data) = 0;

        virtual void SetPerFrameProps(PerFrameProps props) = 0;

        virtual void SubmitBatch(const RenderBatch &batch) = 0;

        virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) = 0;

        [[nodiscard]] virtual std::string_view LastError() const = 0;
    };
} // namespace CoreEngine
