#include "displayManager.h"
#include "display.h"
#include "Logger.h"
#include "config.h"

DisplayManager &DisplayManager::instance()
{
    static DisplayManager inst;
    return inst;
}

bool DisplayManager::initWithSplash(uint8_t sda, uint8_t scl, const String &title, const String &subtitle, uint32_t durationMs)
{
    if (Display::instance().begin(sda, scl))
    {
        // Register a callback so we can show any queued status after the
        // splash finishes.
        Display::instance().startSplash(title.c_str(), subtitle.c_str(), durationMs,
                                        std::bind(&DisplayManager::onSplashFinished, this), 0, 1);
        return true;
    }
    return false;
}

void DisplayManager::showStatus(const String &line0, const String &line1)
{
    // If splash is active, queue the status to be shown afterwards.
    if (!Display::instance().available())
        return;

    if (Display::instance().isSplashActive())
    {
        _queuedLine0 = line0;
        _queuedLine1 = line1;
        _queuedIsError = false;
        return;
    }

    Display::instance().clear();
    Display::instance().printLine(0, line0.c_str());
    if (!line1.isEmpty())
        Display::instance().printLine(1, line1.c_str());
    Display::instance().update();
}

void DisplayManager::showError(const String &msg)
{
    if (!Display::instance().available())
        return;

    if (Display::instance().isSplashActive())
    {
        _queuedLine0 = msg;
        _queuedLine1 = String();
        _queuedIsError = true;
        return;
    }

    Display::instance().clear();
    Display::instance().printLine(0, msg.c_str());
    Display::instance().update();
}

void DisplayManager::run()
{
    // Delegate splash loop (Display handles no-op if unavailable).
    Display::instance().splashLoop();
}

void DisplayManager::onSplashFinished()
{
    // If something was queued while the splash was active, show it now.
    if (!_queuedLine0.isEmpty() || !_queuedLine1.isEmpty())
    {
        if (_queuedIsError)
            showError(_queuedLine0);
        else
            showStatus(_queuedLine0, _queuedLine1);

        // Clear queue
        _queuedLine0 = String();
        _queuedLine1 = String();
        _queuedIsError = false;
    }
}

bool DisplayManager::available() const
{
    return Display::instance().available();
}
