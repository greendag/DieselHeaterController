#include "config.h"

#include <LittleFS.h>

const String Config::DEFAULT_DEVICE_NAME = String("DieselHeaterController");

/*
 * Simple scoped critical section for basic thread/ISR protection.
 * Uses Arduino noInterrupts()/interrupts() which provides a small window
 * of protection on most Arduino platforms. This is intentionally minimal;
 * adapt to FreeRTOS / RTOS primitives if your environment needs task-level locking.
 */
class ScopedCritical
{
public:
    ScopedCritical() { noInterrupts(); }
    ~ScopedCritical() { interrupts(); }
};

Config &Config::instance()
{
    static Config inst;
    return inst;
}

Config::Config()
    : ssid_(), password_(), deviceName_(DEFAULT_DEVICE_NAME),
      fileCbId_(0), suppressReload_(false),
      dirty_(false), lastChangeMs_(0)
{
    loadFromDisk();

    // Register callback to keep in-memory config in sync with file changes.
    fileCbId_ = FileSystem::instance().addFileEventCallback(
        [this](const String &path, FileAction action)
        { this->onFileEvent(path, action); });
}

Config::~Config()
{
    if (fileCbId_ != 0)
        FileSystem::instance().removeFileEventCallback(fileCbId_);
}

/* Getters return copies to avoid returning references to data that may change. */
String Config::getSsid() const
{
    ScopedCritical lock;
    return ssid_;
}
String Config::getPassword() const
{
    ScopedCritical lock;
    return password_;
}
String Config::getDeviceName() const
{
    ScopedCritical lock;
    return deviceName_;
}

/* Setters mark dirty and update timestamp. Persist is debounced via poll(). */
void Config::setSsid(const String &s)
{
    {
        ScopedCritical lock;
        ssid_ = s;
        dirty_ = true;
        lastChangeMs_ = millis();
    }
}

void Config::setPassword(const String &p)
{
    {
        ScopedCritical lock;
        password_ = p;
        dirty_ = true;
        lastChangeMs_ = millis();
    }
}

void Config::setDeviceName(const String &d)
{
    {
        ScopedCritical lock;
        deviceName_ = d;
        dirty_ = true;
        lastChangeMs_ = millis();
    }
}

/**
 * Periodic poll to flush debounced changes.
 * Call this from your main loop at least once per DEBOUNCE_MS interval.
 */
void Config::poll()
{
    // quick snapshot to minimize time in critical section
    bool shouldPersist = false;
    {
        ScopedCritical lock;
        if (dirty_)
        {
            unsigned long now = millis();
            unsigned long elapsed = now - lastChangeMs_;
            if (elapsed >= DEBOUNCE_MS)
                shouldPersist = true;
        }
    }

    if (shouldPersist)
    {
        // persist will clear dirty_ on success
        bool ok = persist();
        if (!ok)
        {
            // leave dirty_ true so retry on next poll
        }
    }
}

/**
 * Force immediate persist of pending changes.
 * Returns true on success or if nothing was pending.
 */
bool Config::forcePersist()
{
    bool hadDirty = false;
    {
        ScopedCritical lock;
        hadDirty = dirty_;
    }
    if (!hadDirty)
        return true;
    return persist();
}

/**
 * Load config from disk into memory. Protected by critical section.
 */
void Config::loadFromDisk()
{
    FileSystem &fs = FileSystem::instance();
    if (!fs.exists(CONFIG_PATH))
    {
        // No backing file; keep defaults
        return;
    }

    String json = fs.read(CONFIG_PATH);
    if (json.length() == 0)
    {
        // empty file or read error -> treat as no config
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
    {
        // parse failed -- leave existing values
        return;
    }

    // Update fields inside critical section
    {
        ScopedCritical lock;
        if (doc.containsKey("ssid"))
            ssid_ = String(doc["ssid"].as<const char *>());
        if (doc.containsKey("password"))
            password_ = String(doc["password"].as<const char *>());
        if (doc.containsKey("deviceName"))
            deviceName_ = String(doc["deviceName"].as<const char *>());

        // loaded from disk means no pending local changes
        dirty_ = false;
        lastChangeMs_ = 0;
    }
}

/**
 * Persist current config to disk using an atomic replace:
 *  - write JSON to a temp file (CONFIG_PATH + ".tmp")
 *  - close, then remove existing CONFIG_PATH
 *  - rename temp -> CONFIG_PATH
 *
 * suppressReload_ is set during the operation so the class does not react
 * to its own writes via FileSystem callbacks.
 *
 * On success clears dirty_.
 */
bool Config::persist()
{
    // Acquire a short critical section to snapshot current data and set suppress flag.
    String json;
    {
        ScopedCritical lock;
        json = serializeToJson();
        suppressReload_ = true;
    }

    // Ensure filesystem is mounted via FileSystem helper.
    FileSystem::instance().mount();

    const String tmpPath = String(CONFIG_PATH) + ".tmp";

    // Write temp file directly via LittleFS for control over rename.
    File tmp = LittleFS.open(tmpPath.c_str(), "w");
    if (!tmp)
    {
        ScopedCritical lock;
        suppressReload_ = false;
        return false;
    }

    size_t len = json.length();
    size_t written = 0;
    if (len > 0)
        written = tmp.write((const uint8_t *)json.c_str(), len);
    tmp.close();

    if (written != len)
    {
        // cleanup
        LittleFS.remove(tmpPath.c_str());
        ScopedCritical lock;
        suppressReload_ = false;
        return false;
    }

    // Remove existing file (if any) and rename temp to target.
    if (LittleFS.exists(CONFIG_PATH))
    {
        LittleFS.remove(CONFIG_PATH);
    }

    bool renamed = LittleFS.rename(tmpPath.c_str(), CONFIG_PATH);

    // allow reloads again
    {
        ScopedCritical lock;
        suppressReload_ = false;
        if (renamed)
        {
            dirty_ = false;
            lastChangeMs_ = 0;
        }
    }

    // Note: using LittleFS directly bypasses FileSystem's event notifications.
    return renamed;
}

/**
 * Serialize current fields to JSON.
 */
String Config::serializeToJson() const
{
    StaticJsonDocument<512> doc;
    doc["ssid"] = ssid_;
    doc["password"] = password_;
    doc["deviceName"] = deviceName_;

    String out;
    serializeJson(doc, out);
    return out;
}

/**
 * Parse JSON and update fields. Returns true on success.
 */
bool Config::parseFromJson(const String &json)
{
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
        return false;

    ScopedCritical lock;
    if (doc.containsKey("ssid"))
        ssid_ = String(doc["ssid"].as<const char *>());
    if (doc.containsKey("password"))
        password_ = String(doc["password"].as<const char *>());
    if (doc.containsKey("deviceName"))
        deviceName_ = String(doc["deviceName"].as<const char *>());
    return true;
}

/**
 * File system event handler. Reloads config when the backing file changes.
 */
void Config::onFileEvent(const String &path, FileAction action)
{
    if (path != CONFIG_PATH)
        return;

    // If we are in the middle of persisting, ignore events caused by our own write
    if (suppressReload_)
        return;

    if (action == FileAction::CREATED || action == FileAction::UPDATED)
    {
        loadFromDisk();
    }
    else if (action == FileAction::REMOVED)
    {
        ScopedCritical lock;
        ssid_.clear();
        password_.clear();
        deviceName_ = DEFAULT_DEVICE_NAME;

        // removal is an external change, ensure we clear pending local dirty state
        dirty_ = false;
        lastChangeMs_ = 0;
    }
}

void Config::print() const
{
    ScopedCritical lock;
    Serial.print(F("Config: ssid="));
    Serial.print(ssid_);
    Serial.print(F(", password="));
    Serial.print(password_);
    Serial.print(F(", deviceName="));
    Serial.println(deviceName_);
}