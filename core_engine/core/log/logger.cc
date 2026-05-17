#include "core/log/logger.h"
#include <mutex>

namespace CoreEngine {
    void Logger::AddSink(const std::shared_ptr<ILogSink> &sink) {
        if (!sink) {
            return;
        }

        std::unique_lock lock(sinksMutex);
        auto updatedSinks = std::make_shared<SinkList>(*sinks);
        updatedSinks->push_back(sink);
        sinks = std::move(updatedSinks);
    }

    void Logger::SetMinLevel(LogLevel level) {
        minLevel.store(level, std::memory_order_release);
    }

    void Logger::Log(LogLevel level, std::string_view category, std::string_view message) {
        if (level < minLevel.load(std::memory_order_acquire)) {
            return;
        }

        std::shared_ptr<const SinkList> localSinks;
        {
            std::shared_lock lock(sinksMutex);
            localSinks = sinks;
        }
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
