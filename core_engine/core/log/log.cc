#include "core/log/log.h"

#include <atomic>

#include "core/log/logger.h"

namespace CoreEngine {
    std::atomic<ILogService *> gLogger{nullptr};

    void Write(LogLevel level, std::string_view category, std::string_view message) {
        ILogService *log_service = gLogger.load(std::memory_order_acquire);
        if (log_service == nullptr) {
            return;
        }

        log_service->Log(level, category, message);
    }

    void Log::Bind(ILogService &service) { gLogger.store(&service, std::memory_order_release); }

    void Log::Unbind() { gLogger.store(nullptr, std::memory_order_release); }

    void Log::Debug(std::string_view category, std::string_view message) { Write(LogLevel::Debug, category, message); }

    void Log::Info(std::string_view category, std::string_view message) { Write(LogLevel::Info, category, message); }

    void Log::Warn(std::string_view category, std::string_view message) { Write(LogLevel::Warn, category, message); }

    void Log::Error(std::string_view category, std::string_view message) { Write(LogLevel::Error, category, message); }

    void Log::Fatal(std::string_view category, std::string_view message) { Write(LogLevel::Fatal, category, message); }
} // namespace CoreEngine
