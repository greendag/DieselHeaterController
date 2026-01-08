#include "otaManager.h"

#include <ArduinoOTA.h>
#include "Logger.h"
#include "networkController.h"
#include "System.h"
#include "config.h"

/*
 * otaManager.cpp
 *
 * Implementation notes:
 * - startArduinoOta() configures ArduinoOTA callbacks (start/end/progress/error)
 *   and calls ArduinoOTA.begin() once the device has a valid IP address.
 * - begin() will defer startup if NetworkController::ipAddress() returns 0.0.0.0.
 * - loop() periodically retries deferred start (throttled to once per second)
 *   and calls ArduinoOTA.handle() when running.
 */

OtaManager &OtaManager::instance()
{
    static OtaManager inst;
    return inst;
}

OtaManager::OtaManager()
    : arduinoOtaEnabled_(false),
      running_(false),
      pendingStart_(false),
      lastStartAttemptMs_(0)
{
}

OtaManager::~OtaManager()
{
    stop();
}

void OtaManager::begin(bool enableArduinoOta)
{
    arduinoOtaEnabled_ = enableArduinoOta;

    if (!arduinoOtaEnabled_)
    {
        Logger::instance().debug("OtaManager: ArduinoOTA disabled by configuration");
        return;
    }

    // If we already have an IP, start immediately; otherwise mark pending.
    IPAddress ip = NetworkController::instance().ipAddress();
    if (ip != IPAddress(0, 0, 0, 0))
    {
        startArduinoOta();
    }
    else
    {
        pendingStart_ = true;
        lastStartAttemptMs_ = 0;
        Logger::instance().info("OtaManager: deferred ArduinoOTA until network available");
    }
}

void OtaManager::loop()
{
    // If starting was deferred, attempt to start periodically (throttled).
    if (pendingStart_)
    {
        unsigned long now = millis();
        if (now - lastStartAttemptMs_ >= 1000)
        {
            lastStartAttemptMs_ = now;
            IPAddress ip = NetworkController::instance().ipAddress();
            if (ip != IPAddress(0, 0, 0, 0))
            {
                pendingStart_ = false;
                startArduinoOta();
            }
        }
    }

    // Process ArduinoOTA events if running.
    if (running_ && arduinoOtaEnabled_)
    {
        ArduinoOTA.handle();
    }
}

void OtaManager::stop()
{
    if (arduinoOtaEnabled_ && running_)
    {
        stopArduinoOta();
    }
    pendingStart_ = false;
    running_ = false;
}

bool OtaManager::isRunning() const
{
    return running_;
}

void OtaManager::startArduinoOta()
{
    if (running_)
    {
        Logger::instance().debug("OtaManager: startArduinoOta() called but already running");
        return;
    }

    // Optionally set hostname from config for IDE discoverability.
    String devName = Config::instance().getDeviceName();
    if (!devName.isEmpty())
    {
        ArduinoOTA.setHostname(devName.c_str());
    }

    ArduinoOTA.onStart([]()
                       { Logger::instance().info("ArduinoOTA: start"); });

    ArduinoOTA.onEnd([]()
                     {
                         Logger::instance().info("ArduinoOTA: end");
                         // give logging a moment to flush before reboot (if performed)
                         delay(50); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
                              // progress is reported frequently; use debug level
                              if (total > 0)
                              {
                                  unsigned int pct = (unsigned int)((progress * 100ULL) / (total ? total : 1));
                                  Logger::instance().debug(String("ArduinoOTA: progress ") + String(pct) + "%");
                              } });

    ArduinoOTA.onError([](ota_error_t error)
                       {
                           switch (error)
                           {
                           case OTA_AUTH_ERROR:
                               Logger::instance().error("ArduinoOTA: auth failed");
                               break;
                           case OTA_BEGIN_ERROR:
                               Logger::instance().error("ArduinoOTA: begin failed");
                               break;
                           case OTA_CONNECT_ERROR:
                               Logger::instance().error("ArduinoOTA: connect failed");
                               break;
                           case OTA_RECEIVE_ERROR:
                               Logger::instance().error("ArduinoOTA: receive failed");
                               break;
                           case OTA_END_ERROR:
                               Logger::instance().error("ArduinoOTA: end failed");
                               break;
                           default:
                               Logger::instance().error(String("ArduinoOTA: unknown error ") + String((int)error));
                               break;
                           } });

    // Start ArduinoOTA now that callbacks are set.
    ArduinoOTA.begin();
    running_ = true;

    Logger::instance().info(String("OtaManager: ArduinoOTA started, IP=") + NetworkController::instance().ipAddress().toString());
}

void OtaManager::stopArduinoOta()
{
    // There is no canonical ArduinoOTA::end() on some cores; to "stop" we simply
    // clear the running flag so handle() is no longer called.
    running_ = false;
    Logger::instance().info("OtaManager: ArduinoOTA stopped");
}
