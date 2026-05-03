#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>

#include "core/render/camera.h"
#include "core/render/camera_data.h"
#include "core/render/i_render_backend.h"
#include "core/render/i_render_context.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitive_type.h"
#include "core/render/render_batch.h"
#include "core/render/render_clear_color.h"
#include "core/render/render_graph.h"

namespace CoreEngine {
    class FrameClock;
}

namespace CoreEngine {
    class World;
    struct CameraComponent;
    struct TransformComponent;
    class DefaultSceneRenderPass;


    class RenderSystem final : public IRenderContext {
    public:
        explicit RenderSystem(std::unique_ptr<IRenderBackend> backend);

        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window);

        void BeginImGuiFrame() const;

        void RenderFrame(World &world, const FrameClock &frame_clock, float delta_seconds);

        [[nodiscard]] MeshHandle GetOrCreatePrimitive(PrimitiveType type) override;

        [[nodiscard]] MeshHandle CreateMesh(const MeshDesc &desc) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        [[nodiscard]] ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) override;

        void DestroyShaderProgram(ShaderProgramHandle handle) override;

        void DestroyMesh(MeshHandle handle);

        [[nodiscard]] FrameBufferHandle CreateFrameBuffer(const FrameBufferDesc &desc) const;

        void DestroyFrameBuffer(FrameBufferHandle handle) const;

        void SetFrameBuffer(FrameBufferHandle handle) const;

        void SetSwapChainFrameBuffer() const;

        void Clear(const RenderClearColor &clear_color) const;

        [[nodiscard]] FrameBufferColorView GetFrameBufferColorView(FrameBufferHandle handle) const;

        [[nodiscard]] FrameBufferDepthView GetFrameBufferDepthView(FrameBufferHandle handle) const;

        [[nodiscard]] RenderPassHandle AddRenderPass(std::unique_ptr<IRenderPass> pass);

        void RemoveRenderPass(RenderPassHandle handle);

        void SetCamera(const Camera &camera);

        void SetCamera(const CameraData &camera_data);

        void ClearCameraOverride();

        void Resize(int width, int height);

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const;

        [[nodiscard]] std::string_view LastError() const;

        [[nodiscard]] IRenderContext &Context();

        [[nodiscard]] RenderGraph &Graph();

    private:
        friend class DefaultSceneRenderPass;

        void ExecuteDefaultScenePass(RenderPassContext &context);

        [[nodiscard]] bool CreateSceneFrameBuffer();

        void DestroySceneFrameBuffer();

        [[nodiscard]] CameraData ResolveWorldCamera(World &world) const;

        [[nodiscard]] CameraData BuildCameraData(const TransformComponent &transform,
                                                 const CameraComponent &camera) const;

        static constexpr std::size_t kPrimitiveCount = static_cast<std::size_t>(PrimitiveType::Count);

        std::unique_ptr<IRenderBackend> backend_;
        RenderDesc desc_{};
        CameraData manual_camera_override_{};
        CameraData default_camera_{};
        bool has_manual_camera_override_ = false;

        int surface_width_ = 1;
        int surface_height_ = 1;

        BatchAccumulator accumulator_;
        RenderGraph render_graph_;
        RenderPassHandle default_scene_pass_;
        RenderFrameResources render_frame_resources_;
        std::array<MeshHandle, kPrimitiveCount> primitive_cache_{};
        FrameBufferHandle scene_framebuffer_{};
        bool initialized_ = false;
    };
} // namespace CoreEngine
