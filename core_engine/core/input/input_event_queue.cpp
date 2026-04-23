#include "core/input/input_event_queue.h"

namespace CoreEngine {
    void InputEventQueue::Clear() noexcept {
        size_ = 0;
        dropped_events_ = 0;
    }

    bool InputEventQueue::Push(const InputEvent &event) noexcept {
        if (size_ >= events_.size()) {
            ++dropped_events_;
            return false;
        }

        events_[size_] = event;
        ++size_;
        return true;
    }

    std::span<const InputEvent> InputEventQueue::Events() const noexcept {
        return {events_.data(), size_};
    }

    bool InputEventQueue::Empty() const noexcept {
        return size_ == 0;
    }

    std::size_t InputEventQueue::Size() const noexcept {
        return size_;
    }

    std::size_t InputEventQueue::DroppedEvents() const noexcept {
        return dropped_events_;
    }
} // namespace CoreEngine