#include "platform/sdl/sdl_platform_event_pump.h"

#include "core/window/window_system.h"
#include "platform/sdl/sdl_input_backend.h"
#include "platform/sdl/sdl_window_backend.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "SDL3/SDL_events.h"

namespace CoreEngine {
    SdlPlatformEventPump::SdlPlatformEventPump(SdlWindowBackend &window_backend, SdlInputBackend &input_backend)
        : window_backend_(window_backend), input_backend_(input_backend) {
    }

    void SdlPlatformEventPump::PumpEvents(WindowSystem &window_system) noexcept {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (ImGui::GetCurrentContext() != nullptr) {
                (void) ImGui_ImplSDL3_ProcessEvent(&event);
            }

            WindowEvent window_event;
            if (window_backend_.HandleEvent(event, window_event)) {
                (void) window_system.PushEvent(window_event);
            }

            input_backend_.HandleEvent(event);
        }
    }
} // namespace CoreEngine
