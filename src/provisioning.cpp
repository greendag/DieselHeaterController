#include "provisioning.h"

#include <DNSServer.h>
#include "Logger.h"
#include "networkController.h"
#include "System.h"
#include "esp_system.h"
#include "multicaseDns.h"
#include "ws.h"
#include "onBoardLed.h"
#include "displayManager.h"

constexpr uint16_t Provisioning::DNS_PORT;
constexpr uint32_t Provisioning::FACTORY_RESET_HOLD_MS;
constexpr int Provisioning::BOOT_BUTTON_PIN;

Provisioning &Provisioning::instance()
{
    static Provisioning inst;
    return inst;
}

Provisioning::Provisioning()
    : configCbId_(0), buttonPressed_(false), buttonPressStartMs_(0)
{
    // Configure boot button with internal pullup
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // Register for config file changes so we can log and react if needed.
    configCbId_ = FileSystem::instance().addFileEventCallback(
        [this](const String &path, FileAction action)
        {
            if (path != Config::CONFIG_PATH)
                return;
            if (action == FileAction::CREATED || action == FileAction::UPDATED || action == FileAction::REMOVED)
            {
                Logger::instance().debug(String("Provisioning: config changed, provisioned=") +
                                         (isProvisioned() ? "true" : "false"));
            }
        });

    if (configCbId_ == 0)
    {
        Logger::instance().warn("Provisioning: failed to register config file callback");
    }
    else
    {
        Logger::instance().debug(String("Provisioning: registered config callback id=") + String(configCbId_));
    }
}

Provisioning::~Provisioning()
{
    if (configCbId_ != 0)
        FileSystem::instance().removeFileEventCallback(configCbId_);
}

bool Provisioning::start()
{
    // If already provisioned, skip AP provisioning flow
    if (isProvisioned())
    {
        Logger::instance().info("Provisioning: device already provisioned; start() skipped");
        return false;
    }

    String apName = String("Heater-") + macSuffixHex();
    Logger::instance().info(String("Provisioning: starting AP '") + apName + "'");

    // Start AP
    NetworkController::instance().startAPMode(apName);

    IPAddress ip = NetworkController::instance().ipAddress();
    if (ip == IPAddress(0, 0, 0, 0))
    {
        Logger::instance().error("Provisioning: failed to start soft AP (no IP reported)");
        return false;
    }

    // mDNS + DNS-SD
    MulticaseDns::instance().begin(apName);
    MulticaseDns::instance().addService("http", "tcp", 80);
    MulticaseDns::instance().addServiceTxt("http", "tcp", "path", "/index.html");

    // Captive DNS
    dnsServer_.start(DNS_PORT, "*", ip);

    // Web server
    Ws::instance().begin(80);

    // Captive-portal probes: return quickly to avoid error noise
    Ws::instance().onRaw("/connecttest.txt", HTTP_GET, [](WebServer &srv)
                         { srv.send(200, "text/plain", "OK"); });
    Ws::instance().onRaw("/generate_204", HTTP_GET, [](WebServer &srv)
                         { srv.send(204, "text/plain", ""); });
    Ws::instance().onRaw("/hotspot-detect.html", HTTP_GET, [](WebServer &srv)
                         { srv.send(200, "text/html", "<html><body>OK</body></html>"); });

    // Static content
    Ws::instance().serveStatic("/", "/provisioning/index.html");
    Ws::instance().serveStatic("/*", "/provisioning/*");

    // Handle provisioning POST; send response, then tear down after
    Ws::instance().onRaw("/save", HTTP_POST, [this](WebServer &srv)
                         {
                             String ssid = srv.arg("ssid");
                             String password = srv.arg("password");
                             String deviceName = srv.arg("deviceName");

                             if (ssid.length() == 0)
                             {
                                 srv.send(400, "text/plain", "Missing ssid");
                                 return;
                             }

                             this->provision(ssid, password, deviceName);
                             srv.send(200, "text/plain", "Saved. Rebooting...");

                             // Defer teardown to avoid tearing down server mid-response.
                             // Set a flag and handle in provisioningLoop().
                             pendingReboot_ = true;
                             rebootAtMs_ = millis() + 500; // 0.5s delay
                         });

    Logger::instance().info(String("Provisioning: AP running, IP=") + ip.toString());

    // Show AP name and the provisioning URL on the display if available.
    String url = String("http://") + ip.toString();
    DisplayManager::instance().showStatus(apName, url);
    return true;
}

void Provisioning::provision(const String &ssid, const String &password, const String &deviceName)
{
    if (ssid.length() == 0)
    {
        Logger::instance().warn("Provisioning: empty SSID provided - aborting provision");
        return;
    }

    Logger::instance().info(String("Provisioning: saving credentials for SSID='") + ssid + "'");
    Config::instance().setSsid(ssid);
    Config::instance().setPassword(password);
    Config::instance().setDeviceName(deviceName);

    bool ok = Config::instance().forcePersist();
    if (!ok)
    {
        Logger::instance().error("Provisioning: failed to persist configuration");
        return;
    }
}

void Provisioning::provisioningLoop()
{
    // Always monitor factory reset button
    checkFactoryResetButton();

    // Process captive DNS only while not provisioned
    if (!isProvisioned())
    {
        dnsServer_.processNextRequest();
    }

    // If a reboot was requested, perform it after delay
    if (pendingReboot_ && millis() >= rebootAtMs_)
    {
        pendingReboot_ = false;
        stop();
        System::instance().reboot();
    }
}

void Provisioning::stop()
{
    Logger::instance().info("Provisioning: stopping");

    // Stop services first
    Ws::instance().stop();
    dnsServer_.stop();
    MulticaseDns::instance().stop();

    // Tear down AP, then switch to STA (avoid WIFI_MODE_NULL which can trigger netstack issues)
    NetworkController::instance().stopAPMode();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    delay(20);

    IPAddress ip = NetworkController::instance().ipAddress();
    if (ip == IPAddress(0, 0, 0, 0))
    {
        Logger::instance().debug("Provisioning: soft AP stopped");
    }
    else
    {
        Logger::instance().warn(String("Provisioning: soft AP still present, IP=") + ip.toString());
    }
}

bool Provisioning::isProvisioned() const
{
    return Config::instance().getSsid().length() > 0;
}

void Provisioning::reset()
{
    Logger::instance().warn("Provisioning: resetting configuration to factory defaults (clearing wifi credentials)");

    // Flash onboard LED white twice
    OnBoardLed::instance().setHexColor("#FFFFFF");
    delay(300);
    OnBoardLed::instance().setHexColor("#000000");
    delay(300);
    OnBoardLed::instance().setHexColor("#FFFFFF");
    delay(300);
    OnBoardLed::instance().setHexColor("#000000");
    delay(300);

    Config::instance().setSsid(String());
    Config::instance().setPassword(String());
    Config::instance().setDeviceName(Config::DEFAULT_DEVICE_NAME);
    Config::instance().forcePersist();

    System::instance().reboot();
}

void Provisioning::checkFactoryResetButton()
{
    bool currentState = (digitalRead(BOOT_BUTTON_PIN) == LOW); // button pressed = LOW with pullup

    if (currentState && !buttonPressed_)
    {
        // Button just pressed
        buttonPressed_ = true;
        buttonPressStartMs_ = millis();
        Logger::instance().debug("Provisioning: boot button pressed, hold for factory reset");
    }
    else if (!currentState && buttonPressed_)
    {
        // Button released before 10s
        buttonPressed_ = false;
        Logger::instance().debug("Provisioning: boot button released");
    }
    else if (currentState && buttonPressed_)
    {
        // Button still held - check if 10s elapsed
        uint32_t elapsed = millis() - buttonPressStartMs_;
        if (elapsed >= FACTORY_RESET_HOLD_MS)
        {
            Logger::instance().warn("Provisioning: factory reset triggered");
            buttonPressed_ = false; // prevent re-trigger
            reset();
        }
    }
}

String Provisioning::macSuffixHex() const
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[5];
    snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
    return String(buf);
}