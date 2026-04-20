#include "core/render/render_system.h"

#include <cstddef>
#include <utility>

#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/render/primitives.h"

namespace CoreEngine {
    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend)
        : backend_(std::move(backend)) {}

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_ = desc;
        initialized_ = backend_ != nullptr && backend_->Initialize(desc, native_window);
        return initialized_;
    }

    void RenderSystem::RenderFrame(World &world) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }

        accumulator_.Clear();

        auto view = world.View<TransformComponent, MeshRendererComponent>();
        for (auto [entity, transform, renderer] : view.each()) {
            (void) entity;

            if (!renderer.visible || !renderer.material.IsValid() || !renderer.mesh.IsValid()) {
                continue;
            }

            accumulator_.Add(renderer.material, renderer.mesh, transform.WorldMatrix());
        }

        backend_->SetCamera(camera_);
        backend_->BeginFrame();
        backend_->Clear(desc_.clear_color);

        for (const RenderBatch &batch : accumulator_.Batches()) {
            backend_->SubmitBatch(batch);
        }

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

    void RenderSystem::DestroyMesh(MeshHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyMesh(handle);

        for (MeshHandle &primitive : primitive_cache_) {
            if (primitive == handle) {
                primitive = {};
            }
        }
    }

    void RenderSystem::SetCamera(const Camera &camera) {
        camera_ = camera.GetCameraData();
    }

    void RenderSystem::SetCamera(const CameraData &camera_data) {
        camera_ = camera_data;
    }

    void RenderSystem::Resize(int width, int height) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }
        backend_->Resize(width, height);
    }

    void RenderSystem::Shutdown() {
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
} // namespace CoreEngine
