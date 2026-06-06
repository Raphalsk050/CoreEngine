#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "log.h"

namespace CoreEngine {
    enum class LogLevel { Debug, Info, Warn, Error, Fatal };

    struct LogMetadata {
        // TODO(rafael): Create a timestamp class in the future
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string_view category;
        std::thread::id threadId;
    };

    struct LogMessage {
        LogMetadata metadata;
        std::string text;
    };

    class ILogSink {
    public:
        virtual ~ILogSink() = default;

        virtual void Write(const LogMessage &message) = 0;
    };

    class Logger final : public ILogService {
    public:
        void Log(LogLevel level, std::string_view category, std::string_view message) override;

        void AddSink(const std::shared_ptr<ILogSink> &sink);

        void SetMinLevel(LogLevel level);

    private:
        using SinkList = std::vector<std::shared_ptr<ILogSink>>;

        std::atomic<LogLevel> minLevel{LogLevel::Debug};
        mutable std::shared_mutex sinksMutex;
        std::shared_ptr<const SinkList> sinks{std::make_shared<SinkList>()};
    };
} // namespace CoreEngine
