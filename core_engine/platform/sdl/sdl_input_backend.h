#pragma once

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_scancode.h"
#include "core/input/input_codes.h"
#include "core/input/input_system.h"

namespace CoreEngine {
    class SdlInputBackend final {
    public:
        explicit SdlInputBackend(InputSystem &input_system);

        void HandleEvent(const SDL_Event &event) noexcept;

    private:
        [[nodiscard]] static Key TranslateScancode(SDL_Scancode scancode) noexcept;

        [[nodiscard]] static MouseButton TranslateMouseButton(uint8_t button) noexcept;

        InputSystem &input_system_;
    };
} // namespace CoreEngine
