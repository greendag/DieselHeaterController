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
        _queuedLines[0] = line0;
        _queuedLines[1] = line1;
        _queuedIsError = false;
        return;
    }

    Display::instance().clear();
    Display::instance().printLine(0, line0);
    if (!line1.isEmpty())
        Display::instance().printLine(1, line1);
    Display::instance().update();
}

void DisplayManager::showStatusAt(uint8_t startLine, const String &line0, const String &line1)
{
    if (!Display::instance().available())
        return;

    // clamp startLine to valid range
    if (startLine >= 8)
        startLine = 0;

    if (Display::instance().isSplashActive())
    {
        if (startLine < 8)
            _queuedLines[startLine] = line0;
        if (startLine + 1 < 8 && !line1.isEmpty())
            _queuedLines[startLine + 1] = line1;
        _queuedIsError = false;
        return;
    }

    Display::instance().clear();
    Display::instance().printLine(startLine, line0);
    if (!line1.isEmpty())
        Display::instance().printLine(startLine + 1, line1);
    Display::instance().update();
}

void DisplayManager::showError(const String &msg)
{
    if (!Display::instance().available())
        return;

    if (Display::instance().isSplashActive())
    {
        _queuedLines[0] = msg;
        _queuedIsError = true;
        return;
    }

    Display::instance().clear();
    Display::instance().printLine(0, msg);
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
    bool anyQueued = false;
    for (int i = 0; i < 8; ++i)
    {
        if (!_queuedLines[i].isEmpty())
        {
            anyQueued = true;
            break;
        }
    }

    if (anyQueued)
    {
        if (_queuedIsError)
        {
            // Error messages are shown on line 0 only
            showError(_queuedLines[0]);
        }
        else
        {
            Display::instance().clear();
            for (uint8_t i = 0; i < 8; ++i)
            {
                if (!_queuedLines[i].isEmpty())
                    Display::instance().printLine(i, _queuedLines[i]);
            }
            Display::instance().update();
        }

        // Clear queue
        for (int i = 0; i < 8; ++i)
            _queuedLines[i] = String();
        _queuedIsError = false;
    }
}

bool DisplayManager::available() const
{
    return Display::instance().available();
}
