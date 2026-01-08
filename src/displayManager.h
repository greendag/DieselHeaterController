#pragma once

#include <Arduino.h>

class DisplayManager
{
public:
    static DisplayManager &instance();

    // Initialize display and start a splash if available.
    // Returns true on successful init.
    bool initWithSplash(uint8_t sda, uint8_t scl, const String &title, const String &subtitle, uint32_t durationMs = 3000);

    // Show two-line status; second line may be empty.
    void showStatus(const String &line0, const String &line1 = String());
    // Show two-line status starting at a specific logical line (0..7).
    // Useful for multi-line layouts where callers want to place text
    // at arbitrary rows (e.g., show additional info on lines 2..3).
    void showStatusAt(uint8_t startLine, const String &line0, const String &line1 = String());

    // Show a single-line error message (on line 0).
    void showError(const String &msg);

    // Call from loop() to drive splash lifecycle.
    void run();

    // Convenience: whether low-level display is available.
    bool available() const;

private:
    DisplayManager() = default;
    // Queued lines shown after splash finishes (up to 8 logical text rows)
    String _queuedLines[8];
    bool _queuedIsError = false;

    // Called by the display when the splash finishes.
    void onSplashFinished();
};
