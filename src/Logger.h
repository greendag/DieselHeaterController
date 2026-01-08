#pragma once

#include <Arduino.h>

/**
 * @file Logger.h
 * @brief Simple singleton logger for serial output with configurable level.
 *
 * Usage:
 *  Logger::instance().init(115200);
 *  Logger::instance().info("Started");
 *  Logger::instance().setLevel(Logger::LogLevel::Debug);
 *
 * Thread-safety: minimal; safe for typical Arduino use.
 */
class Logger
{
public:
    enum class LogLevel : uint8_t
    {
        Debug = 0,
        Info,
        Warn,
        Error,
        Off
    };

    // Singleton access
    static Logger &instance();

    // Initialize serial port (does Serial.begin). Safe to call multiple times.
    // Also marks the "start" time for timestamps (typically called in setup()).
    void init(unsigned long baud = 115200);

    // Logging API
    void log(LogLevel level, const String &msg);
    void debug(const String &msg);
    void info(const String &msg);
    void warn(const String &msg);
    void error(const String &msg);

    // Configure level (default is Info)
    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    // Convenience: parse level from string ("debug","info",...)
    static LogLevel levelFromString(const String &s);
    static const char *levelToString(LogLevel level);

private:
    Logger();
    ~Logger() = default;

    // non-copyable/movable
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    volatile LogLevel level_;
    volatile bool initialized_;

    // Note: Logger no longer keeps its own startMillis_; it uses System::getUptime()
    // for timestamps so uptime is consistent across the project.
};
