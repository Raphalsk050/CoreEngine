#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <span>
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
#include "core/render/debug/render_debug_registry.h"
#include "core/render/i_render_backend.h"
#include "core/render/i_render_context.h"
#include "core/render/material.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitive_topology.h"
#include "core/render/primitive_type.h"
#include "core/render/render_batch.h"
#include "core/render/render_clear_color.h"
#include "core/render/render_graph.h"
#include "core/render/render_mobility.h"

namespace CoreEngine {
    class FrameClock;
}

namespace CoreEngine {
    class World;
    struct CameraComponent;
    struct TransformComponent;
    class DefaultSceneRenderPass;
    class PbrDebugPass;
    class PbrIblPass;
    class PbrShadowPass;

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
        RenderMobility mobility = RenderMobility::Dynamic;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;

        [[nodiscard]] static PrimitiveRendererDesc Unlit(
                PrimitiveType primitive_type, const Math::Vec4 &color = Math::Vec4(1.0f), bool primitive_visible = true,
                bool primitive_cast_shadows = true,
                RenderMobility primitive_mobility = RenderMobility::Dynamic,
                PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList) {
            return PrimitiveRendererDesc{
                    .type = primitive_type,
                    .material = Material::Unlit(UnlitProps{.color = color}),
                    .visible = primitive_visible,
                    .cast_shadows = primitive_cast_shadows,
                    .mobility = primitive_mobility,
                    .topology = primitive_topology,
            };
        }

        [[nodiscard]] static PrimitiveRendererDesc WithMaterial(
                PrimitiveType primitive_type, Material primitive_material, bool primitive_visible = true,
                bool primitive_cast_shadows = true,
                RenderMobility primitive_mobility = RenderMobility::Dynamic,
                PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList) {
            return PrimitiveRendererDesc{
                    .type = primitive_type,
                    .material = std::move(primitive_material),
                    .visible = primitive_visible,
                    .cast_shadows = primitive_cast_shadows,
                    .mobility = primitive_mobility,
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

        void SetDebugView(RenderDebugView view);

        [[nodiscard]] bool SetDebugView(std::string_view name);

        void ClearDebugView();

        [[nodiscard]] std::span<const RenderDebugView> GetAvailableDebugViews() const;

        [[nodiscard]] const RenderDebugStats &GetDebugStats() const;

    private:
        friend class DefaultSceneRenderPass;
        friend class PbrDebugPass;
        friend class PbrIblPass;
        friend class PbrShadowPass;

        struct AsyncModelLoadRequest;
        struct ModelRegistry;
        struct UploadedModelResources;
        struct PbrShadowResources {
            TextureHandle directional_texture{};
            TextureViewHandle directional_srv{};
            std::array<TextureViewHandle, kMaxPbrCascades> directional_dsvs{};
            TextureHandle point_texture{};
            TextureViewHandle point_srv{};
            std::array<TextureViewHandle, kMaxPbrShadowedPointLights * kPbrPointShadowFaceCount> point_dsvs{};
            std::uint32_t cascade_count = 0;
            std::uint32_t max_point_lights = 0;
            std::uint32_t directional_resolution = 0;
            std::uint32_t point_resolution = 0;
        };
        struct PbrPrecomputedIblPaths {
            std::string environment_cube_path{};
            std::string irradiance_cube_path{};
            std::string prefiltered_specular_cube_path{};
            std::string brdf_lut_path{};
            std::string manifest_path{};

            [[nodiscard]] bool IsComplete() const {
                return !environment_cube_path.empty() && !irradiance_cube_path.empty() &&
                       !prefiltered_specular_cube_path.empty() && !brdf_lut_path.empty();
            }
        };
        struct PbrIblResources {
            TextureHandle source_equirectangular_texture{};
            TextureHandle environment_cube_texture{};
            TextureViewHandle environment_cube_srv{};
            std::array<TextureViewHandle, 6> environment_cube_rtvs{};
            TextureHandle irradiance_texture{};
            TextureViewHandle irradiance_srv{};
            std::array<TextureViewHandle, 6> irradiance_rtvs{};
            TextureHandle prefiltered_specular_texture{};
            TextureViewHandle prefiltered_specular_srv{};
            std::vector<TextureViewHandle> prefiltered_specular_rtvs{};
            TextureHandle brdf_lut_texture{};
            TextureViewHandle brdf_lut_srv{};
            TextureViewHandle brdf_lut_rtv{};
            TextureHandle precomputed_environment_cube_texture{};
            TextureViewHandle precomputed_environment_cube_srv{};
            TextureHandle precomputed_irradiance_texture{};
            TextureViewHandle precomputed_irradiance_srv{};
            TextureHandle precomputed_prefiltered_specular_texture{};
            TextureViewHandle precomputed_prefiltered_specular_srv{};
            TextureHandle precomputed_brdf_lut_texture{};
            TextureViewHandle precomputed_brdf_lut_srv{};
            TextureViewHandle active_environment_cube_srv{};
            TextureViewHandle active_irradiance_srv{};
            TextureViewHandle active_prefiltered_specular_srv{};
            TextureViewHandle active_brdf_lut_srv{};
            std::string source_key{};
            std::string source_path{};
            PbrPrecomputedIblPaths pending_precomputed_bake_paths{};
            std::string pending_precomputed_bake_source_key{};
            std::uint32_t irradiance_resolution = 0;
            std::uint32_t environment_resolution = 0;
            std::uint32_t prefiltered_resolution = 0;
            std::uint32_t prefiltered_mip_count = 0;
            std::uint32_t brdf_lut_resolution = 0;
            bool generation_pending = false;
            bool generated = false;
            bool owns_source_texture = false;
            bool using_precomputed_cache = false;
            bool save_generated_precomputed_cache = false;
        };
        enum class ShadowCasterFilter {
            All,
            StaticOnly,
            DynamicOnly,
        };

        void ExecuteDefaultScenePass(RenderPassContext &context);

        void ExecutePbrShadowPass(RenderPassContext &context);

        void ExecutePbrIblPass(RenderPassContext &context);

        void ExecutePbrDebugPass(RenderPassContext &context);

        void DrawPbrSkybox(RenderPassContext &context, const CameraData &camera, float intensity);

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

        [[nodiscard]] bool CreatePbrResources();

        void DestroyPbrResources();

        [[nodiscard]] bool CreatePbrShadowResources();

        void DestroyPbrShadowResources();

        [[nodiscard]] bool CreatePbrIblResources();

        [[nodiscard]] bool EnsurePbrIblGenerationResources();

        [[nodiscard]] bool EnsurePbrIblGenerationPrograms();

        void DestroyPbrRuntimeIblResources();

        void DestroyPbrIblResources();

        void DestroyPbrPrecomputedIblResources();

        void UseRuntimePbrIblResources();

        void SetActivePbrIblResources(TextureViewHandle environment_cube, TextureViewHandle irradiance,
                                      TextureViewHandle prefiltered_specular, TextureViewHandle brdf_lut);

        [[nodiscard]] bool LoadPbrPrecomputedIbl(const PbrPrecomputedIblPaths &paths);

        [[nodiscard]] bool SaveGeneratedPbrIblCache();

        void GatherShadowCasters(World &world, ShadowCasterFilter filter = ShadowCasterFilter::All);

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
        GeometryBatchAccumulator shadow_accumulator_;
        std::unordered_map<entt::entity, Math::Mat4> world_transform_cache_;
        RenderGraph render_graph_;
        RenderPassHandle default_scene_pass_;
        RenderPassHandle pbr_shadow_pass_;
        RenderPassHandle pbr_ibl_pass_;
        RenderPassHandle pbr_debug_pass_;
        RenderFrameResources render_frame_resources_;
        RenderDebugRegistry debug_registry_;
        PbrShadowResources pbr_shadow_resources_{};
        PbrIblResources pbr_ibl_resources_{};
        PbrShadowFrameData pbr_shadow_frame_data_{};
        PbrGlobalResources pbr_global_resources_{};
        ShaderProgramHandle pbr_shadow_depth_program_{};
        ShaderProgramHandle pbr_equirect_to_cube_program_{};
        ShaderProgramHandle pbr_irradiance_program_{};
        ShaderProgramHandle pbr_prefiltered_specular_program_{};
        ShaderProgramHandle pbr_brdf_lut_program_{};
        ShaderProgramHandle pbr_skybox_program_{};
        ShaderProgramHandle pbr_skybox_fallback_program_{};
        ShaderProgramHandle pbr_debug_texture_2d_program_{};
        ShaderProgramHandle pbr_debug_texture_array_program_{};
        ShaderProgramHandle pbr_debug_texture_cube_program_{};
        std::array<MeshHandle, kPrimitiveCount> primitive_cache_{};
        FrameBufferHandle scene_framebuffer_{};
        bool initialized_ = false;
    };
} // namespace CoreEngine
