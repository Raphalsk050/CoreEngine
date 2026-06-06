#include "core/input/input_system.h"

#include <utility>

namespace CoreEngine {
    namespace {
        [[nodiscard]] constexpr std::size_t ToIndex(Key key) noexcept { return static_cast<std::size_t>(key); }

        [[nodiscard]] constexpr std::size_t ToIndex(MouseButton button) noexcept {
            return static_cast<std::size_t>(button);
        }

        [[nodiscard]] constexpr float KeyAxisValue(bool negative, bool positive) noexcept {
            return (positive ? 1.0f : 0.0f) - (negative ? 1.0f : 0.0f);
        }
    } // namespace

    void InputSystem::BeginFrame() noexcept {
        previous_keys_ = current_keys_;
        previous_mouse_buttons_ = current_mouse_buttons_;
        pressed_keys_.reset();
        released_keys_.reset();
        pressed_mouse_buttons_.reset();
        released_mouse_buttons_.reset();
        mouse_delta_ = {};
        mouse_wheel_ = {};
        read_queue_.Clear();
    }

    void InputSystem::CommitFrame() noexcept {
        std::swap(read_queue_, write_queue_);
        write_queue_.Clear();
    }

    bool InputSystem::PushEvent(const InputEvent &event) noexcept {
        const bool queued = write_queue_.Push(event);
        ApplyEvent(event);
        return queued;
    }

    std::span<const InputEvent> InputSystem::Events() const noexcept { return read_queue_.Events(); }

    std::size_t InputSystem::DroppedEvents() const noexcept {
        return read_queue_.DroppedEvents() + write_queue_.DroppedEvents();
    }

    bool InputSystem::IsKeyDown(Key key) const noexcept { return IsValidKey(key) && current_keys_.test(ToIndex(key)); }

    bool InputSystem::WasKeyPressed(Key key) const noexcept {
        return IsValidKey(key) && pressed_keys_.test(ToIndex(key));
    }

    bool InputSystem::WasKeyReleased(Key key) const noexcept {
        return IsValidKey(key) && released_keys_.test(ToIndex(key));
    }

    bool InputSystem::IsMouseButtonDown(MouseButton button) const noexcept {
        return IsValidMouseButton(button) && current_mouse_buttons_.test(ToIndex(button));
    }

    bool InputSystem::WasMouseButtonPressed(MouseButton button) const noexcept {
        return IsValidMouseButton(button) && pressed_mouse_buttons_.test(ToIndex(button));
    }

    bool InputSystem::WasMouseButtonReleased(MouseButton button) const noexcept {
        return IsValidMouseButton(button) && released_mouse_buttons_.test(ToIndex(button));
    }

    InputVector2 InputSystem::MousePosition() const noexcept { return mouse_position_; }

    InputVector2 InputSystem::MouseDelta() const noexcept { return mouse_delta_; }

    InputVector2 InputSystem::MouseWheel() const noexcept { return mouse_wheel_; }

    bool InputSystem::BindButton(InputActionId action, Key key) noexcept {
        if (!IsValidAction(action) || !IsValidKey(key)) {
            return false;
        }

        button_bindings_[ActionIndex(action)] = ButtonBinding{
                .kind = BindingKind::Key,
                .key = key,
        };
        return true;
    }

    bool InputSystem::BindButton(InputActionId action, MouseButton button) noexcept {
        if (!IsValidAction(action) || !IsValidMouseButton(button)) {
            return false;
        }

        button_bindings_[ActionIndex(action)] = ButtonBinding{
                .kind = BindingKind::MouseButton,
                .mouse_button = button,
        };
        return true;
    }

    bool InputSystem::BindAxis2D(InputActionId action, Key negative_x, Key positive_x, Key negative_y,
                                 Key positive_y) noexcept {
        if (!IsValidAction(action) || !IsValidKey(negative_x) || !IsValidKey(positive_x) || !IsValidKey(negative_y) ||
            !IsValidKey(positive_y)) {
            return false;
        }

        axis2d_bindings_[ActionIndex(action)] = Axis2DBinding{
                .enabled = true,
                .negative_x = negative_x,
                .positive_x = positive_x,
                .negative_y = negative_y,
                .positive_y = positive_y,
        };
        return true;
    }

    bool InputSystem::IsActionDown(InputActionId action) const noexcept {
        return IsValidAction(action) && IsBindingDown(button_bindings_[ActionIndex(action)]);
    }

    bool InputSystem::WasActionPressed(InputActionId action) const noexcept {
        return IsValidAction(action) && WasBindingPressed(button_bindings_[ActionIndex(action)]);
    }

    bool InputSystem::WasActionReleased(InputActionId action) const noexcept {
        return IsValidAction(action) && WasBindingReleased(button_bindings_[ActionIndex(action)]);
    }

    InputVector2 InputSystem::GetAxis2D(InputActionId action) const noexcept {
        if (!IsValidAction(action)) {
            return {};
        }

        const Axis2DBinding &binding = axis2d_bindings_[ActionIndex(action)];
        if (!binding.enabled) {
            return {};
        }

        return InputVector2{
                .x = KeyAxisValue(IsKeyDown(binding.negative_x), IsKeyDown(binding.positive_x)),
                .y = KeyAxisValue(IsKeyDown(binding.negative_y), IsKeyDown(binding.positive_y)),
        };
    }

    bool InputSystem::IsValidKey(Key key) noexcept { return key != Key::Unknown && ToIndex(key) < KeyCount; }

    bool InputSystem::IsValidMouseButton(MouseButton button) noexcept {
        return button != MouseButton::Unknown && ToIndex(button) < MouseButtonCount;
    }

    std::size_t InputSystem::ActionIndex(InputActionId action) noexcept {
        return static_cast<std::size_t>(action.value - 1u);
    }

    bool InputSystem::IsValidAction(InputActionId action) noexcept {
        return action.IsValid() && action.value <= MaxActions;
    }

    void InputSystem::ApplyEvent(const InputEvent &event) noexcept {
        switch (event.type) {
            case InputEventType::KeyDown: {
                if (!IsValidKey(event.key)) {
                    return;
                }

                const std::size_t index = ToIndex(event.key);
                if (!event.repeat && !current_keys_.test(index)) {
                    pressed_keys_.set(index);
                }
                current_keys_.set(index);
                break;
            }

            case InputEventType::KeyUp: {
                if (!IsValidKey(event.key)) {
                    return;
                }

                const std::size_t index = ToIndex(event.key);
                if (current_keys_.test(index)) {
                    released_keys_.set(index);
                }
                current_keys_.reset(index);
                break;
            }

            case InputEventType::MouseButtonDown: {
                if (!IsValidMouseButton(event.mouse_button)) {
                    return;
                }

                const std::size_t index = ToIndex(event.mouse_button);
                if (!current_mouse_buttons_.test(index)) {
                    pressed_mouse_buttons_.set(index);
                }
                current_mouse_buttons_.set(index);
                mouse_position_ = {.x = event.x, .y = event.y};
                break;
            }

            case InputEventType::MouseButtonUp: {
                if (!IsValidMouseButton(event.mouse_button)) {
                    return;
                }

                const std::size_t index = ToIndex(event.mouse_button);
                if (current_mouse_buttons_.test(index)) {
                    released_mouse_buttons_.set(index);
                }
                current_mouse_buttons_.reset(index);
                mouse_position_ = {.x = event.x, .y = event.y};
                break;
            }

            case InputEventType::MouseMoved:
                mouse_position_ = {.x = event.x, .y = event.y};
                mouse_delta_.x += event.delta_x;
                mouse_delta_.y += event.delta_y;
                break;

            case InputEventType::MouseWheel:
                mouse_wheel_.x += event.delta_x;
                mouse_wheel_.y += event.delta_y;
                break;

            case InputEventType::FocusLost: ClearState(); break;
        }
    }

    void InputSystem::ClearState() noexcept {
        current_keys_.reset();
        previous_keys_.reset();
        pressed_keys_.reset();
        released_keys_.reset();
        current_mouse_buttons_.reset();
        previous_mouse_buttons_.reset();
        pressed_mouse_buttons_.reset();
        released_mouse_buttons_.reset();
        mouse_delta_ = {};
        mouse_wheel_ = {};
    }

    bool InputSystem::IsBindingDown(const ButtonBinding &binding) const noexcept {
        switch (binding.kind) {
            case BindingKind::Key:         return IsKeyDown(binding.key);
            case BindingKind::MouseButton: return IsMouseButtonDown(binding.mouse_button);
            case BindingKind::None:        return false;
        }

        return false;
    }

    bool InputSystem::WasBindingPressed(const ButtonBinding &binding) const noexcept {
        switch (binding.kind) {
            case BindingKind::Key:         return WasKeyPressed(binding.key);
            case BindingKind::MouseButton: return WasMouseButtonPressed(binding.mouse_button);
            case BindingKind::None:        return false;
        }

        return false;
    }

    bool InputSystem::WasBindingReleased(const ButtonBinding &binding) const noexcept {
        switch (binding.kind) {
            case BindingKind::Key:         return WasKeyReleased(binding.key);
            case BindingKind::MouseButton: return WasMouseButtonReleased(binding.mouse_button);
            case BindingKind::None:        return false;
        }

        return false;
    }
} // namespace CoreEngine
