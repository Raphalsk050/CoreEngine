#include "core/render/render_system.h"

#include <cstddef>
#include <utility>

#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/time/frame_clock.h"
#include "core/ecs/components/camera_component.h"
#include "core/log/logger.h"
#include "core/render/primitives.h"
#include "core/render/render_pass/default_scene_render_pass.h"

namespace CoreEngine {
    //clang-format off
    constexpr RenderPassStage kScenePassStages[] = {
        RenderPassStage::FrameSetup,            // Per-frame setup before any scene rendering.
        RenderPassStage::Shadow,                // Renders shadows maps and other light-space depth resources
        RenderPassStage::DepthPrePass,          // Fills scene depth before color rendering
        RenderPassStage::GBuffer,               // Writes deferred rendering geometry buffers
        RenderPassStage::Lighting,              // Computes lighting from scene/material buffers
        RenderPassStage::ForwardOpaque,         // Renders opaque forward geometry
        RenderPassStage::ForwardTransparent,    // Renders transparent forward geometry after opaque
        RenderPassStage::PostProcess,           // Applies fullscreen effects after scene rendering
        RenderPassStage::Debug,                 // Produces debug overlays or debug textures
    };
    //clang-format on

    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend)
        : backend_(std::move(backend)) {
    }

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_ = desc;
        surface_width_ = desc.width > 0 ? desc.width : 1;
        surface_height_ = desc.height > 0 ? desc.height : 1;

        default_camera_ = Camera{}
                .LookAt({0.f, 0.f, -5.f}, {0.f, 0.f, 0.f})
                .Perspective(60.f,
                             static_cast<float>(surface_width_),
                             static_cast<float>(surface_height_),
                             0.01f,
                             1000.f)
                .GetCameraData();

        initialized_ = backend_ != nullptr && backend_->Initialize(desc, native_window);
        if (initialized_) {
            initialized_ = CreateSceneFrameBuffer();
            default_scene_pass_ = render_graph_.AddPass(std::make_unique<DefaultSceneRenderPass>(*this));
        }

        return initialized_;
    }

    void RenderSystem::BeginImGuiFrame() const {
        if (!initialized_ || backend_ == nullptr || !desc_.enable_imgui) {
            return;
        }

        backend_->BeginImGuiFrame();
    }

    void RenderSystem::RenderFrame(World &world, const FrameClock &frame_clock, float delta_seconds) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }

        render_frame_resources_.Clear();

        backend_->BeginFrame();

        const RenderFrameTiming timing{
            .delta_seconds = delta_seconds,
            .total_seconds = frame_clock.TotalSeconds(),
            .frame_index = frame_clock.FrameIndex(),
        };

        // preserves the time snapshot to pass to all the render passes equally
        RenderPassContext pass_context{
            *backend_, world, frame_clock, timing, render_frame_resources_, surface_width_, surface_height_
        };

        for (const RenderPassStage &stage: kScenePassStages) {
            render_graph_.Execute(stage, pass_context);
        }

        backend_->CompositeFrameBuffer(scene_framebuffer_);

        render_graph_.Execute(RenderPassStage::UI, pass_context);
        backend_->SetSwapChainFrameBuffer();

        if (desc_.enable_imgui) {
            backend_->RenderImGui();
        }

        render_graph_.Execute(RenderPassStage::Present, pass_context);

        backend_->EndFrame();
    }

    MeshHandle RenderSystem::GetOrCreatePrimitive(PrimitiveType type) {
        const auto index = static_cast<std::size_t>(type);
        if (index >= primitive_cache_.size()) {
            return {};
        }

        MeshHandle &cached = primitive_cache_[index];
        if (cached.IsValid()) {
            return cached;
        }

        const MeshDesc desc = Primitives::MeshFor(type);
        if (!desc.IsValid()) {
            return {};
        }

        cached = CreateMesh(desc);
        return cached;
    }

    MeshHandle RenderSystem::CreateMesh(const MeshDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->UploadMesh(desc);
    }

    MaterialHandle RenderSystem::ResolveMaterial(const MaterialDesc &desc) {
        if (!initialized_ || backend_ == nullptr) {
            return {};
        }

        return backend_->ResolveMaterial(desc);
    }

    ShaderProgramHandle RenderSystem::CreateShaderProgram(const ShaderProgramDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->CreateShaderProgram(desc);
    }

    void RenderSystem::DestroyShaderProgram(ShaderProgramHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyShaderProgram(handle);
    }

    void RenderSystem::DestroyMesh(MeshHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyMesh(handle);

        for (MeshHandle &primitive: primitive_cache_) {
            if (primitive == handle) {
                primitive = {};
            }
        }
    }

    FrameBufferHandle RenderSystem::CreateFrameBuffer(const FrameBufferDesc &desc) const {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->CreateFrameBuffer(desc);
    }

    void RenderSystem::DestroyFrameBuffer(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyFrameBuffer(handle);
    }

    void RenderSystem::SetFrameBuffer(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->SetFrameBuffer(handle);
    }

    void RenderSystem::SetSwapChainFrameBuffer() const {
        if (backend_ == nullptr) {
            return;
        }

        backend_->SetSwapChainFrameBuffer();
    }

    void RenderSystem::Clear(const RenderClearColor &clear_color) const {
        if (backend_ == nullptr) {
            return;
        }

        backend_->Clear(clear_color);
    }

    FrameBufferColorView RenderSystem::GetFrameBufferColorView(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return {};
        }

        return backend_->GetFrameBufferColorView(handle);
    }

    FrameBufferDepthView RenderSystem::GetFrameBufferDepthView(FrameBufferHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return {};
        }

        return backend_->GetFrameBufferDepthView(handle);
    }

    RenderPassHandle RenderSystem::AddRenderPass(std::unique_ptr<IRenderPass> pass) {
        return render_graph_.AddPass(std::move(pass));
    }

    void RenderSystem::RemoveRenderPass(RenderPassHandle handle) {
        render_graph_.RemovePass(handle);
    }

    void RenderSystem::SetCamera(const Camera &camera) {
        manual_camera_override_ = camera.GetCameraData();
        has_manual_camera_override_ = true;
    }

    void RenderSystem::SetCamera(const CameraData &camera_data) {
        manual_camera_override_ = camera_data;
        has_manual_camera_override_ = true;
    }

    void RenderSystem::ClearCameraOverride() {
        has_manual_camera_override_ = false;
    }

    void RenderSystem::Resize(int width, int height) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }
        DestroySceneFrameBuffer();
        surface_width_ = width > 0 ? width : 1;
        surface_height_ = height > 0 ? height : 1;
        backend_->Resize(surface_width_, surface_height_);
        initialized_ = CreateSceneFrameBuffer();
    }

    void RenderSystem::Shutdown() {
        default_scene_pass_ = {};
        render_graph_.Clear();

        DestroySceneFrameBuffer();

        if (backend_ != nullptr) {
            backend_->Shutdown();
        }

        primitive_cache_.fill({});
        initialized_ = false;
    }

    bool RenderSystem::IsInitialized() const {
        return initialized_;
    }

    std::string_view RenderSystem::LastError() const {
        return backend_ != nullptr ? backend_->LastError() : "Render backend is not available";
    }

    IRenderContext &RenderSystem::Context() {
        return *this;
    }

    RenderGraph &RenderSystem::Graph() {
        return render_graph_;
    }

    void RenderSystem::ExecuteDefaultScenePass(RenderPassContext &context) {
        World &world = context.GetWorld();
        auto group = world.Registry().group<TransformComponent, MeshRendererComponent>();
        accumulator_.Reserve(group.size());
        accumulator_.Clear();

        for (const auto &[entity, transform, renderer]: group.each()) {
            (void) entity;
            if (!renderer.visible || !renderer.material.IsValid() || !renderer.mesh.IsValid()) {
                continue;
            }

            accumulator_.Add(renderer.material, renderer.mesh, transform.WorldMatrix());
        }

        const CameraData active_camera = has_manual_camera_override_
                                             ? manual_camera_override_
                                             : ResolveWorldCamera(world);

        PerFrameProps props{
            .camera = active_camera,
            .frame_clock = Math::Vec4(
                context.DeltaSeconds(),
                static_cast<float>(context.TotalSeconds()), 0.0f, 0.0f)
        };

        context.SetPerFrameProps(props);
        context.SetFrameBuffer(scene_framebuffer_);
        context.Clear(desc_.clear_color);

        context.SetGlobalColorTexture(GlobalTextureSlot::SceneColor,
                                      context.GetFrameBufferColorView(scene_framebuffer_));
        context.SetGlobalDepthTexture(GlobalTextureSlot::SceneDepth,
                                      context.GetFrameBufferDepthView(scene_framebuffer_));

        for (const RenderBatch &batch: accumulator_.Batches()) {
            context.SubmitBatch(batch);
        }
    }

    bool RenderSystem::CreateSceneFrameBuffer() {
        if (backend_ == nullptr) {
            return false;
        }

        FrameBufferDesc desc;
        desc.width = surface_width_;
        desc.height = surface_height_;
        desc.sample_color = true;
        desc.has_depth = true;
        desc.sample_depth = true;
        desc.depth_format = FrameBufferFormat::Depth32Float;

        scene_framebuffer_ = backend_->CreateFrameBuffer(desc);
        return scene_framebuffer_.IsValid();
    }

    void RenderSystem::DestroySceneFrameBuffer() {
        if (backend_ == nullptr || !scene_framebuffer_.IsValid()) {
            return;
        }

        backend_->DestroyFrameBuffer(scene_framebuffer_);
        scene_framebuffer_ = {};
    }

    CameraData RenderSystem::ResolveWorldCamera(World &world) const {
        const TransformComponent *best_transform = nullptr;
        const CameraComponent *best_camera = nullptr;

        auto group = world.Registry().group<CameraComponent>(entt::get<TransformComponent>);
        for (const entt::entity entity: group) {
            const CameraComponent &camera = group.get<CameraComponent>(entity);
            const TransformComponent &transform = group.get<TransformComponent>(entity);

            if (!camera.enabled) {
                continue;
            }

            if (best_camera == nullptr || camera.priority > best_camera->priority) {
                best_camera = &camera;
                best_transform = &transform;
            }
        }

        if (best_transform == nullptr || best_camera == nullptr) {
            return default_camera_;
        }

        return BuildCameraData(*best_transform, *best_camera);
    }

    CameraData RenderSystem::BuildCameraData(const TransformComponent &transform, const CameraComponent &camera) const {
        const int width = surface_width_ > 0 ? surface_width_ : 1;
        const int height = surface_height_ > 0 ? surface_height_ : 1;

        const float aspect_ratio = camera.aspect_mode == CameraAspectMode::Fixed
                                       ? camera.fixed_aspect_ratio
                                       : static_cast<float>(width) / static_cast<float>(height);

        const Math::Quat &rotation = transform.Rotation();
        const Math::Vec3 &position = transform.Position();
        const Math::Vec3 forward = rotation * Math::Vec3{0.f, 0.f, 1.f};
        const Math::Vec3 up = rotation * Math::Vec3{0.f, 1.f, 0.f};

        CameraData data;
        data.view = Math::LookAtLH(position, position + forward, up);

        if (camera.projection_type == CameraProjectionType::Perspective) {
            data.projection = Math::PerspectiveLH(Math::Deg2Rad(camera.fov_y_degrees), aspect_ratio, camera.near_z,
                                                  camera.far_z);
            return data;
        }

        const float half_height = camera.orthographic_height * 0.5f;
        const float half_width = half_height * aspect_ratio;

        data.projection = Math::OrthoLH(
            -half_width,
            half_width,
            -half_height,
            half_height,
            camera.near_z,
            camera.far_z);

        return data;
    }
} // namespace CoreEngine
