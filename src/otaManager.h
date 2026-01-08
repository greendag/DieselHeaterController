#pragma once

#include <Arduino.h>

/**
 * @file otaManager.h
 * @brief Singleton that encapsulates Arduino OTA lifecycle and deferred startup.
 *
 * OtaManager centralizes handling of ArduinoOTA initialization and periodic handling.
 * - Supports deferred start when network is not yet available (common on boot).
 * - Keeps the application from scattering `ArduinoOTA.begin()` / `ArduinoOTA.handle()`
 *   calls across multiple places.
 *
 * Usage:
 *  - Call `OtaManager::instance().begin(true)` once at startup (or after network is up).
 *  - Call `OtaManager::instance().loop()` from the main loop() to process OTA events
 *    and to attempt deferred startup when an IP becomes available.
 *  - Call `OtaManager::instance().stop()` to stop processing OTA events (no formal
 *    ArduinoOTA::end() exists on all cores; stop means ceasing to call handle()).
 *
 * Notes:
 *  - This class only implements ArduinoOTA-based updates (IDE/PlatformIO OTA). It
 *    intentionally omits HTTP upload endpoints (those can be added later).
 *  - The manager will, if possible, set the OTA hostname from `Config::instance().getDeviceName()`.
 */
class OtaManager
{
public:
    /**
     * @brief Get the singleton instance.
     */
    static OtaManager &instance();

    /**
     * @brief Begin OTA manager operation.
     *
     * If `enableArduinoOta` is true and the device already has a valid IP address,
     * the ArduinoOTA subsystem is configured and started immediately. If no IP is
     * available yet, the start is deferred and attempted from `loop()`. This prevents
     * attempting ArduinoOTA before the network stack is ready.
     *
     * @param enableArduinoOta Enable ArduinoOTA (mDNS/IDE OTA). Default true.
     */
    void begin(bool enableArduinoOta = true);

    /**
     * @brief Per-loop processing.
     *
     * Call from the application's `loop()` to:
     *  - attempt deferred startup (if IP was not present at `begin()` time),
     *  - call `ArduinoOTA.handle()` when the manager is running.
     */
    void loop();

    /**
     * @brief Stop OTA processing.
     *
     * This stops calling `ArduinoOTA.handle()` and clears internal running flags.
     * There is no standardized `ArduinoOTA.end()` across all cores, so stop only
     * prevents further handling.
     */
    void stop();

    /**
     * @brief Return whether OTA handling is active (i.e., `ArduinoOTA.handle()` is called).
     */
    bool isRunning() const;

private:
    OtaManager();
    ~OtaManager();
    OtaManager(const OtaManager &) = delete;
    OtaManager &operator=(const OtaManager &) = delete;

    // Internal helpers to start/stop ArduinoOTA behavior.
    void startArduinoOta();
    void stopArduinoOta();

    // Configuration flags/state
    bool arduinoOtaEnabled_;           ///< true if ArduinoOTA was requested
    bool running_;                     ///< true if ArduinoOTA is currently being handled
    bool pendingStart_;                ///< true if begin() deferred start due to no IP
    unsigned long lastStartAttemptMs_; ///< throttle deferred start attempts
};
