#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

/**
 * @class NetworkController
 * @brief Singleton that manages WiFi operations (AP mode, STA connect, scanning).
 *
 * Responsibilities:
 *  - Start/stop a soft Access Point (AP) for provisioning.
 *  - Connect/disconnect in Station (STA) mode using stored credentials.
 *  - Provide the current IP address and perform network scans.
 *
 * Notes:
 *  - Not thread-safe; call from the main task / same context that manages network state.
 *  - Methods are idempotent where practical (stopAPMode/disconnectFromWiFi can be called repeatedly).
 *  - Use ipAddress() to determine when to start services that require a usable IP (e.g. mDNS).
 */
class NetworkController
{
public:
    // Access singleton instance
    static NetworkController &instance();

    // non-copyable, non-movable
    NetworkController(const NetworkController &) = delete;
    NetworkController &operator=(const NetworkController &) = delete;
    NetworkController(NetworkController &&) = delete;
    NetworkController &operator=(NetworkController &&) = delete;

    /**
     * @brief Start Access Point mode (used for provisioning).
     * @param name SSID to advertise for the AP (e.g. "Heater-XXXX").
     *
     * This configures the WiFi hardware into AP mode and starts a soft AP. If AP is already
     * running this call will attempt to reconfigure it to the requested name.
     */
    void startAPMode(const String &name);

    /**
     * @brief Stop the soft AP if running.
     *
     * Safe to call repeatedly. This will disconnect any connected clients and clear
     * the AP mode bit; the function leaves WiFi in WIFI_MODE_NULL.
     */
    void stopAPMode();

    /**
     * @brief Start Station (STA) mode and attempt to connect using configured credentials.
     * @return true if connection succeeded, false otherwise.
     *
     * This method reads SSID/password from the Config singleton and attempts to connect.
     * It blocks for a short, bounded timeout while waiting for association.
     */
    bool connectToWiFi();

    /**
     * @brief Disconnect from WiFi station (if connected) and clear WiFi mode.
     *
     * Safe to call repeatedly. This will disconnect the STA interface and set WiFi mode
     * to WIFI_MODE_NULL. It does not modify stored credentials unless WiFi.disconnect(true)
     * is explicitly used in the implementation.
     */
    void disconnectFromWiFi();

    /**
     * @brief Scan for available WiFi networks (blocking).
     * @return Vector of SSID strings found by the scan. Empty vector if none found or on error.
     *
     * Note: WiFi.scanNetworks() is a blocking call and may take a couple of seconds.
     * Consider calling this from a background task if you need non-blocking behavior.
     */
    std::vector<String> scanNetworks();

    /**
     * @brief Return the current IP address for the active interface.
     * @return IPAddress of the active interface (STA if connected, otherwise AP).
     *         Returns 0.0.0.0 if no IP is available.
     *
     * Prefer this over WiFi.localIP() / WiFi.softAPIP() so callers don't need to know
     * which mode the device is running in.
     */
    IPAddress ipAddress();

private:
    // Private ctor for singleton
    NetworkController();
    ~NetworkController() = default;
};