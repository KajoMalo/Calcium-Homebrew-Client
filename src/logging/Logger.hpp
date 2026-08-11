#pragma once

#include "LogLevel.hpp"
#include "LogSink.hpp"

#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <string_view>
#include <sstream>

namespace calcium::logging {

/// Thread-safe, multi-sink logger.
///
/// Usage:
///   auto& log = Logger::instance();
///   log.set_level(LogLevel::Debug);
///   log.add_sink(std::make_shared<ConsoleSink>());
///   log.info("Repository", "Loaded {} apps", count);
class Logger {
public:
    /// Singleton accessor. The logger is created on first use.
    static Logger& instance();

    // Non-copyable, non-movable singleton.
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    // ── Configuration ─────────────────────────────────────────────────────
    void set_level(LogLevel level);
    LogLevel level() const;

    void add_sink(std::shared_ptr<ILogSink> sink);
    void clear_sinks();

    // ── Logging methods ───────────────────────────────────────────────────
    void log(LogLevel level, std::string_view tag, std::string message);

    void debug  (std::string_view tag, std::string message) { log(LogLevel::Debug,   tag, std::move(message)); }
    void info   (std::string_view tag, std::string message) { log(LogLevel::Info,    tag, std::move(message)); }
    void warning(std::string_view tag, std::string message) { log(LogLevel::Warning, tag, std::move(message)); }
    void error  (std::string_view tag, std::string message) { log(LogLevel::Error,   tag, std::move(message)); }

private:
    Logger();

    LogLevel                              m_level{LogLevel::Info};
    std::vector<std::shared_ptr<ILogSink>> m_sinks;
    mutable std::mutex                    m_mutex;
};

// ─── Convenience macros ───────────────────────────────────────────────────────
// These compile away entirely when the level is below the configured minimum.

#define CALCIUM_LOG_DEBUG(tag, msg)   ::calcium::logging::Logger::instance().debug  (tag, msg)
#define CALCIUM_LOG_INFO(tag, msg)    ::calcium::logging::Logger::instance().info   (tag, msg)
#define CALCIUM_LOG_WARNING(tag, msg) ::calcium::logging::Logger::instance().warning(tag, msg)
#define CALCIUM_LOG_ERROR(tag, msg)   ::calcium::logging::Logger::instance().error  (tag, msg)

} // namespace calcium::logging
