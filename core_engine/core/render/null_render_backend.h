#pragma once

#include "core/render/i_render_backend.h"

namespace CoreEngine {
    class NullRenderBackend final : public IRenderBackend {
    public:
        [[nodiscard]] bool Initialize(const RenderDesc &desc,
                                      NativeWindowHandle native_window) override;

        void BeginFrame() override;

        void Clear(const RenderClearColor &clear_color) override;

        void BeginImGuiFrame() override;

        void RenderImGui() override;

        void EndFrame() override;

        void Resize(int width, int height) override;

        void Shutdown() override;

        [[nodiscard]] MeshHandle UploadMesh(const MeshDesc &desc) override;

        void DestroyMesh(MeshHandle handle) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        void SetPerFrameProps(PerFrameProps props) override;

        void SubmitBatch(const RenderBatch &batch) override;

        [[nodiscard]] std::string_view LastError() const override;
    };
} // namespace CoreEngine
