#pragma once

#include "LogLevel.hpp"
#include <string>
#include <chrono>

namespace calcium::logging {

/// A single log record passed to every sink.
struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel  level;
    std::string tag;      ///< Source subsystem, e.g. "Repository"
    std::string message;
};

/// Abstract base for log output destinations.
class ILogSink {
public:
    virtual ~ILogSink() = default;

    /// Receive and handle a log record. Must be thread-safe.
    virtual void write(const LogRecord& record) = 0;

    /// Flush any buffered output.
    virtual void flush() {}
};

} // namespace calcium::logging
