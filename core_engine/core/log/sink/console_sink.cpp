#include "core/log/sink/console_sink.h"

#include <iostream>

#include "core/log/logger.h"

namespace CoreEngine {
    void ConsoleSink::Write(const LogMessage &message) {
        // std::ostream &stream =
        //         (message.metadata.level == LogLevel::Error || message.metadata.level == LogLevel::Fatal)
        //             ? std::cerr
        //             : std::cout;

        std::ostream &stream = std::cout;

        stream
                << GetColorCode(message.metadata.level)
                << "["
                << ToString(message.metadata.level)
                << "]"
                << "\x1b[0m"
                << " "
                << "["
                << message.metadata.category
                << "] "
                << "[thread:"
                << message.metadata.threadId
                << "] "
                << message.text
                << std::endl;
    }
}
