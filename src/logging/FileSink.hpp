#pragma once

#include "LogSink.hpp"

#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace calcium::logging {

/// Writes log records to a file. Thread-safe.
class FileSink final : public ILogSink {
public:
    explicit FileSink(const std::string& path)
        : m_file(path, std::ios::app)
    {}

    bool is_open() const { return m_file.is_open(); }

    void write(const LogRecord& r) override {
        if (!m_file.is_open()) return;
        std::lock_guard lock{m_mutex};
        m_file << '[' << format_ts(r.timestamp) << ']'
               << '[' << to_string(r.level)     << ']'
               << '[' << r.tag                  << "] "
               << r.message << '\n';
    }

    void flush() override {
        std::lock_guard lock{m_mutex};
        m_file.flush();
    }

private:
    static std::string format_ts(const std::chrono::system_clock::time_point& tp) {
        using namespace std::chrono;
        auto tt = system_clock::to_time_t(tp);
        auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
        std::tm buf{};
#if defined(_WIN32)
        localtime_s(&buf, &tt);
#else
        localtime_r(&tt, &buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setw(3) << std::setfill('0') << ms.count();
        return oss.str();
    }

    std::ofstream m_file;
    std::mutex    m_mutex;
};

} // namespace calcium::logging
