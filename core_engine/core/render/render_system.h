#pragma once

#include <memory>
#include <string_view>

#include "core/render/camera.h"
#include "core/render/camera_data.h"
#include "core/render/i_render_backend.h"
#include "core/render/render_batch.h"

namespace CoreEngine {
    class World;

    class RenderSystem final {
    public:
        explicit RenderSystem(std::unique_ptr<IRenderBackend> backend);

        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window);

        void RenderFrame(World &world);

        void SetCamera(const Camera &camera);

        void SetCamera(const CameraData &camera_data);

        void Resize(int width, int height);

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const;

        [[nodiscard]] std::string_view LastError() const;

        [[nodiscard]] IRenderContext &Context();

    private:
        std::unique_ptr<IRenderBackend> backend_;
        RenderDesc                      desc_{};
        CameraData                      camera_{};
        BatchAccumulator                accumulator_;
        bool                            initialized_ = false;
    };
} // namespace CoreEngine
