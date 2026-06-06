#include "core/render/render_system.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <tsl/robin_map.h>

#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/hierarchy_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/node.h"
#include "core/ecs/world.h"
#include "core/log/logger.h"
#include "core/render/material.h"
#include "core/render/primitives.h"
#include "core/render/render_pass/default_scene_render_pass.h"
#include "core/time/frame_clock.h"

namespace CoreEngine {
    struct RenderSystem::AsyncModelLoadRequest {
        ModelHandle handle;
        Future<ModelHandle> future;
    };

    struct RenderSystem::UploadedModelResources {
        std::vector<MeshHandle> meshes;
        std::vector<std::string> mesh_names;
        std::vector<MaterialHandle> materials;
        std::vector<std::uint32_t> mesh_material_indices;
        std::vector<ModelNodeAsset> nodes;
        std::string error_message;

        [[nodiscard]] bool IsSuccess() const { return error_message.empty() && !meshes.empty() && !materials.empty(); }
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
            std::vector<MaterialHandle> materials;
            std::vector<std::uint32_t> mesh_material_indices;
            std::vector<ModelNodeAsset> nodes;
            std::unique_ptr<ModelLoadResult> decoded_result;
            std::string error_message;
            FuturePromise<ModelHandle> completion;
        };

        struct LoadTask {
            ModelHandle handle;
            ModelLoadDesc desc;
        };

        tsl::robin_map<uint32_t, Record> records;
        std::unordered_map<std::string, TextureHandle> texture_cache;
        mutable std::mutex mutex;
        std::mutex load_queue_mutex;
        std::condition_variable_any load_event;
        std::deque<LoadTask> load_queue;
        std::jthread load_worker;
        bool load_worker_started = false;
        uint32_t next_model_id = 1;
        uint32_t model_generation = 1;
    };

    namespace {
        struct PendingModelUpload {
            ModelHandle handle;
            ModelLoadResult result;
            FuturePromise<ModelHandle> completion;
        };

        [[nodiscard]] const ModelTextureAsset *FindModelTexture(const ModelMaterialAsset &material,
                                                                ModelTextureSemantic semantic) {
            for (const ModelTextureAsset &texture: material.textures) {
                if (texture.semantic == semantic && texture.IsValid()) {
                    return &texture;
                }
            }

            return nullptr;
        }

        [[nodiscard]] std::uint32_t NormalizeModelMaterialIndex(std::uint32_t material_index,
                                                                std::size_t material_count) {
            if (material_count == 0u) {
                return 0u;
            }

            return material_index < material_count ? material_index : 0u;
        }

        [[nodiscard]] std::string ModelTextureCacheKey(const ModelTextureAsset &texture) {
            return texture.path + (texture.srgb ? "|srgb" : "|linear");
        }

        void DestroyUploadedMeshes(IRenderBackend &backend, std::vector<MeshHandle> &meshes) {
            for (MeshHandle mesh: meshes) {
                backend.DestroyMesh(mesh);
            }
            meshes.clear();
        }

        [[nodiscard]] std::string MakeModelNodeName(const ModelNodeAsset &node, std::size_t index) {
            if (!node.name.empty()) {
                return node.name;
            }

            return "ModelNode_" + std::to_string(index);
        }

        [[nodiscard]] std::string MakeModelMeshNodeName(const std::string &mesh_name, std::size_t mesh_index) {
            if (!mesh_name.empty()) {
                return mesh_name;
            }

            return "ModelMesh_" + std::to_string(mesh_index);
        }

        [[nodiscard]] Math::Mat4 ResolveCachedWorldMatrix(World &world, entt::entity entity,
                                                          std::unordered_map<entt::entity, Math::Mat4> &cache,
                                                          std::uint32_t depth = 0) {
            constexpr std::uint32_t kMaxHierarchyDepth = 1024u;
            if (entity == entt::null || !world.Registry().valid(entity) || depth >= kMaxHierarchyDepth) {
                return Math::Identity();
            }

            if (const auto it = cache.find(entity); it != cache.end()) {
                return it->second;
            }

            const TransformComponent *transform = world.TryGetComponent<TransformComponent>(entity);
            if (transform == nullptr) {
                return Math::Identity();
            }

            Math::Mat4 world_matrix = transform->WorldMatrix();
            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            if (hierarchy != nullptr && hierarchy->parent != entt::null && hierarchy->parent != entity &&
                world.Registry().valid(hierarchy->parent)) {
                world_matrix = ResolveCachedWorldMatrix(world, hierarchy->parent, cache, depth + 1u) * world_matrix;
            }

            cache.emplace(entity, world_matrix);
            return world_matrix;
        }
    } // namespace

    //clang-format off
    constexpr RenderPassStage kScenePassStages[] = {
            RenderPassStage::FrameSetup,         // Per-frame setup before any scene rendering.
            RenderPassStage::Shadow,             // Renders shadows maps and other light-space depth resources
            RenderPassStage::DepthPrePass,       // Fills scene depth before color rendering
            RenderPassStage::GBuffer,            // Writes deferred rendering geometry buffers
            RenderPassStage::Lighting,           // Computes lighting from scene/material buffers
            RenderPassStage::ForwardOpaque,      // Renders opaque forward geometry
            RenderPassStage::ForwardTransparent, // Renders transparent forward geometry after opaque
            RenderPassStage::PostProcess,        // Applies fullscreen effects after scene rendering
            RenderPassStage::Debug,              // Produces debug overlays or debug textures
    };
    //clang-format on

    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend,
                               std::unique_ptr<IModelImporter> model_importer) :
        backend_(std::move(backend)), model_importer_(std::move(model_importer)),
        models_(std::make_unique<ModelRegistry>()) {}

    RenderSystem::~RenderSystem() {
        if (initialized_ && backend_ != nullptr) {
            render_graph_.Clear(backend_.get());
        }
        DestroyAllModels();
    }

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_ = desc;
        surface_width_ = desc.width > 0 ? desc.width : 1;
        surface_height_ = desc.height > 0 ? desc.height : 1;

        default_camera_ = Camera{}.LookAt({0.f, 0.f, -5.f}, {0.f, 0.f, 0.f})
                                  .Perspective(60.f, static_cast<float>(surface_width_),
                                               static_cast<float>(surface_height_), 0.01f, 1000.f)
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
        RenderPassContext pass_context{*backend_,      world,          frame_clock, timing, render_frame_resources_,
                                       surface_width_, surface_height_};

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

        UploadedModelResources resources = BuildModelResources(result.asset);
        if (!resources.IsSuccess()) {
            return {};
        }

        ModelRegistry::Record record;
        record.state = ModelLoadState::Ready;
        record.meshes = std::move(resources.meshes);
        record.mesh_names = std::move(resources.mesh_names);
        record.materials = std::move(resources.materials);
        record.mesh_material_indices = std::move(resources.mesh_material_indices);
        record.nodes = std::move(resources.nodes);

        std::lock_guard lock{models_->mutex};
        const uint32_t id = models_->next_model_id++;
        record.generation = models_->model_generation++;
        const uint32_t generation = record.generation;
        models_->records[id] = std::move(record);
        return ModelHandle{.id = id, .generation = generation};
    }

    ModelHandle RenderSystem::LoadModelAsync(const ModelLoadDesc &desc) { return StartModelLoadAsync(desc).handle; }

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

        EnsureModelLoadWorker();
        {
            std::lock_guard lock{models_->load_queue_mutex};
            models_->load_queue.push_back(ModelRegistry::LoadTask{
                    .handle = handle,
                    .desc = desc,
            });
        }
        models_->load_event.notify_one();

        return AsyncModelLoadRequest{
                .handle = handle,
                .future = future,
        };
    }

    void RenderSystem::EnsureModelLoadWorker() {
        if (models_ == nullptr || model_importer_ == nullptr) {
            return;
        }

        std::lock_guard lock{models_->load_queue_mutex};
        if (models_->load_worker_started) {
            return;
        }

        ModelRegistry *registry = models_.get();
        IModelImporter *importer = model_importer_.get();
        models_->load_worker_started = true;
        models_->load_worker = std::jthread{[registry, importer](std::stop_token stop_token) {
            while (!stop_token.stop_requested()) {
                ModelRegistry::LoadTask task;
                {
                    std::unique_lock queue_lock{registry->load_queue_mutex};
                    const bool has_task = registry->load_event.wait(
                            queue_lock, stop_token, [registry] { return !registry->load_queue.empty(); });

                    if (!has_task || stop_token.stop_requested()) {
                        return;
                    }

                    task = std::move(registry->load_queue.front());
                    registry->load_queue.pop_front();
                }

                ModelLoadResult result;
                try {
                    result = importer->Load(task.desc);
                } catch (const std::exception &ex) {
                    result.error_message = ex.what();
                }

                if (stop_token.stop_requested()) {
                    return;
                }

                std::lock_guard record_lock{registry->mutex};
                const auto it = registry->records.find(task.handle.id);
                if (it == registry->records.end() || it.value().generation != task.handle.generation) {
                    continue;
                }

                it.value().decoded_result = std::make_unique<ModelLoadResult>(std::move(result));
            }
        }};
    }

    void RenderSystem::StopModelLoadWorker() {
        if (models_ == nullptr || !models_->load_worker_started) {
            return;
        }

        std::jthread worker;
        {
            std::lock_guard lock{models_->load_queue_mutex};
            models_->load_worker.request_stop();
            worker = std::move(models_->load_worker);
            models_->load_queue.clear();
            models_->load_worker_started = false;
        }
        models_->load_event.notify_all();
    }

    void RenderSystem::RemoveQueuedModelLoad(ModelHandle handle) {
        if (models_ == nullptr || !handle.IsValid()) {
            return;
        }

        std::lock_guard lock{models_->load_queue_mutex};
        for (auto it = models_->load_queue.begin(); it != models_->load_queue.end();) {
            if (it->handle == handle) {
                it = models_->load_queue.erase(it);
                continue;
            }

            ++it;
        }
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

    std::size_t RenderSystem::GetModelMaterialCount(ModelHandle handle) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return 0;
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready) {
            return 0;
        }

        return it.value().materials.size();
    }

    MaterialHandle RenderSystem::GetModelMaterial(ModelHandle handle, std::size_t material_index) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready || material_index >= it.value().materials.size()) {
            return {};
        }

        return it.value().materials[material_index];
    }

    MaterialHandle RenderSystem::GetModelMeshMaterial(ModelHandle handle, std::size_t mesh_index) const {
        if (!handle.IsValid() || models_ == nullptr) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto it = models_->records.find(handle.id);
        if (it == models_->records.end() || it.value().generation != handle.generation ||
            it.value().state != ModelLoadState::Ready || mesh_index >= it.value().mesh_material_indices.size()) {
            return {};
        }

        const std::uint32_t material_index = it.value().mesh_material_indices[mesh_index];
        if (material_index >= it.value().materials.size()) {
            return {};
        }

        return it.value().materials[material_index];
    }

    ModelInstance RenderSystem::InstantiateModel(World &world, ModelHandle handle, Node parent,
                                                 const ModelInstantiationDesc &desc) const {
        ModelInstance instance;
        if (!handle.IsValid() || models_ == nullptr || (parent.IsValid() && parent.OwnerWorld() != &world)) {
            return instance;
        }

        std::vector<MeshHandle> meshes;
        std::vector<std::string> mesh_names;
        std::vector<MaterialHandle> materials;
        std::vector<std::uint32_t> mesh_material_indices;
        std::vector<ModelNodeAsset> nodes;
        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->records.find(handle.id);
            if (it == models_->records.end() || it.value().generation != handle.generation ||
                it.value().state != ModelLoadState::Ready) {
                return instance;
            }

            meshes = it.value().meshes;
            mesh_names = it.value().mesh_names;
            materials = it.value().materials;
            mesh_material_indices = it.value().mesh_material_indices;
            nodes = it.value().nodes;
        }

        const std::string root_name = desc.root_name.empty() ? "Model" : desc.root_name;
        instance.root = world.CreateNode(root_name);
        if (parent.IsValid() && !instance.root.SetParent(parent)) {
            instance.root.Destroy();
            instance = {};
            return instance;
        }

        std::vector<Node> created_nodes;
        created_nodes.resize(nodes.size());
        instance.nodes.reserve(nodes.size());
        instance.mesh_nodes.reserve(meshes.size());

        for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index) {
            const ModelNodeAsset &node_asset = nodes[node_index];
            Node node = world.CreateNode(MakeModelNodeName(node_asset, node_index));
            node.SetLocalMatrix(node_asset.local_transform);

            const bool has_valid_parent =
                    node_asset.parent_index < created_nodes.size() && created_nodes[node_asset.parent_index].IsValid();
            node.SetParent(has_valid_parent ? created_nodes[node_asset.parent_index] : instance.root);

            created_nodes[node_index] = node;
            instance.nodes.push_back(node);
        }

        if (created_nodes.empty()) {
            created_nodes.push_back(instance.root);
        }

        for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index) {
            const Node parent_node = created_nodes[node_index].IsValid() ? created_nodes[node_index] : instance.root;
            for (const std::uint32_t mesh_index: nodes[node_index].mesh_indices) {
                if (mesh_index >= meshes.size() || mesh_index >= mesh_material_indices.size()) {
                    continue;
                }

                const std::uint32_t material_index = mesh_material_indices[mesh_index];
                if (material_index >= materials.size()) {
                    continue;
                }

                Node mesh_node = world.CreateNode(MakeModelMeshNodeName(mesh_names[mesh_index], mesh_index));
                mesh_node.SetParent(parent_node);
                mesh_node.AddComponent<MeshRendererComponent>(MeshRendererComponent{
                        .mesh = meshes[mesh_index],
                        .material = materials[material_index],
                        .visible = desc.visible,
                });
                instance.mesh_nodes.push_back(mesh_node);
            }
        }

        if (nodes.empty()) {
            for (std::size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
                if (mesh_index >= mesh_material_indices.size()) {
                    continue;
                }

                const std::uint32_t material_index = mesh_material_indices[mesh_index];
                if (material_index >= materials.size()) {
                    continue;
                }

                Node mesh_node = world.CreateNode(MakeModelMeshNodeName(mesh_names[mesh_index], mesh_index));
                mesh_node.SetParent(instance.root);
                mesh_node.AddComponent<MeshRendererComponent>(MeshRendererComponent{
                        .mesh = meshes[mesh_index],
                        .material = materials[material_index],
                        .visible = desc.visible,
                });
                instance.mesh_nodes.push_back(mesh_node);
            }
        }

        return instance;
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
            RemoveQueuedModelLoad(handle);
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

    void RenderSystem::RemoveRenderPass(RenderPassHandle handle) { render_graph_.RemovePass(handle, backend_.get()); }

    void RenderSystem::SetCamera(const Camera &camera) {
        manual_camera_override_ = camera.GetCameraData();
        has_manual_camera_override_ = true;
    }

    void RenderSystem::SetCamera(const CameraData &camera_data) {
        manual_camera_override_ = camera_data;
        has_manual_camera_override_ = true;
    }

    void RenderSystem::ClearCameraOverride() { has_manual_camera_override_ = false; }

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
        render_graph_.Clear(backend_.get());

        DestroyAllModels();
        DestroySceneFrameBuffer();

        if (backend_ != nullptr) {
            backend_->Shutdown();
        }

        primitive_cache_.fill({});
        initialized_ = false;
    }

    bool RenderSystem::IsInitialized() const { return initialized_; }

    std::string_view RenderSystem::LastError() const {
        if (backend_ == nullptr) {
            return "Render backend is not available";
        }

        return backend_->LastError();
    }

    IRenderContext &RenderSystem::Context() { return *this; }

    RenderGraph &RenderSystem::Graph() { return render_graph_; }

    void RenderSystem::ExecuteDefaultScenePass(RenderPassContext &context) {
        World &world = context.GetWorld();
        auto group = world.Registry().group<TransformComponent, MeshRendererComponent>();
        accumulator_.Reserve(group.size());
        accumulator_.Clear();
        world_transform_cache_.clear();
        world_transform_cache_.reserve(group.size());

        for (auto [entity, transform, renderer]: group.each()) {
            if (!renderer.visible || !renderer.material.IsValid() || !renderer.mesh.IsValid()) {
                continue;
            }

            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            const Math::Mat4 world_matrix = hierarchy == nullptr || hierarchy->parent == entt::null
                                                    ? transform.WorldMatrix()
                                                    : ResolveCachedWorldMatrix(world, entity, world_transform_cache_);
            accumulator_.Add(renderer.material, renderer.mesh, world_matrix);
        }

        const CameraData active_camera =
                has_manual_camera_override_ ? manual_camera_override_ : ResolveWorldCamera(world);

        PerFrameProps props{.camera = active_camera,
                            .frame_clock = Math::Vec4(context.DeltaSeconds(),
                                                      static_cast<float>(context.TotalSeconds()), 0.0f, 0.0f)};

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

            UploadedModelResources resources = BuildModelResources(upload.result.asset);
            std::string error_message = resources.error_message;

            bool keep_uploaded_meshes = false;
            bool resolve_future = false;
            bool reject_future = false;
            {
                std::lock_guard lock{models_->mutex};
                const auto it = models_->records.find(upload.handle.id);
                if (it != models_->records.end() && it.value().generation == upload.handle.generation) {
                    if (error_message.empty()) {
                        it.value().meshes = std::move(resources.meshes);
                        it.value().mesh_names = std::move(resources.mesh_names);
                        it.value().materials = std::move(resources.materials);
                        it.value().mesh_material_indices = std::move(resources.mesh_material_indices);
                        it.value().nodes = std::move(resources.nodes);
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
                DestroyUploadedMeshes(*backend_, resources.meshes);
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

        StopModelLoadWorker();

        std::vector<ModelRegistry::Record> records;
        {
            std::lock_guard lock{models_->mutex};
            records.reserve(models_->records.size());
            for (auto it = models_->records.begin(); it != models_->records.end(); ++it) {
                records.push_back(std::move(it.value()));
            }
            models_->records.clear();
            models_->texture_cache.clear();
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

    TextureHandle RenderSystem::LoadModelTexture(const ModelTextureAsset &texture) {
        if (!texture.IsValid() || models_ == nullptr) {
            return {};
        }

        const std::string cache_key = ModelTextureCacheKey(texture);
        {
            std::lock_guard lock{models_->mutex};
            const auto it = models_->texture_cache.find(cache_key);
            if (it != models_->texture_cache.end()) {
                return it->second;
            }
        }

        const TextureLoadDesc desc{
                .path = texture.path,
                .data = texture.data,
                .format = texture.srgb ? TextureFormat::RGBA8UnormSrgb : TextureFormat::RGBA8Unorm,
                .generate_mipmaps = true,
                .flip_vertically = false,
                .premultiply_alpha = false,
        };

        TextureHandle loaded_texture = LoadTexture2DAsync(desc);
        if (!loaded_texture.IsValid()) {
            return {};
        }

        std::lock_guard lock{models_->mutex};
        const auto existing = models_->texture_cache.find(cache_key);
        if (existing != models_->texture_cache.end()) {
            return existing->second;
        }

        models_->texture_cache[cache_key] = loaded_texture;
        return loaded_texture;
    }

    MaterialHandle RenderSystem::ResolveModelMaterial(const ModelMaterialAsset &material) {
        const ModelTextureAsset *base_color_texture = FindModelTexture(material, ModelTextureSemantic::BaseColor);
        if (base_color_texture != nullptr) {
            const TextureHandle albedo = LoadModelTexture(*base_color_texture);
            if (albedo.IsValid()) {
                return Material::TexturedUnlit(albedo, TexturedUnlitProps{.color = material.base_color}).Resolve(*this);
            }
        }

        return Material::Unlit(UnlitProps{.color = material.base_color}).Resolve(*this);
    }

    RenderSystem::UploadedModelResources RenderSystem::BuildModelResources(const ModelAsset &asset) {
        UploadedModelResources resources;
        if (backend_ == nullptr || !asset.IsValid()) {
            resources.error_message = "Invalid model asset";
            return resources;
        }

        resources.meshes.reserve(asset.meshes.size());
        resources.mesh_names.reserve(asset.meshes.size());
        resources.mesh_material_indices.reserve(asset.meshes.size());
        resources.materials.reserve(asset.materials.empty() ? 1u : asset.materials.size());
        resources.nodes = asset.nodes;

        if (asset.materials.empty()) {
            const MaterialHandle material = Material::Unlit().Resolve(*this);
            if (!material.IsValid()) {
                resources.error_message = backend_->LastError().empty() ? "Failed to resolve default model material"
                                                                        : std::string{backend_->LastError()};
                return resources;
            }
            resources.materials.push_back(material);
        } else {
            for (const ModelMaterialAsset &material_asset: asset.materials) {
                const MaterialHandle material = ResolveModelMaterial(material_asset);
                if (!material.IsValid()) {
                    resources.error_message = backend_->LastError().empty() ? "Failed to resolve model material"
                                                                            : std::string{backend_->LastError()};
                    return resources;
                }
                resources.materials.push_back(material);
            }
        }

        for (const ModelMeshAsset &mesh: asset.meshes) {
            const MeshDesc mesh_desc{
                    .vertices = mesh.vertices,
                    .indices = mesh.indices,
            };

            MeshHandle mesh_handle = backend_->UploadMesh(mesh_desc);
            if (!mesh_handle.IsValid()) {
                resources.error_message = backend_->LastError().empty() ? "Failed to upload model mesh"
                                                                        : std::string{backend_->LastError()};
                DestroyUploadedMeshes(*backend_, resources.meshes);
                return resources;
            }

            resources.meshes.push_back(mesh_handle);
            resources.mesh_names.push_back(mesh.name);
            resources.mesh_material_indices.push_back(
                    NormalizeModelMaterialIndex(mesh.material_index, resources.materials.size()));
        }

        if (resources.nodes.empty()) {
            resources.nodes.reserve(resources.meshes.size());
            for (std::uint32_t mesh_index = 0; mesh_index < resources.meshes.size(); ++mesh_index) {
                resources.nodes.push_back(ModelNodeAsset{
                        .name = MakeModelMeshNodeName(resources.mesh_names[mesh_index], mesh_index),
                        .parent_index = kInvalidModelNodeIndex,
                        .local_transform = Math::Identity(),
                        .mesh_indices = {mesh_index},
                });
            }
        }

        return resources;
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

    CameraData RenderSystem::BuildCameraData(const Math::Vec3 &position, const Math::Quat &rotation,
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
            data.projection =
                    Math::PerspectiveLH(Math::Deg2Rad(camera.fov_y_degrees), aspect_ratio, camera.near_z, camera.far_z);
            return data;
        }

        const float half_height = camera.orthographic_height * 0.5f;
        const float half_width = half_height * aspect_ratio;

        data.projection =
                Math::OrthoLH(-half_width, half_width, -half_height, half_height, camera.near_z, camera.far_z);

        return data;
    }
} // namespace CoreEngine
