#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "core/log/logger.h"

namespace CoreEngine {
    struct EditorLogEntry {
        std::chrono::system_clock::time_point timestamp{};
        LogLevel level = LogLevel::Info;
        std::string category;
        std::string text;
    };

    /**
     * @brief Captures recent engine log messages for editor presentation.
     *
     * Responsibility: provide a bounded, thread-safe log history for ImGui
     * panels without changing the logger ownership model or adding hot-path
     * allocations outside log submission.
     */
    class EditorLogSink final : public ILogSink {
    public:
        explicit EditorLogSink(std::size_t max_entries = 1024);

        void Write(const LogMessage &message) override;

        void Clear();

        [[nodiscard]] std::vector<EditorLogEntry> Snapshot() const;

        [[nodiscard]] std::size_t DroppedCount() const noexcept;

    private:
        mutable std::mutex mutex_;
        std::deque<EditorLogEntry> entries_;
        std::size_t max_entries_ = 1024;
        std::size_t dropped_count_ = 0;
    };
} // namespace CoreEngine
