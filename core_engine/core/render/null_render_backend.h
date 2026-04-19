#pragma once

#include "core/render/i_render_backend.h"

namespace CoreEngine {
    class NullRenderBackend final : public IRenderBackend {
    public:
        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window) override;

        void BeginFrame() override;

        void Clear(const RenderClearColor &clear_color) override;

        void EndFrame() override;

        void Resize(int width, int height) override;

        void Shutdown() override;

        [[nodiscard]] std::string_view LastError() const override;
    };
} // namespace CoreEngine
