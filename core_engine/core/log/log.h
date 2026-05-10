#pragma once
#include <format>
#include <string>
#include <string_view>
#include <utility>

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

        template<typename... Args>
        static void Debug(std::string_view category, std::format_string<Args...> fmt, Args &&...args) {
            Debug(category, std::format(fmt, std::forward<Args>(args)...));
        }

        static void Info(std::string_view category, std::string_view message);

        template<typename... Args>
        static void Info(std::string_view category, std::format_string<Args...> fmt, Args &&...args) {
            Info(category, std::format(fmt, std::forward<Args>(args)...));
        }

        static void Warn(std::string_view category, std::string_view message);

        template<typename... Args>
        static void Warn(std::string_view category, std::format_string<Args...> fmt, Args &&...args) {
            Warn(category, std::format(fmt, std::forward<Args>(args)...));
        }

        static void Error(std::string_view category, std::string_view message);

        template<typename... Args>
        static void Error(std::string_view category, std::format_string<Args...> fmt, Args &&...args) {
            Error(category, std::format(fmt, std::forward<Args>(args)...));
        }

        static void Fatal(std::string_view category, std::string_view message);

        template<typename... Args>
        static void Fatal(std::string_view category, std::format_string<Args...> fmt, Args &&...args) {
            Fatal(category, std::format(fmt, std::forward<Args>(args)...));
        }
    };
}
