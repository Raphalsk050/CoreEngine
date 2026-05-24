#include "core/editor/editor_log_sink.h"

#include <algorithm>

namespace CoreEngine {
    EditorLogSink::EditorLogSink(std::size_t max_entries)
        : max_entries_(std::max<std::size_t>(max_entries, 1u)) {
    }

    void EditorLogSink::Write(const LogMessage &message) {
        std::lock_guard lock{mutex_};
        while (entries_.size() >= max_entries_) {
            entries_.pop_front();
            ++dropped_count_;
        }

        entries_.push_back(EditorLogEntry{
            .timestamp = message.metadata.timestamp,
            .level = message.metadata.level,
            .category = std::string{message.metadata.category},
            .text = message.text,
        });
    }

    void EditorLogSink::Clear() {
        std::lock_guard lock{mutex_};
        entries_.clear();
        dropped_count_ = 0;
    }

    std::vector<EditorLogEntry> EditorLogSink::Snapshot() const {
        std::lock_guard lock{mutex_};
        return {entries_.begin(), entries_.end()};
    }

    std::size_t EditorLogSink::DroppedCount() const noexcept {
        std::lock_guard lock{mutex_};
        return dropped_count_;
    }
} // namespace CoreEngine
