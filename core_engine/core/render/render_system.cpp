#include "core/render/render_system.h"

#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <tsl/robin_map.h>

#include "core/ecs/components/hierarchy_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/node.h"
#include "core/ecs/world.h"
#include "core/time/frame_clock.h"
#include "core/ecs/components/camera_component.h"
#include "core/log/logger.h"
#include "core/render/primitives.h"
#include "core/render/render_pass/default_scene_render_pass.h"

namespace CoreEngine {
    struct RenderSystem::AsyncModelLoadRequest {
        ModelHandle handle;
        Future<ModelHandle> future;
    };

    struct RenderSystem::ModelRegistry {
        struct Record {
            Record() = default;
            Record(const Record &) = delete;
            Record &operator=(const Record &) = delete;
            Record(Record &&) noexcept = default;
            Record &operator=(Record &&) noexcept = default;

            uint32_t generation = 0;
            ModelLoadState state = ModelLoadState::Invalid;
            std::vector<MeshHandle> meshes;
            std::vector<std::string> mesh_names;
            std::unique_ptr<ModelLoadResult> decoded_result;
            std::string error_message;
            FuturePromise<ModelHandle> completion;
            std::jthread worker;
        };

        tsl::robin_map<uint32_t, Record> records;
        mutable std::mutex mutex;
        uint32_t next_model_id = 1;
        uint32_t model_generation = 1;
    };

    namespace {
        struct PendingModelUpload {
            ModelHandle handle;
            ModelLoadResult result;
            FuturePromise<ModelHandle> completion;
        };
    }

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

    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend,
                               std::unique_ptr<IModelImporter> model_importer)
        : backend_(std::move(backend)),
          model_importer_(std::move(model_importer)),
          models_(std::make_unique<ModelRegistry>()) {
    }

    RenderSystem::~RenderSystem() {
        DestroyAllModels();
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

        PumpModelUploads();

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

    TextureHandle RenderSystem::LoadTexture2D(const TextureLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }
        return backend_->LoadTexture2D(desc);
    }

    TextureHandle RenderSystem::LoadTexture2DAsync(const TextureLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }
        return backend_->LoadTexture2DAsync(desc);
    }

    TextureLoadState RenderSystem::GetTextureLoadState(TextureHandle handle) const {
        if (!handle.IsValid() || backend_ == nullptr) {
            return TextureLoadState::Invalid;
        }
        return backend_->GetTextureLoadState(handle);
    }

    void RenderSystem::DestroyTexture(TextureHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }
        backend_->DestroyTexture(handle);
    }

    ModelHandle RenderSystem::LoadModel(const ModelLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || model_importer_ == nullptr || models_ == nullptr ||
            !desc.IsValid()) {
            return {};
        }

        ModelLoadResult result;
        try {
            result = model_importer_->Load(desc);
        } catch (const std::exception &ex) {
            result.error_message = ex.what();
        }
        if (!result.IsSuccess()) {
            return {};
        }

        std::string error_message;
        std::vector<MeshHandle> uploaded_meshes;
        uploaded_meshes.reserve(result.asset.meshes.size());

        for (const ModelMeshAsset &mesh: result.asset.meshes) {
            const MeshDesc mesh_desc{
                .vertices = mesh.vertices,
                .indices = mesh.indices,
            };

            MeshHandle mesh_handle = backend_->UploadMesh(mesh_desc);
            if (!mesh_handle.IsValid()) {
                error_message = backend_->LastError().empty()
                                    ? "Failed to upload model mesh"
                                    : std::string{backend_->LastError()};
                break;
            }

            uploaded_meshes.push_back(mesh_handle);
        }

        if (!error_message.empty()) {
            for (MeshHandle mesh: uploaded_meshes) {
                backend_->DestroyMesh(mesh);
            }
            return {};
        }

        ModelRegistry::Record record;
        record.state = ModelLoadState::Ready;
        record.meshes = std::move(uploaded_meshes);
        record.mesh_names.reserve(result.asset.meshes.size());
        for (const ModelMeshAsset &mesh: result.asset.meshes) {
            record.mesh_names.push_back(mesh.name);
        }

        std::lock_guard lock{models_->mutex};
        const uint32_t id = models_->next_model_id++;
        record.generation = models_->model_generation++;
        const uint32_t generation = record.generation;
        models_->records[id] = std::move(record);
        return ModelHandle{.id = id, .generation = generation};
    }

    ModelHandle RenderSystem::LoadModelAsync(const ModelLoadDesc &desc) {
        return StartModelLoadAsync(desc).handle;
    }

    Future<ModelHandle> RenderSystem::LoadModelAsyncFuture(const ModelLoadDesc &desc) {
        return StartModelLoadAsync(desc).future;
    }

    RenderSystem::AsyncModelLoadRequest RenderSystem::StartModelLoadAsync(const ModelLoadDesc &desc) {
        if (!initialized_ || backend_ == nullptr || model_importer_ == nullptr || models_ == nullptr ||
            !desc.IsValid()) {
            return AsyncModelLoadRequest{
                .handle = {},
                .future = Future<ModelHandle>::Failed("Invalid asynchronous model load request"),
            };
        }

        ModelHandle handle;
        Future<ModelHandle> future;
        {
            std::lock_guard lock{models_->mutex};
            handle = ModelHandle{
                .id = models_->next_model_id++,
                .generation = models_->model_generation++,
            };

            ModelRegistry::Record record;
            record.generation = handle.generation;
            record.state = ModelLoadState::Pending;
            future = record.completion.GetFuture();
            models_->records[handle.id] = std::move(record);
        }

        ModelRegistry *registry = models_.get();
        IModelImporter *importer = model_importer_.get();
        std::jthread worker{[registry, importer, handle, desc](std::stop_token stop_token) {
            ModelLoadResult result;
            try {
                result = importer->Load(desc);
            } catch (const std::exception &ex) {
                result.error_message = ex.what();
            }

            if (stop_token.stop_requested()) {
                return;
            }

            std::lock_guard lock{registry->mutex};
            const auto it = registry->records.find(handle.id);
            if (it == registry->records.end() || it.value().generation != handle.generation) {
                return;
            }

            it.value().decoded_result = std::make_unique<ModelLoadResult>(std::move(result));
        }};

        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->records.find(handle.id);
            if (it != models_->records.end() && it.value().generation == handle.generation) {
                it.value().worker = std::move(worker);
            }
        }

        return AsyncModelLoadRequest{
            .handle = handle,
            .future = future,
        };
    }

    ModelLoadState RenderSystem::GetModelLoadState(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return ModelLoadState::Invalid;
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation) {
            return ModelLoadState::Invalid;
        }

        return it.value().state;
    }

    std::size_t RenderSystem::GetModelMeshCount(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return 0;
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready) {
            return 0;
        }

        return it.value().meshes.size();
    }

    MeshHandle RenderSystem::GetModelMesh(ModelHandle handle, std::size_t mesh_index) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready || mesh_index >= it.value().meshes.size()) {
            return {};
        }

        return it.value().meshes[mesh_index];
    }

    std::string RenderSystem::GetModelLoadError(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation) {
            return {};
        }

        return it.value().error_message;
    }

    void RenderSystem::DestroyModel(ModelHandle handle) {
        if (!handle.IsValid() || models_ == nullptr) {
            return;
        }

        ModelRegistry::Record record;
        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->records.find(handle.id);
            if (it == models_->records.end() || it.value().generation != handle.generation) {
                return;
            }

            record = std::move(it.value());
            models_->records.erase(it);
        }

        if (record.state == ModelLoadState::Pending) {
            record.completion.Cancel("Model load was cancelled");
        }

        if (backend_ != nullptr) {
            for (MeshHandle mesh: record.meshes) {
                backend_->DestroyMesh(mesh);
            }
        }
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

        DestroyAllModels();
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
        if (backend_ == nullptr) {
            return "Render backend is not available";
        }

        return backend_->LastError();
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
            if (!renderer.visible || !renderer.material.IsValid() || !renderer.mesh.IsValid()) {
                continue;
            }

            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            const Math::Mat4 world_matrix = hierarchy == nullptr || hierarchy->parent == entt::null
                                                ? transform.WorldMatrix()
                                                : Node{entity, &world}.GetWorldMatrix();
            accumulator_.Add(renderer.material, renderer.mesh, world_matrix);
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

    void RenderSystem::PumpModelUploads() {
        if (backend_ == nullptr || models_ == nullptr) {
            return;
        }

        std::vector<PendingModelUpload> pending_uploads;
        {
            std::lock_guard lock{models_->mutex};
            for (auto it = models_->records.begin(); it != models_->records.end(); ++it) {
                ModelRegistry::Record &record = it.value();
                if (record.state != ModelLoadState::Pending || record.decoded_result == nullptr) {
                    continue;
                }

                pending_uploads.push_back(PendingModelUpload{
                    .handle = ModelHandle{.id = it.key(), .generation = record.generation},
                    .result = std::move(*record.decoded_result),
                    .completion = record.completion,
                });
                record.decoded_result.reset();
            }
        }

        for (PendingModelUpload &upload: pending_uploads) {
            if (!upload.result.IsSuccess()) {
                std::string error_message = upload.result.error_message.empty()
                                                ? "Failed to import model"
                                                : std::move(upload.result.error_message);
                bool reject_future = false;
                {
                    std::lock_guard lock{models_->mutex};
                    const auto it = models_->records.find(upload.handle.id);
                    if (it != models_->records.end() && it.value().generation == upload.handle.generation) {
                        it.value().state = ModelLoadState::Failed;
                        it.value().error_message = error_message;
                        reject_future = true;
                    }
                }

                if (reject_future) {
                    upload.completion.Reject(std::move(error_message));
                }
                continue;
            }

            std::vector<MeshHandle> uploaded_meshes;
            std::vector<std::string> mesh_names;
            std::string error_message;

            uploaded_meshes.reserve(upload.result.asset.meshes.size());
            mesh_names.reserve(upload.result.asset.meshes.size());

            for (const ModelMeshAsset &mesh: upload.result.asset.meshes) {
                const MeshDesc mesh_desc{
                    .vertices = mesh.vertices,
                    .indices = mesh.indices,
                };

                MeshHandle mesh_handle = backend_->UploadMesh(mesh_desc);
                if (!mesh_handle.IsValid()) {
                    error_message = backend_->LastError().empty()
                                        ? "Failed to upload model mesh"
                                        : std::string{backend_->LastError()};
                    break;
                }

                uploaded_meshes.push_back(mesh_handle);
                mesh_names.push_back(mesh.name);
            }

            bool keep_uploaded_meshes = false;
            bool resolve_future = false;
            bool reject_future = false;
            {
                std::lock_guard lock{models_->mutex};
                const auto it = models_->records.find(upload.handle.id);
                if (it != models_->records.end() && it.value().generation == upload.handle.generation) {
                    if (error_message.empty()) {
                        it.value().meshes = std::move(uploaded_meshes);
                        it.value().mesh_names = std::move(mesh_names);
                        it.value().state = ModelLoadState::Ready;
                        keep_uploaded_meshes = true;
                        resolve_future = true;
                    } else {
                        it.value().state = ModelLoadState::Failed;
                        it.value().error_message = error_message;
                        reject_future = true;
                    }
                }
            }

            if (!keep_uploaded_meshes) {
                for (MeshHandle mesh: uploaded_meshes) {
                    backend_->DestroyMesh(mesh);
                }
            }

            if (resolve_future) {
                upload.completion.Resolve(upload.handle);
            } else if (reject_future) {
                upload.completion.Reject(std::move(error_message));
            }
        }
    }

    void RenderSystem::DestroyAllModels() {
        if (models_ == nullptr) {
            return;
        }

        std::vector<ModelRegistry::Record> records;
        {
            std::lock_guard lock{models_->mutex};
            records.reserve(models_->records.size());
            for (auto it = models_->records.begin(); it != models_->records.end(); ++it) {
                records.push_back(std::move(it.value()));
            }
            models_->records.clear();
        }

        for (const ModelRegistry::Record &record: records) {
            if (record.state == ModelLoadState::Pending) {
                record.completion.Cancel("Model load was cancelled");
            }
        }

        if (backend_ != nullptr) {
            for (const ModelRegistry::Record &record: records) {
                for (MeshHandle mesh: record.meshes) {
                    backend_->DestroyMesh(mesh);
                }
            }
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
        entt::entity best_entity = entt::null;
        const CameraComponent *best_camera = nullptr;

        auto group = world.Registry().group<CameraComponent>(entt::get<TransformComponent>);
        for (const entt::entity entity: group) {
            const CameraComponent &camera = group.get<CameraComponent>(entity);

            if (!camera.enabled) {
                continue;
            }

            if (best_camera == nullptr || camera.priority > best_camera->priority) {
                best_camera = &camera;
                best_entity = entity;
            }
        }

        if (best_entity == entt::null || best_camera == nullptr) {
            return default_camera_;
        }

        const Node camera_node{best_entity, &world};
        return BuildCameraData(camera_node.GetWorldPosition(), camera_node.GetWorldRotation(), *best_camera);
    }

    CameraData RenderSystem::BuildCameraData(const Math::Vec3 &position,
                                             const Math::Quat &rotation,
                                             const CameraComponent &camera) const {
        const int width = surface_width_ > 0 ? surface_width_ : 1;
        const int height = surface_height_ > 0 ? surface_height_ : 1;

        const float aspect_ratio = camera.aspect_mode == CameraAspectMode::Fixed
                                       ? camera.fixed_aspect_ratio
                                       : static_cast<float>(width) / static_cast<float>(height);

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
