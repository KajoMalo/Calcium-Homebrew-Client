#include "Logger.hpp"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace calcium::logging {

// ─── Built-in console sink ────────────────────────────────────────────────────

namespace {

/// Formats a timestamp as "YYYY-MM-DD HH:MM:SS.mmm".
std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    using namespace std::chrono;
    auto tt  = system_clock::to_time_t(tp);
    auto ms  = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

class ConsoleSink final : public ILogSink {
public:
    void write(const LogRecord& r) override {
        // Route warnings and errors to stderr, the rest to stdout.
        std::ostream& out = (r.level >= LogLevel::Warning) ? std::cerr : std::cout;
        out << '[' << format_timestamp(r.timestamp) << ']'
            << '[' << to_string(r.level)            << ']'
            << '[' << r.tag                         << "] "
            << r.message << '\n';
    }

    void flush() override {
        std::cout.flush();
        std::cerr.flush();
    }
};

} // anonymous namespace

// ─── Logger singleton ─────────────────────────────────────────────────────────

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() {
    // Always start with a console sink so messages are visible immediately.
    m_sinks.push_back(std::make_shared<ConsoleSink>());
}

void Logger::set_level(LogLevel level) {
    std::lock_guard lock{m_mutex};
    m_level = level;
}

LogLevel Logger::level() const {
    std::lock_guard lock{m_mutex};
    return m_level;
}

void Logger::add_sink(std::shared_ptr<ILogSink> sink) {
    if (!sink) return;
    std::lock_guard lock{m_mutex};
    m_sinks.push_back(std::move(sink));
}

void Logger::clear_sinks() {
    std::lock_guard lock{m_mutex};
    m_sinks.clear();
}

void Logger::log(LogLevel level, std::string_view tag, std::string message) {
    std::lock_guard lock{m_mutex};

    if (level < m_level) return;

    LogRecord record{
        std::chrono::system_clock::now(),
        level,
        std::string(tag),
        std::move(message)
    };

    for (auto& sink : m_sinks) {
        sink->write(record);
    }
}

} // namespace calcium::logging
