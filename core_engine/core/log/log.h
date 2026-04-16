#pragma once
#include <string_view>

namespace CoreEngine {
    enum class LogLevel;

    class ILogService {
    public:
        virtual ~ILogService() = default;

        virtual void Log(LogLevel level, std::string_view category, std::string_view message) = 0;
    };

    class Log {
    public:
        static void Bind(ILogService &service);

        static void Unbind();

        static void Debug(std::string_view category, std::string_view message);

        static void Info(std::string_view category, std::string_view message);

        static void Warn(std::string_view category, std::string_view message);

        static void Error(std::string_view category, std::string_view message);

        static void Fatal(std::string_view category, std::string_view message);
    };
}
