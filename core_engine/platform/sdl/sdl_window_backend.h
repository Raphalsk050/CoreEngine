#pragma once

#include <string>

#include "core/window/i_window_backend.h"
#include "core/window/window_event.h"
#include "platform/sdl/sdl_context.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"

namespace CoreEngine {
    class SdlWindowBackend final : public IWindowBackend {
    public:
        explicit SdlWindowBackend(SdlContext &context);

        [[nodiscard]] bool Initialize(const WindowDesc &desc) override;

        void PollEvents(WindowEventQueue &queue) override;

        [[nodiscard]] bool HandleEvent(const SDL_Event &event, WindowEvent &out_event) noexcept;

        void Shutdown() override;

        [[nodiscard]] bool ShouldClose() const override;

        [[nodiscard]] NativeWindowHandle GetNativeHandle() const override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        SdlContext &context_;
        SDL_Window *window_ = nullptr;
        void *metal_view_ = nullptr;
        bool should_close_ = false;
        std::string last_error_;
    };
} // namespace CoreEngine