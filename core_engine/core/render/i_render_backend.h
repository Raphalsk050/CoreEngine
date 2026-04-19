#pragma once

#include <string_view>

#include "core/render/i_render_context.h"
#include "core/render/render_desc.h"
#include "core/window/native_window_handle.h"

namespace CoreEngine {
    class IRenderBackend : public IRenderContext {
    public:
        ~IRenderBackend() override = default;

        [[nodiscard]] virtual bool Initialize(const RenderDesc &desc,
                                              NativeWindowHandle native_window) = 0;

        virtual void BeginFrame() = 0;

        virtual void Clear(const RenderClearColor &clear_color) = 0;

        virtual void EndFrame() = 0;

        virtual void Resize(int width, int height) = 0;

        virtual void Shutdown() = 0;

        [[nodiscard]] virtual std::string_view LastError() const = 0;
    };
} // namespace CoreEngine
