#include "sdl_window_backend.h"

#include "core/window/window_event.h"
#include "core/window/window_event_queue.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_properties.h"

#if defined(__APPLE__)
#include "SDL3/SDL_metal.h"
#endif

namespace CoreEngine {
    namespace {
        [[nodiscard]] WindowExtent GetCurrentWindowSize(SDL_Window *window, WindowExtent fallback) noexcept {
            int width = 0;
            int height = 0;
            if (window != nullptr && SDL_GetWindowSize(window, &width, &height) && width > 0 && height > 0) {
                return WindowExtent{.width = width, .height = height};
            }

            return fallback;
        }

        void WarpMouseToWindowCenter(SDL_Window *window, WindowExtent fallback) noexcept {
            const WindowExtent extent = GetCurrentWindowSize(window, fallback);
            if (!extent.IsValid()) {
                return;
            }

            SDL_WarpMouseInWindow(
                window,
                static_cast<float>(extent.width) * 0.5f,
                static_cast<float>(extent.height) * 0.5f);
        }
    }

    bool SdlWindowBackend::SetWindowCursorMode(WindowCursorMode cursor_mode) {
        if (window_ == nullptr) {
            last_error_ = "Window is not initialized";
            return false;
        }

        if (state_.cursor_mode == cursor_mode) {
            return true;
        }

        bool success = false;

        switch (cursor_mode) {
            case WindowCursorMode::CURSOR_CONSTRAINED: {
                const bool relative_disabled = SDL_SetWindowRelativeMouseMode(window_, false);
                const bool rect_cleared = SDL_SetWindowMouseRect(window_, nullptr);
                const bool grabbed = SDL_SetWindowMouseGrab(window_, true);
                const bool visible = SDL_ShowCursor();
                success = relative_disabled && rect_cleared && grabbed && visible;
                break;
            }
            case WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN: {
                const bool rect_cleared = SDL_SetWindowMouseRect(window_, nullptr);
                const bool grab_released = SDL_SetWindowMouseGrab(window_, false);
                const bool relative_enabled = SDL_SetWindowRelativeMouseMode(window_, true);
                success = rect_cleared && grab_released && relative_enabled;
                break;
            }
            case WindowCursorMode::CURSOR_NORMAL: {
                WarpMouseToWindowCenter(window_, state_.logical_size);
                const bool relative_disabled = SDL_SetWindowRelativeMouseMode(window_, false);
                const bool grab_released = SDL_SetWindowMouseGrab(window_, false);
                const bool rect_cleared = SDL_SetWindowMouseRect(window_, nullptr);
                const bool visible = SDL_ShowCursor();
                success = relative_disabled && grab_released && rect_cleared && visible;
                break;
            }
            default:
                last_error_ = "Unsupported window cursor mode";
                return false;
        }

        if (success) {
            state_.cursor_mode = cursor_mode;
            last_error_.clear();
            return true;
        }

        last_error_ = SDL_GetError();
        return false;
    }

    SdlWindowBackend::SdlWindowBackend(SdlContext &context) : context_(context) {
    }

    bool SdlWindowBackend::Initialize(const WindowDesc &desc) {
        if (!context_.InitializeVideo()) {
            last_error_ = SDL_GetError();
            return false;
        }

        SDL_WindowFlags flags = 0;

        if (desc.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        if (!desc.decorated) {
            flags |= SDL_WINDOW_BORDERLESS;
        }

        if (desc.fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN;
        }

        if (desc.highDpi) {
            flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
        }

        if (desc.surface_type == WindowSurfaceType::Metal) {
            flags |= SDL_WINDOW_METAL;
        }

        window_ = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
        if (window_ == nullptr) {
            last_error_ = SDL_GetError();
            return false;
        }

#if defined(__APPLE__)
        if (desc.surface_type == WindowSurfaceType::Metal) {
            metal_view_ = SDL_Metal_CreateView(window_);
            if (metal_view_ == nullptr) {
                last_error_ = SDL_GetError();
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                return false;
            }
        }
#else
        if (desc.surface_type == WindowSurfaceType::Metal) {
            last_error_ = "Metal window surfaces are only supported on Apple platforms";
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            return false;
        }
#endif

        should_close_ = false;
        state_.logical_size = GetCurrentWindowSize(window_, WindowExtent{.width = desc.width, .height = desc.height});
        state_.pixel_size = state_.logical_size;
        (void) SDL_GetWindowSizeInPixels(window_, &state_.pixel_size.width, &state_.pixel_size.height);
        state_.cursor_mode = WindowCursorMode::CURSOR_NORMAL;
        return true;
    }

    void SdlWindowBackend::PollEvents(WindowEventQueue &queue) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            WindowEvent window_event;
            if (HandleEvent(event, window_event)) {
                queue.Push(window_event);
            }
        }
    }

    bool SdlWindowBackend::HandleEvent(const SDL_Event &event, WindowEvent &out_event) noexcept {
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                should_close_ = true;
                out_event = WindowEvent{.type = WindowEventType::CloseRequested};
                return true;

            case SDL_EVENT_WINDOW_RESIZED:
                state_.logical_size = WindowExtent{
                    .width = event.window.data1,
                    .height = event.window.data2,
                };
                out_event = WindowEvent{
                    .type = WindowEventType::Resized,
                    .width = event.window.data1,
                    .height = event.window.data2,
                };
                return true;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                state_.pixel_size = WindowExtent{
                    .width = event.window.data1,
                    .height = event.window.data2,
                };
                out_event = WindowEvent{
                    .type = WindowEventType::PixelSizeChanged,
                    .width = event.window.data1,
                    .height = event.window.data2,
                };
                return true;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                state_.focused = true;
                out_event = WindowEvent{.type = WindowEventType::FocusGained};
                return true;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                state_.focused = false;
                out_event = WindowEvent{.type = WindowEventType::FocusLost};
                return true;

            case SDL_EVENT_WINDOW_MINIMIZED:
                state_.minimized = true;
                out_event = WindowEvent{.type = WindowEventType::Minimized};
                return true;

            case SDL_EVENT_WINDOW_RESTORED:
                state_.minimized = false;
                out_event = WindowEvent{.type = WindowEventType::Restored};
                return true;

            default:
                return false;
        }
    }

    void SdlWindowBackend::Shutdown() {
#if defined(__APPLE__)
        if (metal_view_ != nullptr) {
            SDL_Metal_DestroyView(static_cast<SDL_MetalView>(metal_view_));
            metal_view_ = nullptr;
        }
#else
        metal_view_ = nullptr;
#endif

        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        should_close_ = false;
    }

    bool SdlWindowBackend::ShouldClose() const {
        return should_close_;
    }

    NativeWindowHandle SdlWindowBackend::GetNativeHandle() const {
        NativeWindowHandle native_handle;

        if (window_ == nullptr) {
            return native_handle;
        }

#ifdef _WIN32
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window_);
        native_handle.platform = NativeWindowPlatform::Win32;
        native_handle.window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        native_handle.platform_window = window_;
#elif defined(__APPLE__)
        if (metal_view_ != nullptr) {
            native_handle.platform = NativeWindowPlatform::MacOS;
            native_handle.window = metal_view_;
            native_handle.platform_window = window_;
        }
#endif

        return native_handle;
    }

    const WindowState &SdlWindowBackend::GetState() const {
        return state_;
    }

    std::string_view SdlWindowBackend::LastError() const {
        return last_error_;
    }
} // namespace CoreEngine
