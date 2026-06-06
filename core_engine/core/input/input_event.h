#pragma once

#include "core/input/input_codes.h"

namespace CoreEngine {
    enum class InputEventType { KeyDown, KeyUp, MouseButtonDown, MouseButtonUp, MouseMoved, MouseWheel, FocusLost };

    struct InputEvent {
        InputEventType type = InputEventType::FocusLost;
        Key key = Key::Unknown;
        MouseButton mouse_button = MouseButton::Unknown;
        float x = 0.0f;
        float y = 0.0f;
        float delta_x = 0.0f;
        float delta_y = 0.0f;
        bool repeat = false;
    };
} // namespace CoreEngine
