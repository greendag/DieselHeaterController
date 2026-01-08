#include "Logger.h"
#include "System.h"

#include <stdio.h> // for snprintf

static void formatTimestamp(char *buf, size_t bufsize)
{
    uint64_t ms = System::instance().getUptime();

    uint64_t total_seconds = ms / 1000ULL;
    uint64_t hours = total_seconds / 3600ULL;
    uint64_t minutes = (total_seconds / 60ULL) % 60ULL;
    uint64_t seconds = total_seconds % 60ULL;
    uint64_t msec = ms % 1000ULL;

    // Format: HH:MM:SS.mmm (hours may grow beyond 24)
    snprintf(buf, bufsize, "%02llu:%02llu:%02llu.%03llu",
             (unsigned long long)hours,
             (unsigned long long)minutes,
             (unsigned long long)seconds,
             (unsigned long long)msec);
}

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::Logger()
    : level_(LogLevel::Info), initialized_(false)
{
}

void Logger::init(unsigned long baud)
{
    if (initialized_)
        return;

    // Initialize System early if desired (not required if user calls System::init elsewhere)
    System::instance().init();

    Serial.begin(baud);
    // brief wait for Serial on boards that support it
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 2000)
    {
        delay(5);
    }
    initialized_ = true;
    info(String("Logger initialized at ") + String(baud) + " baud");
}

void Logger::setLevel(LogLevel level)
{
    level_ = level;
    info(String("Log level set to ") + levelToString(level));
}

Logger::LogLevel Logger::getLevel() const
{
    return level_;
}

void Logger::log(LogLevel level, const String &msg)
{
    if (level < level_ || level_ == LogLevel::Off)
        return;

    const char *prefix = levelToString(level);

    char ts[24];
    formatTimestamp(ts, sizeof(ts));

    // print even if not initialized
    Serial.print(ts);
    Serial.print(" [");
    Serial.print(prefix);
    Serial.print("] ");
    Serial.println(msg);
}

void Logger::debug(const String &msg) { log(LogLevel::Debug, msg); }
void Logger::info(const String &msg) { log(LogLevel::Info, msg); }
void Logger::warn(const String &msg) { log(LogLevel::Warn, msg); }
void Logger::error(const String &msg) { log(LogLevel::Error, msg); }

Logger::LogLevel Logger::levelFromString(const String &s)
{
    String t = s;
    t.toLowerCase();
    if (t == "debug")
        return LogLevel::Debug;
    if (t == "info")
        return LogLevel::Info;
    if (t == "warn" || t == "warning")
        return LogLevel::Warn;
    if (t == "error")
        return LogLevel::Error;
    return LogLevel::Off;
}

const char *Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    default:
        return "OFF";
    }
}
