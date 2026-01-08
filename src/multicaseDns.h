#pragma once
/**
 * @file multicaseDns.h
 * @brief Singleton wrapper for mDNS / DNS‑SD on ESP32 (ESPmDNS).
 *
 * Minimal wrapper to:
 *  - start/stop the mDNS responder with a requested hostname,
 *  - advertise service types (SRV/PTR),
 *  - attach TXT key/value metadata.
 *
 * Notes:
 *  - Uses NetworkController::ipAddress() to ensure a usable IP exists before starting.
 *  - The mDNS stack may auto-rename hostnames on conflict; hostname() returns the
 *    name claimed by the mDNS implementation when available.
 */

#include <Arduino.h>

class MulticaseDns
{
public:
    static MulticaseDns &instance();

    // Start mDNS with requested hostname (no ".local"). Returns true on success.
    bool begin(const String &hostname);

    // Stop mDNS responder. Safe to call repeatedly.
    void stop();

    // Announce a service (e.g. "http","tcp",80). Returns true when parameters valid and running.
    bool addService(const String &service, const String &proto, uint16_t port);

    // Add a TXT key/value for a previously added service.
    bool addServiceTxt(const String &service, const String &proto, const String &key, const String &value);

    // True if the wrapper believes mDNS is running.
    bool isRunning() const;

    // Return the hostname claimed by the mDNS responder (no ".local"), or empty if unknown.
    String hostname() const;

private:
    MulticaseDns();
    ~MulticaseDns();

    bool running_;
};