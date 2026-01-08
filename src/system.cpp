#include "System.h"
#include "Logger.h"
#include "esp_system.h"
#include <limits.h>

System &System::instance()
{
    static System inst;
    return inst;
}

System::System()
    : startMillis_(millis())
{
}

void System::init()
{
    startMillis_ = millis();
}

uint64_t System::getUptime() const
{
    unsigned long now = millis();
    unsigned long start = startMillis_;

    // compute difference handling 32-bit wrap-around
    uint64_t diff;
    if (now >= start)
    {
        diff = (uint64_t)(now - start);
    }
    else
    {
        // wrap occurred: (UINT32_MAX - start + 1) + now
        diff = (uint64_t)((uint64_t)UINT32_MAX - (uint64_t)start + 1ULL + (uint64_t)now);
    }
    return diff;
}

esp_reset_reason_t System::resetReasonCode() const
{
    return esp_reset_reason();
}

String System::resetReason() const
{
    const char *s = resetReasonToString(esp_reset_reason());
    return String(s);
}

const char *System::resetReasonToString(esp_reset_reason_t r)
{
    switch (r)
    {
    case ESP_RST_UNKNOWN:
        return "Unknown reset reason";
    case ESP_RST_POWERON:
        return "Power-on reset";
    case ESP_RST_EXT:
        return "External reset (RESET pin)";
    case ESP_RST_SW:
        return "Software reset";
    case ESP_RST_PANIC:
        return "Exception/panic (software crash)";
    case ESP_RST_INT_WDT:
        return "Interrupt watchdog reset";
    case ESP_RST_TASK_WDT:
        return "Task watchdog reset";
    case ESP_RST_WDT:
        return "Other watchdog reset";
    case ESP_RST_DEEPSLEEP:
        return "Wake from deep sleep (deep-sleep reset)";
    case ESP_RST_BROWNOUT:
        return "Brownout reset (power instability)";
    case ESP_RST_SDIO:
        return "SDIO reset";
    default:
        return "Unknown reset reason";
    }
}

void System::reboot(uint32_t delayMs)
{
    Logger::instance().info(String("System reboot requested, delaying ") + String(delayMs) + " ms");
    if (delayMs > 0)
        delay(delayMs);
    Logger::instance().info("System restarting now");
    ESP.restart();
}
