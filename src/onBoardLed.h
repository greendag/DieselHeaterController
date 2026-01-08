#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/**
 * @file OnBoardLed.h
 * @brief Singleton controller for the on-board NeoPixel LED.
 *
 * Use OnBoardLed::instance() to access the single instance.
 * Call blinkLoop() from your main loop to drive non-blocking blinking.
 */
class OnBoardLed
{
public:
    // Access singleton instance
    static OnBoardLed &instance();

    // non-copyable, non-movable
    OnBoardLed(const OnBoardLed &) = delete;
    OnBoardLed &operator=(const OnBoardLed &) = delete;
    OnBoardLed(OnBoardLed &&) = delete;
    OnBoardLed &operator=(OnBoardLed &&) = delete;

    // Set color by components (0-255)
    void rgb(uint8_t r, uint8_t g, uint8_t b);

    // Set color with hex string: "#RRGGBB" or "RRGGBB"
    bool setHexColor(const String &hexColor);

    // Turn off LED
    void off();

    // Set intensity 0-100 (maps to 0-255)
    void intensity(uint8_t level);

    // Start non-blocking blink with hex color and intensity (on/off durations in ms)
    bool startBlink(const String &hexColor, uint8_t intensity, uint32_t onMs = 500, uint32_t offMs = 500);

    // Stop blinking and restore current color/intensity
    void stopBlink();

    // Call from loop() to drive blinking
    void blinkLoop();

private:
    OnBoardLed();
    ~OnBoardLed() = default;

    static constexpr uint8_t LED_PIN = 48;
    static constexpr uint8_t NUM_PIXELS = 1;

    Adafruit_NeoPixel strip;

    volatile bool isBlinking;
    volatile unsigned long lastToggle;
    volatile uint32_t onDuration;
    volatile uint32_t offDuration;
    volatile bool isOn;

    uint32_t currentColor;     // 0xRRGGBB
    uint8_t currentBrightness; // 0-255

    // helpers
    static uint32_t colorFromRGB(uint8_t r, uint8_t g, uint8_t b);
    static bool parseHexColor(const String &hex, uint8_t &r, uint8_t &g, uint8_t &b);
    void applyColor();
};
