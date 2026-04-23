#include "sdl_window_backend.h"

#include "core/window/window_event.h"
#include "core/window/window_event_queue.h"
#include "SDL3/SDL_properties.h"

#if defined(__APPLE__)
#include "SDL3/SDL_metal.h"
#endif

namespace CoreEngine {
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
                out_event = WindowEvent{
                    .type = WindowEventType::Resized,
                    .width = event.window.data1,
                    .height = event.window.data2,
                };
                return true;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                out_event = WindowEvent{
                    .type = WindowEventType::PixelSizeChanged,
                    .width = event.window.data1,
                    .height = event.window.data2,
                };
                return true;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                out_event = WindowEvent{.type = WindowEventType::FocusGained};
                return true;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                out_event = WindowEvent{.type = WindowEventType::FocusLost};
                return true;

            case SDL_EVENT_WINDOW_MINIMIZED:
                out_event = WindowEvent{.type = WindowEventType::Minimized};
                return true;

            case SDL_EVENT_WINDOW_RESTORED:
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
#elif defined(__APPLE__)
        if (metal_view_ != nullptr) {
            native_handle.platform = NativeWindowPlatform::MacOS;
            native_handle.window = metal_view_;
        }
#endif

        return native_handle;
    }

    std::string_view SdlWindowBackend::LastError() const {
        return last_error_;
    }
} // namespace CoreEngine