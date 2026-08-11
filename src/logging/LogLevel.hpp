#pragma once

#include <string_view>

namespace calcium::logging {

/// Log severity levels, ordered from most to least verbose.
enum class LogLevel : int {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Off     = 4,   ///< Used to disable all logging.
};

/// Human-readable label for a log level.
inline constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Off:     return "OFF  ";
    }
    return "?????";
}

/// Parse a string into a LogLevel. Returns LogLevel::Info on unknown input.
inline LogLevel parse_log_level(std::string_view s) noexcept {
    if (s == "debug"   || s == "DEBUG")   return LogLevel::Debug;
    if (s == "info"    || s == "INFO")    return LogLevel::Info;
    if (s == "warning" || s == "WARNING"
     || s == "warn"    || s == "WARN")    return LogLevel::Warning;
    if (s == "error"   || s == "ERROR")   return LogLevel::Error;
    if (s == "off"     || s == "OFF")     return LogLevel::Off;
    return LogLevel::Info;
}

} // namespace calcium::logging
