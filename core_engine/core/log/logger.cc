#include "core/log/logger.h"

namespace CoreEngine {
    void Logger::AddSink(const std::shared_ptr<ILogSink> &sink) {
        if (!sink) {
            return;
        }

        const auto currentSinks = std::atomic_load_explicit(&sinks, std::memory_order_acquire);
        auto updatedSinks = std::make_shared<SinkList>(*currentSinks);
        updatedSinks->push_back(sink);
        std::atomic_store_explicit(
            &sinks,
            std::shared_ptr<const SinkList>(std::move(updatedSinks)),
            std::memory_order_release);
    }

    void Logger::SetMinLevel(LogLevel level) {
        minLevel.store(level, std::memory_order_release);
    }

    void Logger::Log(LogLevel level, std::string_view category, std::string_view message) {
        if (level < minLevel.load(std::memory_order_acquire)) {
            return;
        }

        const auto localSinks = std::atomic_load_explicit(&sinks, std::memory_order_acquire);
        if (localSinks->empty()) {
            return;
        }

        const auto safeCategory = category.empty() ? std::string_view{"General"} : category;
        const auto now = std::chrono::system_clock::now();
        const auto threadId = std::this_thread::get_id();

        LogMessage logMessage{
            .metadata = {
                .timestamp = now,
                .level = level,
                .category = safeCategory,
                .threadId = threadId,
            },
            .text = std::string(message),
        };

        for (const auto &sink: *localSinks) {
            if (sink) {
                sink->Write(logMessage);
            }
        }
    }
}
