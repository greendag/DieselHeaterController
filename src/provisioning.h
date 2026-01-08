#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include "fileSystem.h"
#include "config.h"

class Provisioning
{
public:
    static Provisioning &instance();
    bool start();
    void provisioningLoop();
    void stop();
    bool isProvisioned() const;
    void provision(const String &ssid, const String &password, const String &deviceName);
    void reset();
    void checkFactoryResetButton();

private:
    Provisioning();
    ~Provisioning();

    String macSuffixHex() const;

    uint32_t configCbId_;

    DNSServer dnsServer_;
    static constexpr uint16_t DNS_PORT = 53;

    // Factory reset state tracking
    bool buttonPressed_;
    uint32_t buttonPressStartMs_;
    static constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
    static constexpr int BOOT_BUTTON_PIN = 0;

    // Reboot scheduling after successful POST /save
    bool pendingReboot_ = false;
    uint32_t rebootAtMs_ = 0;
};