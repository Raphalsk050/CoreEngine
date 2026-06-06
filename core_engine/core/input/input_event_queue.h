#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "core/input/input_event.h"

namespace CoreEngine {
    class InputEventQueue final {
    public:
        static constexpr std::size_t MaxEvents = 1024;

        void Clear() noexcept;

        [[nodiscard]] bool Push(const InputEvent &event) noexcept;

        [[nodiscard]] std::span<const InputEvent> Events() const noexcept;

        [[nodiscard]] bool Empty() const noexcept;

        [[nodiscard]] std::size_t Size() const noexcept;

        [[nodiscard]] std::size_t DroppedEvents() const noexcept;

    private:
        std::array<InputEvent, MaxEvents> events_{};
        std::size_t size_ = 0;
        std::size_t dropped_events_ = 0;
    };
} // namespace CoreEngine
