#pragma once

#include <string_view>

#include "core/render/camera_data.h"
#include "core/render/material_desc.h"
#include "core/render/mesh_desc.h"
#include "core/render/render_batch.h"
#include "core/render/render_desc.h"
#include "core/window/native_window_handle.h"

namespace CoreEngine {
    struct PerFrameProps {
        const CameraData &camera;
        Math::Vec4 frame_clock;
    };

    class IRenderBackend {
    public:
        virtual ~IRenderBackend() = default;

        [[nodiscard]] virtual bool Initialize(const RenderDesc &desc,
                                              NativeWindowHandle native_window) = 0;

        virtual void BeginFrame() = 0;

        virtual void Clear(const RenderClearColor &clear_color) = 0;

        virtual void BeginImGuiFrame() = 0;

        virtual void RenderImGui() = 0;

        virtual void EndFrame() = 0;

        virtual void Resize(int width, int height) = 0;

        virtual void Shutdown() = 0;

        [[nodiscard]] virtual MeshHandle UploadMesh(const MeshDesc &desc) = 0;

        virtual void DestroyMesh(MeshHandle handle) = 0;

        [[nodiscard]] virtual MaterialHandle ResolveMaterial(const MaterialDesc &desc) = 0;

        virtual void SetPerFrameProps(PerFrameProps props) = 0;

        virtual void SubmitBatch(const RenderBatch &batch) = 0;

        [[nodiscard]] virtual std::string_view LastError() const = 0;
    };
} // namespace CoreEngine
