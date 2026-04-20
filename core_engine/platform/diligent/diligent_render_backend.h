#pragma once

#include <memory>

#include "core/render/i_render_backend.h"

namespace CoreEngine {
    enum class DiligentRenderBackendApi {
        D3D11,
        D3D12
    };

    class DiligentRenderBackend final : public IRenderBackend {
    public:
        struct Impl;

        explicit DiligentRenderBackend(DiligentRenderBackendApi api);

        ~DiligentRenderBackend() override;

        [[nodiscard]] bool Initialize(const RenderDesc &desc,
                                      NativeWindowHandle native_window) override;

        void BeginFrame() override;

        void Clear(const RenderClearColor &clear_color) override;

        void EndFrame() override;

        void Resize(int width, int height) override;

        void Shutdown() override;

        [[nodiscard]] MeshHandle UploadMesh(const MeshDesc &desc) override;

        void DestroyMesh(MeshHandle handle) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        void SetCamera(const CameraData &camera) override;

        void SubmitBatch(const RenderBatch &batch) override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        std::unique_ptr<Impl> impl_;
    };
} // namespace CoreEngine