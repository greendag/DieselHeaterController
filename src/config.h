#pragma once
/**
 * @file config.h
 * @brief Singleton configuration object backed by a JSON file on LittleFS.
 *
 * Behavior:
 *  - Single instance accessible via Config::instance().
 *  - Loads its content from disk on construction (first access).
 *  - Registers a FileSystem event callback and reloads if the backing file is CREATED/UPDATED/REMOVED.
 *  - Public getters / setters; setters mark state dirty and are debounced before writing to flash.
 *
 * Thread-safety: Basic critical-section protection is provided for setters/load/persist.
 *              Getters return copies to avoid returning references to data that may change.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "fileSystem.h"

// I2C pins for display (can be adjusted per board)
constexpr uint8_t DISPLAY_SDA = 21;
constexpr uint8_t DISPLAY_SCL = 4;

class Config
{
public:
    static const String DEFAULT_DEVICE_NAME;
    static constexpr const char *CONFIG_PATH = "/config.json";

    // Singleton access
    static Config &instance();

    // Getters (return copies)
    String getSsid() const;
    String getPassword() const;
    String getDeviceName() const;

    // Setters (mark dirty, persist is debounced)
    void setSsid(const String &s);
    void setPassword(const String &p);
    void setDeviceName(const String &d);

    // Polling: call periodically from main loop to flush debounced changes
    void poll();

    // Force flush pending changes immediately
    bool forcePersist();

    // Debug print
    void print() const;

private:
    // Private ctor/dtor for singleton
    Config();
    ~Config();

    // non-copyable, non-movable
    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;
    Config(Config &&) = delete;
    Config &operator=(Config &&) = delete;

    // Backing fields (private)
    String ssid_;
    String password_;
    String deviceName_;

    // FileSystem callback management
    uint32_t fileCbId_;
    volatile bool suppressReload_; // set true while writing to avoid reacting to our own write

    // Debounce state
    volatile bool dirty_;
    volatile unsigned long lastChangeMs_;
    static constexpr unsigned long DEBOUNCE_MS = 2000; // milliseconds

    // Load from file (internal)
    void loadFromDisk();

    // Persist current in-memory config to disk (internal)
    bool persist();

    // Serialize/deserialize helpers (internal)
    String serializeToJson() const;
    bool parseFromJson(const String &json);

    // FileSystem event callback
    void onFileEvent(const String &path, FileAction action);
};
