#include "core/render/render_system.h"

#include <utility>

#include "core/ecs/world.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/mesh_renderer_component.h"

namespace CoreEngine {
    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend)
        : backend_(std::move(backend)) {}

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_        = desc;
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
            if (!renderer.visible) {
                continue;
            }

            const MaterialHandle mat  = renderer.material.Resolve(*backend_);
            const MeshHandle     mesh = renderer.mesh;

            if (!mat.IsValid() || !mesh.IsValid()) {
                continue;
            }

            accumulator_.Add(mat, mesh, transform.WorldMatrix());
        }

        backend_->SetCamera(camera_);
        backend_->BeginFrame();
        backend_->Clear(desc_.clear_color);

        for (const RenderBatch &batch : accumulator_.Batches()) {
            backend_->SubmitBatch(batch);
        }

        backend_->EndFrame();
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
        initialized_ = false;
    }

    bool RenderSystem::IsInitialized() const {
        return initialized_;
    }

    std::string_view RenderSystem::LastError() const {
        return backend_ != nullptr ? backend_->LastError() : "Render backend is not available";
    }

    IRenderContext &RenderSystem::Context() {
        return *backend_;
    }
} // namespace CoreEngine