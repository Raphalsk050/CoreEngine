#pragma once

#include "core/log/logger.h"

namespace CoreEngine {
    class ConsoleSink : public ILogSink {
    public:
        void Write(const LogMessage &message) override;

        static const char *GetColorCode(LogLevel level) {
            switch (level) {
                case LogLevel::Debug: return "\x1b[90m"; // gray
                case LogLevel::Info: return "\x1b[32m"; // green
                case LogLevel::Warn: return "\x1b[33m"; // yellow
                case LogLevel::Error: return "\x1b[31m"; // red
                case LogLevel::Fatal: return "\x1b[97;41m"; // white on red
                default: return "\x1b[0m";
            }
        }

        static const char *ToString(const LogLevel level) {
            switch (level) {
                case LogLevel::Debug: return "DEBUG";
                case LogLevel::Info: return "INFO";
                case LogLevel::Warn: return "WARN";
                case LogLevel::Error: return "ERROR";
                case LogLevel::Fatal: return "FATAL";
                default: return "UNKNOWN";
            }
        }
    };
}
