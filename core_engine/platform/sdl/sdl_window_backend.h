#pragma once

#include <string>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "core/window/i_window_backend.h"
#include "core/window/window_event.h"
#include "platform/sdl/sdl_context.h"

namespace CoreEngine {
    class SdlWindowBackend final : public IWindowBackend {
    public:
        [[nodiscard]] bool SetWindowCursorMode(WindowCursorMode cursor_mode) override;

        explicit SdlWindowBackend(SdlContext &context);

        [[nodiscard]] bool Initialize(const WindowDesc &desc) override;

        void PollEvents(WindowEventQueue &queue) override;

        [[nodiscard]] bool HandleEvent(const SDL_Event &event, WindowEvent &out_event) noexcept;

        void Shutdown() override;

        [[nodiscard]] bool ShouldClose() const override;

        [[nodiscard]] NativeWindowHandle GetNativeHandle() const override;

        [[nodiscard]] const WindowState &GetState() const override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        SdlContext &context_;
        SDL_Window *window_ = nullptr;
        void *metal_view_ = nullptr;
        bool should_close_ = false;
        WindowState state_{};
        std::string last_error_;
    };
} // namespace CoreEngine
