#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>

class Display
{
public:
    // Get the singleton instance
    static Display &instance();

    // Initialize I2C (SDA, SCL) and the SSD1306. Returns true on success.
    // Uses I2C address 0x3C and geometry 128x64.
    bool begin(uint8_t sda, uint8_t scl);

    // Basic operations
    void clear();                       // clear buffer, does not push
    void update();                      // push buffer to display
    void setContrast(uint8_t contrast); // set contrast (0-255)
    void invert(bool inv);              // invert display

    // Print text on a logical text line (0..7). Uses built-in small font.
    // Text longer than line width will be clipped.
    void printLine(uint8_t line, const String &text);

    // Show a vertical menu (items.size() up to 8 visible rows). Selected
    // index is highlighted. Calls display() internally.
    void showMenu(const std::vector<String> &items, size_t selected);

    // Start a splash screen with a larger title and optional subtitle.
    // Non-blocking: returns immediately. Call `splashLoop()` from `loop()`
    // to allow the display to be shown for at least `durationMs`.
    // Optionally provide a callback invoked once when the splash finishes.
    // `preferredTitleSize` / `preferredSubSize` (0 = auto) allow callers to
    // request larger/smaller built-in font sizes. Values refer to Adafruit
    // `setTextSize()` multipliers (1..n). If 0, sizes are chosen automatically.
    void startSplash(const String &title, const String &subtitle, uint32_t durationMs = 3000,
                     std::function<void()> onFinish = nullptr, uint8_t preferredTitleSize = 0,
                     uint8_t preferredSubSize = 0);

    // Call periodically (e.g., from `loop()`) to update splash state.
    // Returns true while the splash is active.
    bool splashLoop();

    // Query whether the display is available (init succeeded).
    bool available() const { return !_disabled; }

    // Query whether a splash screen is currently active.
    bool isSplashActive() const;

private:
    Display();
    ~Display();

    // Non-copyable
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;

    // Implementation detail (opaque)
    struct Impl;
    Impl *_impl;
    bool _disabled;
};
