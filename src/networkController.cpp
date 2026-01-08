#include "networkController.h"

#include <WiFi.h>
#include "config.h"
#include "Logger.h"
#include "System.h"

NetworkController &NetworkController::instance()
{
    static NetworkController inst;
    return inst;
}

NetworkController::NetworkController()
{
    // start with WiFi off until explicitly requested
    WiFi.mode(WIFI_MODE_NULL);
    WiFi.disconnect(true);
}

/**
 * Start soft AP with given SSID.
 */
void NetworkController::startAPMode(const String &name)
{
    Logger::instance().info(String("Starting AP mode: ") + name);

    // Ensure WiFi is in AP mode
    WiFi.mode(WIFI_MODE_AP);

    // Start soft AP (open network). Add password or config as needed later.
    bool ok = WiFi.softAP(name.c_str());
    if (!ok)
    {
        Logger::instance().error(String("softAP failed for: ") + name);
        return;
    }

    IPAddress ip = WiFi.softAPIP();

    Logger::instance().info(String("AP started, IP=") + ip.toString());
}

/**
 * Stop soft AP if it is running.
 */
void NetworkController::stopAPMode()
{
    // Check if AP bit is set in current WiFi mode
    wifi_mode_t mode = WiFi.getMode();
    if (mode & WIFI_MODE_AP)
    {
        Logger::instance().info("Stopping AP mode");
        WiFi.softAPdisconnect(true); // disconnect clients and stop AP
        WiFi.mode(WIFI_MODE_NULL);   // turn off WiFi or caller can set desired mode later
        delay(50);                   // give stack a moment to settle
        Logger::instance().info(String("AP stopped, mode=") + String((int)WiFi.getMode()));
    }
    else
    {
        Logger::instance().debug("stopAPMode: AP mode not active");
    }
}

/**
 * Start STA mode and attempt to connect using credentials from Config.
 */
bool NetworkController::connectToWiFi()
{
    String ssid = Config::instance().getSsid();
    String pass = Config::instance().getPassword();

    if (ssid.length() == 0)
    {
        Logger::instance().warn("No SSID configured; cannot start STA mode");
        return false;
    }

    Logger::instance().info(String("Connecting to WiFi SSID=\"") + ssid + "\"");

    // Configure STA mode and attempt connection
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect(true);
    delay(100);

    if (pass.length() > 0)
        WiFi.begin(ssid.c_str(), pass.c_str());
    else
        WiFi.begin(ssid.c_str());

    const unsigned long timeoutMs = 15000; // total wait time
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs)
    {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        IPAddress ip = WiFi.localIP();
        Logger::instance().info(String("WiFi connected, IP=") + ip.toString());
        return true;
    }
    else
    {
        Logger::instance().warn("WiFi connect timed out");
        return false;
    }
}

/**
 * Disconnect from WiFi station (if connected) and clear WiFi mode.
 */
void NetworkController::disconnectFromWiFi()
{
    // If connected as STA or STA mode active, disconnect
    if (WiFi.status() == WL_CONNECTED || (WiFi.getMode() & WIFI_MODE_STA))
    {
        Logger::instance().info("Disconnecting from WiFi (STA)");
        WiFi.disconnect(true); // erase credentials if true, disconnect clients if AP
        WiFi.mode(WIFI_MODE_NULL);
        delay(50);
        Logger::instance().info(String("Disconnected, status=") + String(WiFi.status()));
    }
    else
    {
        Logger::instance().debug("disconnectFromWiFi: STA not active");
    }
}

/**
 * Scan for WiFi networks and return list of SSID strings.
 *
 * This is a blocking call (WiFi.scanNetworks()) and can take a couple of seconds.
 * Use asynchronous scan variants if you need non-blocking behaviour.
 */
std::vector<String> NetworkController::scanNetworks()
{
    Logger::instance().info("Scanning for WiFi networks...");
    int n = WiFi.scanNetworks();
    std::vector<String> ssids;
    if (n <= 0)
    {
        Logger::instance().info("No WiFi networks found");
        return ssids;
    }

    ssids.reserve((size_t)n);
    for (int i = 0; i < n; ++i)
    {
        String s = WiFi.SSID(i);
        ssids.push_back(s);

        // detailed debug log
        Logger::instance().debug(String("Found: ") + s +
                                 " RSSI=" + String(WiFi.RSSI(i)) +
                                 " CH=" + String(WiFi.channel(i)) +
                                 " ENC=" + String(WiFi.encryptionType(i)));
    }

    // free scan results
    WiFi.scanDelete();

    return ssids;
}

/**
 * Return the current IP address for the active interface.
 * Prefers STA IP when connected, otherwise returns AP IP if available.
 * Returns 0.0.0.0 if no IP is available.
 */
IPAddress NetworkController::ipAddress()
{
    // If STA is connected, prefer that IP.
    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.localIP();
    }

    // Otherwise, if AP mode is active, return softAP IP.
    IPAddress apIp = WiFi.softAPIP();
    if (apIp != IPAddress(0, 0, 0, 0))
    {
        return apIp;
    }

    // No IP available
    return IPAddress(0, 0, 0, 0);
}
