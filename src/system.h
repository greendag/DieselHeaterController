#pragma once

#include <Arduino.h>
#include <cstdint>
#include "esp_system.h"

/**
 * @file System.h
 * @brief Provides uptime since initialization and reset reason information.
 *
 * Usage:
 *   System::instance().init();                // call early in setup()
 *   uint64_t ms = System::instance().getUptime();
 *   String reason = System::instance().resetReason();
 *
 * Notes:
 *  - getUptime() returns ms since init() (or since first construction) and handles
 *    millis() wrap-around. It does not survive deep-sleep resets.
 *  - resetReason() reports the ESP32 SDK reset reason at boot.
 */
class System
{
public:
    // Singleton access
    static System &instance();

    // Optionally call in setup() to mark start time (otherwise start is taken at construction)
    void init();

    // Uptime in milliseconds since init/start
    uint64_t getUptime() const;

    // Return raw esp reset reason enum
    esp_reset_reason_t resetReasonCode() const;

    // Human-readable reset reason
    String resetReason() const;

    // Reboot the device after an optional delay in milliseconds.
    // This calls ESP.restart() after the delay. Default is immediate reboot.
    void reboot(uint32_t delayMs = 0);

private:
    System();
    ~System() = default;

    // non-copyable, non-movable
    System(const System &) = delete;
    System &operator=(const System &) = delete;
    System(System &&) = delete;
    System &operator=(System &&) = delete;

    // snapshot of millis() at start
    volatile unsigned long startMillis_;

    // helper to map enum -> string
    static const char *resetReasonToString(esp_reset_reason_t r);
};
