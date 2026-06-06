// core/window/window_event_queue.h
#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "core/window/window_event.h"

namespace CoreEngine {
    class WindowEventQueue final {
    public:
        static constexpr std::size_t MaxEvents = 128;

        void Clear() noexcept {
            size_ = 0;
            droppedEvents_ = 0;
        }

        bool Push(const WindowEvent &event) noexcept {
            if (size_ >= events_.size()) {
                ++droppedEvents_;
                return false;
            }

            events_[size_] = event;
            ++size_;
            return true;
        }

        [[nodiscard]] std::span<const WindowEvent> Events() const noexcept { return {events_.data(), size_}; }

        [[nodiscard]] bool Empty() const noexcept { return size_ == 0; }

        [[nodiscard]] std::size_t Size() const noexcept { return size_; }

        [[nodiscard]] std::size_t DroppedEvents() const noexcept { return droppedEvents_; }

    private:
        std::array<WindowEvent, MaxEvents> events_{};
        std::size_t size_ = 0;
        std::size_t droppedEvents_ = 0;
    };
} // namespace CoreEngine
