#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assets/i_model_importer.h"
#include "core/assets/model_asset.h"
#include "core/async/future.h"
#include "core/ecs/node.h"
#include "core/render/camera.h"
#include "core/render/camera_data.h"
#include "core/render/i_render_backend.h"
#include "core/render/i_render_context.h"
#include "core/render/material.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitive_topology.h"
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

    struct ModelInstantiationDesc {
        std::string root_name = "Model";
        bool visible = true;
    };

    /**
     * @brief Describes a renderable built from an engine primitive mesh.
     *
     * Responsibility: keep simple primitive creation explicit while avoiding
     * repeated mesh upload, material resolution, and component boilerplate in gameplay code.
     */
    struct PrimitiveRendererDesc {
        PrimitiveType type = PrimitiveType::Cube;
        Material material = Material::Unlit();
        bool visible = true;
        bool cast_shadows = true;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;

        [[nodiscard]] static PrimitiveRendererDesc Unlit(
                PrimitiveType primitive_type, const Math::Vec4 &color = Math::Vec4(1.0f), bool primitive_visible = true,
                bool primitive_cast_shadows = true,
                PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList) {
            return PrimitiveRendererDesc{
                    .type = primitive_type,
                    .material = Material::Unlit(UnlitProps{.color = color}),
                    .visible = primitive_visible,
                    .cast_shadows = primitive_cast_shadows,
                    .topology = primitive_topology,
            };
        }

        [[nodiscard]] static PrimitiveRendererDesc WithMaterial(
                PrimitiveType primitive_type, Material primitive_material, bool primitive_visible = true,
                bool primitive_cast_shadows = true,
                PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList) {
            return PrimitiveRendererDesc{
                    .type = primitive_type,
                    .material = std::move(primitive_material),
                    .visible = primitive_visible,
                    .cast_shadows = primitive_cast_shadows,
                    .topology = primitive_topology,
            };
        }
    };

    struct ModelInstance {
        Node root;
        std::vector<Node> nodes;
        std::vector<Node> mesh_nodes;

        [[nodiscard]] bool IsValid() const { return root.IsValid(); }
    };

    class RenderSystem final : public IRenderContext {
    public:
        explicit RenderSystem(std::unique_ptr<IRenderBackend> backend,
                              std::unique_ptr<IModelImporter> model_importer = nullptr);

        ~RenderSystem() override;

        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window);

        void BeginImGuiFrame() const;

        void RenderFrame(World &world, const FrameClock &frame_clock, float delta_seconds);

        [[nodiscard]] MeshHandle GetOrCreatePrimitive(PrimitiveType type) override;

        [[nodiscard]] MeshHandle CreateMesh(const MeshDesc &desc) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        [[nodiscard]] ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) override;

        [[nodiscard]] TextureHandle LoadTexture2D(const TextureLoadDesc &desc);

        [[nodiscard]] TextureHandle LoadTexture2DAsync(const TextureLoadDesc &desc);

        [[nodiscard]] TextureLoadState GetTextureLoadState(TextureHandle handle) const;

        void DestroyTexture(TextureHandle handle);

        [[nodiscard]] ModelHandle LoadModel(const ModelLoadDesc &desc);

        [[nodiscard]] ModelHandle LoadModelAsync(const ModelLoadDesc &desc);

        // Completes after the model import finishes and all meshes are uploaded to the render backend.
        [[nodiscard]] Future<ModelHandle> LoadModelAsyncFuture(const ModelLoadDesc &desc);

        [[nodiscard]] ModelLoadState GetModelLoadState(ModelHandle handle) const;

        [[nodiscard]] std::size_t GetModelMeshCount(ModelHandle handle) const;

        [[nodiscard]] MeshHandle GetModelMesh(ModelHandle handle, std::size_t mesh_index) const;

        [[nodiscard]] std::size_t GetModelMaterialCount(ModelHandle handle) const;

        [[nodiscard]] MaterialHandle GetModelMaterial(ModelHandle handle, std::size_t material_index) const;

        [[nodiscard]] MaterialHandle GetModelMeshMaterial(ModelHandle handle, std::size_t mesh_index) const;

        [[nodiscard]] ModelInstance InstantiateModel(World &world, ModelHandle handle, Node parent = {},
                                                     const ModelInstantiationDesc &desc = {}) const;

        [[nodiscard]] bool SetPrimitiveRenderer(Node node, const PrimitiveRendererDesc &desc);

        [[nodiscard]] std::string GetModelLoadError(ModelHandle handle) const;

        void DestroyModel(ModelHandle handle);

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

        void SetPostProcess(PostProcessDesc desc);

        [[nodiscard]] const PostProcessDesc &GetPostProcess() const;

        void Resize(int width, int height);

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const;

        [[nodiscard]] std::string_view LastError() const;

        [[nodiscard]] IRenderContext &Context();

        [[nodiscard]] RenderGraph &Graph();

    private:
        friend class DefaultSceneRenderPass;

        struct AsyncModelLoadRequest;
        struct ModelRegistry;
        struct UploadedModelResources;

        void ExecuteDefaultScenePass(RenderPassContext &context);

        void PumpModelUploads();

        void DestroyAllModels();

        void EnsureModelLoadWorker();

        void StopModelLoadWorker();

        void RemoveQueuedModelLoad(ModelHandle handle);

        [[nodiscard]] TextureHandle LoadModelTexture(const ModelTextureAsset &texture);

        [[nodiscard]] MaterialHandle ResolveModelMaterial(const ModelMaterialAsset &material,
                                                          ModelMaterialPipeline pipeline);

        [[nodiscard]] UploadedModelResources BuildModelResources(const ModelAsset &asset,
                                                                 ModelMaterialPipeline material_pipeline);

        [[nodiscard]] bool CreateSceneFrameBuffer();

        void DestroySceneFrameBuffer();

        [[nodiscard]] CameraData ResolveWorldCamera(World &world) const;

        [[nodiscard]] CameraData BuildCameraData(const Math::Vec3 &position, const Math::Quat &rotation,
                                                 const CameraComponent &camera) const;

        static constexpr std::size_t kPrimitiveCount = static_cast<std::size_t>(PrimitiveType::Count);

        [[nodiscard]] AsyncModelLoadRequest StartModelLoadAsync(const ModelLoadDesc &desc);

        std::unique_ptr<IRenderBackend> backend_;
        std::unique_ptr<IModelImporter> model_importer_;
        std::unique_ptr<ModelRegistry> models_;
        RenderDesc desc_{};
        CameraData manual_camera_override_{};
        CameraData default_camera_{};
        bool has_manual_camera_override_ = false;

        int surface_width_ = 1;
        int surface_height_ = 1;

        BatchAccumulator accumulator_;
        std::unordered_map<entt::entity, Math::Mat4> world_transform_cache_;
        RenderGraph render_graph_;
        RenderPassHandle default_scene_pass_;
        RenderFrameResources render_frame_resources_;
        std::array<MeshHandle, kPrimitiveCount> primitive_cache_{};
        FrameBufferHandle scene_framebuffer_{};
        bool initialized_ = false;
    };
} // namespace CoreEngine
