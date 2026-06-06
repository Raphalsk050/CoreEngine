#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/input/input_codes.h"
#include "core/input/input_event.h"
#include "core/input/input_event_queue.h"

namespace CoreEngine {
    struct InputVector2 {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct InputActionId {
        uint16_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
    };

    [[nodiscard]] constexpr InputActionId MakeInputActionId(uint16_t value) noexcept { return InputActionId{value}; }

    class InputSystem final {
    public:
        static constexpr std::size_t MaxActions = 128;

        void BeginFrame() noexcept;

        void CommitFrame() noexcept;

        [[nodiscard]] bool PushEvent(const InputEvent &event) noexcept;

        [[nodiscard]] std::span<const InputEvent> Events() const noexcept;

        [[nodiscard]] std::size_t DroppedEvents() const noexcept;

        [[nodiscard]] bool IsKeyDown(Key key) const noexcept;

        [[nodiscard]] bool WasKeyPressed(Key key) const noexcept;

        [[nodiscard]] bool WasKeyReleased(Key key) const noexcept;

        [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const noexcept;

        [[nodiscard]] bool WasMouseButtonPressed(MouseButton button) const noexcept;

        [[nodiscard]] bool WasMouseButtonReleased(MouseButton button) const noexcept;

        [[nodiscard]] InputVector2 MousePosition() const noexcept;

        [[nodiscard]] InputVector2 MouseDelta() const noexcept;

        [[nodiscard]] InputVector2 MouseWheel() const noexcept;

        [[nodiscard]] bool BindButton(InputActionId action, Key key) noexcept;

        [[nodiscard]] bool BindButton(InputActionId action, MouseButton button) noexcept;

        [[nodiscard]] bool BindAxis2D(InputActionId action, Key negative_x, Key positive_x, Key negative_y,
                                      Key positive_y) noexcept;

        [[nodiscard]] bool IsActionDown(InputActionId action) const noexcept;

        [[nodiscard]] bool WasActionPressed(InputActionId action) const noexcept;

        [[nodiscard]] bool WasActionReleased(InputActionId action) const noexcept;

        [[nodiscard]] InputVector2 GetAxis2D(InputActionId action) const noexcept;

    private:
        enum class BindingKind : uint8_t { None, Key, MouseButton };

        struct ButtonBinding {
            BindingKind kind = BindingKind::None;
            Key key = Key::Unknown;
            MouseButton mouse_button = MouseButton::Unknown;
        };

        struct Axis2DBinding {
            bool enabled = false;
            Key negative_x = Key::Unknown;
            Key positive_x = Key::Unknown;
            Key negative_y = Key::Unknown;
            Key positive_y = Key::Unknown;
        };

        static constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count);
        static constexpr std::size_t MouseButtonCount = static_cast<std::size_t>(MouseButton::Count);

        [[nodiscard]] static bool IsValidKey(Key key) noexcept;

        [[nodiscard]] static bool IsValidMouseButton(MouseButton button) noexcept;

        [[nodiscard]] static std::size_t ActionIndex(InputActionId action) noexcept;

        [[nodiscard]] static bool IsValidAction(InputActionId action) noexcept;

        void ApplyEvent(const InputEvent &event) noexcept;

        void ClearState() noexcept;

        [[nodiscard]] bool IsBindingDown(const ButtonBinding &binding) const noexcept;

        [[nodiscard]] bool WasBindingPressed(const ButtonBinding &binding) const noexcept;

        [[nodiscard]] bool WasBindingReleased(const ButtonBinding &binding) const noexcept;

        std::bitset<KeyCount> current_keys_{};
        std::bitset<KeyCount> previous_keys_{};
        std::bitset<KeyCount> pressed_keys_{};
        std::bitset<KeyCount> released_keys_{};

        std::bitset<MouseButtonCount> current_mouse_buttons_{};
        std::bitset<MouseButtonCount> previous_mouse_buttons_{};
        std::bitset<MouseButtonCount> pressed_mouse_buttons_{};
        std::bitset<MouseButtonCount> released_mouse_buttons_{};

        InputVector2 mouse_position_{};
        InputVector2 mouse_delta_{};
        InputVector2 mouse_wheel_{};

        InputEventQueue write_queue_{};
        InputEventQueue read_queue_{};

        std::array<ButtonBinding, MaxActions> button_bindings_{};
        std::array<Axis2DBinding, MaxActions> axis2d_bindings_{};
    };
} // namespace CoreEngine
