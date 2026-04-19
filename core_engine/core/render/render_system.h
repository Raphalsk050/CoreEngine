#pragma once

#include <memory>
#include <string_view>

#include "core/render/i_render_backend.h"

namespace CoreEngine {
    class RenderSystem final {
    public:
        explicit RenderSystem(std::unique_ptr<IRenderBackend> backend);

        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window);

        void RenderFrame();

        void Resize(int width, int height);

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const;

        [[nodiscard]] std::string_view LastError() const;

    private:
        std::unique_ptr<IRenderBackend> backend_;
        RenderDesc desc_{};
        bool initialized_ = false;
    };
} // namespace CoreEngine
