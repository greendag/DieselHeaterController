/**
 * @file main.cpp
 * @brief Main entry point for the Diesel Heater Controller ESP32 application.
 *
 * Initializes system components, handles provisioning, and runs the main loop for console and LED control.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#include "Logger.h"
#include "OnBoardLed.h"
#include "System.h"
#include "config.h"
#include "console.h"
#include "fileSystem.h"
#include "networkController.h"
#include "otaManager.h"
#include "provisioning.h"
#include "ws.h"
#include "displayManager.h"

void setup()
{
  // Initialize logger and system clock early so other components can use timestamps/uptime.
  System::instance().init();
  Logger::instance().init(115200);
  OtaManager::instance().begin(true);

  // Initialize display (I2C pins moved to config.h)
  if (DisplayManager::instance().initWithSplash(DISPLAY_SDA, DISPLAY_SCL, "Diesel Heater", "Starting...", 3000))
  {
    Logger::instance().info("Display initialized");
  }
  else
  {
    Logger::instance().warn("Display unavailable (init failed)");
  }

  bool initSuccess = true;

  if (!FileSystem::instance().mount())
  {
    initSuccess = false;
    Logger::instance().error("Filesystem mount failed");
    DisplayManager::instance().showError("FS mount failed");
  }

  if (initSuccess && !Provisioning::instance().isProvisioned())
  {
    Logger::instance().info("Device not provisioned, starting provisioning mode");
    // Show the original provisioning message first, then start provisioning
    DisplayManager::instance().showStatus("Provisioning", "AP mode started");
    if (Provisioning::instance().start())
    {
      // Provisioning::start() will update the display with the AP SSID and URL.
      OnBoardLed::instance().startBlink("#FFFF00", 5, 250, 250);
    }
    else
    {
      Logger::instance().error("Failed to start provisioning AP");
      OnBoardLed::instance().startBlink("#FF0000", 75, 500, 500);
      DisplayManager::instance().showError("Provisioning Failed");
    }
  }
  else if (initSuccess)
  {
    Logger::instance().info("Device provisioned, starting normal operation");

    if (NetworkController::instance().connectToWiFi())
    {
      Logger::instance().info("Connected to WiFi network");
      ArduinoOTA.begin();
      OnBoardLed::instance().startBlink("#00FF00", 5, 1000, 2000);

      DisplayManager::instance().showStatus("WiFi Connected", "Normal mode");
    }
    else
    {
      Logger::instance().warn("Failed to connect to WiFi network, entering error state");
      OnBoardLed::instance().startBlink("#FF0000", 75, 500, 500);
      DisplayManager::instance().showStatus("WiFi failed", "Check network");
    }
  }
  else
  {
    Logger::instance().error("Initialization failed, entering error state");
    OnBoardLed::instance().startBlink("#FF0000", 75, 500, 500);
    DisplayManager::instance().showError("Init failed");
  }
}

void loop()
{
  delay(10);

  // Drive the non-blocking splash screen state if active
  DisplayManager::instance().run();

  Provisioning::instance().checkFactoryResetButton();
  Provisioning::instance().provisioningLoop();
  ArduinoOTA.handle();
  OnBoardLed::instance().blinkLoop();
  Console::instance().consoleLoop();
  Config::instance().poll();
  Ws::instance().wsLoop();
  OtaManager::instance().loop();
}
