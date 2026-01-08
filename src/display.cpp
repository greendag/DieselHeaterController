#include "display.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
// Custom fonts provided by Adafruit GFX (bundled with the library)
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <functional>

namespace
{
    constexpr int SCREEN_WIDTH = 128;
    constexpr int SCREEN_HEIGHT = 64;
    constexpr uint8_t SSD1306_ADDR = 0x3C;
    // Font metrics: built-in font is 8px tall at text size 1
    constexpr uint8_t LINE_HEIGHT = 8;
    constexpr uint8_t MAX_LINES = SCREEN_HEIGHT / LINE_HEIGHT; // 8
}

struct Display::Impl
{
    // construct Adafruit_SSD1306 with Wire; reset pin = -1 (use library default)
    Adafruit_SSD1306 oled{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1};
    // Splash state
    bool splashActive = false;
    String splashTitle;
    String splashSubtitle;
    uint32_t splashEndMs = 0;
    uint8_t splashTitleSize = 2;
    uint8_t splashSubSize = 1;
    std::function<void()> splashCallback;
};

Display::Display() : _impl(nullptr), _disabled(true) {}

Display::~Display()
{
    if (_impl)
    {
        delete _impl;
        _impl = nullptr;
    }
}

Display &Display::instance()
{
    static Display inst;
    return inst;
}

bool Display::begin(uint8_t sda, uint8_t scl)
{
    // If already initialized, return current state
    if (_impl)
    {
        return !_disabled;
    }

    // Initialize I2C on the specified pins.
    // You confirmed SDA=21, SCL=4 when calling begin(21, 4).
    Wire.begin(sda, scl);

    _impl = new Impl();

    // Attempt to initialize the display.
    // SSD1306_SWITCHCAPVCC asks the library to use the charge pump.
    if (!_impl->oled.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDR))
    {
        // init failed: leave disabled so methods are safe no-ops
        delete _impl;
        _impl = nullptr;
        _disabled = true;
        return false;
    }

    // Basic setup
    _impl->oled.clearDisplay();
    _impl->oled.setTextSize(1);
    _impl->oled.setTextColor(SSD1306_WHITE);
    _impl->oled.setRotation(0);
    _impl->oled.display();

    _disabled = false;
    return true;
}

void Display::clear()
{
    if (_disabled || !_impl)
        return;
    _impl->oled.clearDisplay();
}

void Display::update()
{
    if (_disabled || !_impl)
        return;
    _impl->oled.display();
}

void Display::setContrast(uint8_t contrast)
{
    if (_disabled || !_impl)
        return;
    _impl->oled.ssd1306_command(SSD1306_SETCONTRAST);
    _impl->oled.ssd1306_command(contrast);
}

void Display::invert(bool inv)
{
    if (_disabled || !_impl)
        return;
    if (inv)
        _impl->oled.invertDisplay(true);
    else
        _impl->oled.invertDisplay(false);
}

void Display::printLine(uint8_t line, const String &text)
{
    if (_disabled || !_impl)
        return;
    if (line >= MAX_LINES)
        return;

    const uint8_t y = line * LINE_HEIGHT;
    // Clear that line area first
    _impl->oled.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, SSD1306_BLACK);

    _impl->oled.setCursor(0, y);
    _impl->oled.setTextSize(1);
    _impl->oled.setTextColor(SSD1306_WHITE);
    // Adafruit GFX prints Arduino String just fine
    _impl->oled.print(text);
    // Note: does not call display() so multiple lines can be updated before a single display()
}

void Display::showMenu(const std::vector<String> &items, size_t selected)
{
    if (_disabled || !_impl)
        return;
    _impl->oled.clearDisplay();
    _impl->oled.setTextSize(1);

    // We render up to MAX_LINES items starting from items[0]
    const size_t visible = min<size_t>(items.size(), MAX_LINES);
    for (size_t i = 0; i < visible; ++i)
    {
        uint8_t y = i * LINE_HEIGHT;

        bool isSelected = (i == selected);
        if (isSelected)
        {
            // draw filled background for selection
            _impl->oled.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, SSD1306_WHITE);
            _impl->oled.setTextColor(SSD1306_BLACK);
        }
        else
        {
            _impl->oled.setTextColor(SSD1306_WHITE);
        }

        _impl->oled.setCursor(0, y);
        // truncate text if too long for width; print will clip automatically.
        _impl->oled.print(items[i]);
    }

    _impl->oled.display();
}

void Display::startSplash(const String &title, const String &subtitle, uint32_t durationMs, std::function<void()> onFinish, uint8_t preferredTitleSize, uint8_t preferredSubSize)
{
    if (_disabled || !_impl)
        return;
    _impl->splashTitle = title;
    _impl->splashSubtitle = subtitle;
    _impl->splashTitleSize = 2;
    _impl->splashSubSize = 1;
    _impl->splashEndMs = millis() + durationMs;
    _impl->splashActive = true;
    _impl->splashCallback = onFinish;

    // Render once now: prefer large Adafruit GFX fonts when they fit, but
    // fall back to built-in scaled font sizes. If `preferredTitleSize` or
    // `preferredSubSize` are non-zero the caller requests built-in sizes and
    // we'll honour them (clamped to fit the screen).
    _impl->oled.clearDisplay();
    _impl->oled.setTextColor(SSD1306_WHITE);

    const GFXfont *titleFonts[] = {&FreeSans18pt7b, &FreeSans12pt7b, nullptr};
    const GFXfont *subFonts[] = {&FreeSans9pt7b, nullptr};

    bool rendered = false;
    const int16_t gap = 6;

    // If caller requested preferred built-in sizes, skip trying GFX fonts
    // and go directly to the built-in-size layout.
    bool skipGfx = (preferredTitleSize != 0) || (preferredSubSize != 0);

    // Try using GFX fonts first (larger, nicer-looking)
    for (const GFXfont *tfont : titleFonts)
    {
        if (skipGfx && tfont != nullptr)
            continue;
        for (const GFXfont *sfont : subFonts)
        {
            if (skipGfx && sfont != nullptr)
                continue;
            if (tfont)
                _impl->oled.setFont(tfont);
            else
                _impl->oled.setFont(nullptr);

            int16_t bx, by;
            uint16_t bw, bh;
            _impl->oled.getTextBounds(_impl->splashTitle, 0, 0, &bx, &by, &bw, &bh);

            if (sfont)
                _impl->oled.setFont(sfont);
            else
                _impl->oled.setFont(nullptr);

            int16_t sbx, sby;
            uint16_t sbw, sbh;
            _impl->oled.getTextBounds(_impl->splashSubtitle, 0, 0, &sbx, &sby, &sbw, &sbh);

            // Compute centered positions using bounds (account for baseline offsets)
            int16_t titleX = (SCREEN_WIDTH - static_cast<int16_t>(bw)) / 2 - bx;
            int16_t titleY = (SCREEN_HEIGHT / 2) - (static_cast<int16_t>(bh + sbh + gap) / 2) - by;

            int16_t subX = (SCREEN_WIDTH - static_cast<int16_t>(sbw)) / 2 - sbx;
            int16_t subY = titleY + static_cast<int16_t>(bh) + gap - sby;

            // Validate vertical fit
            if (titleY >= 0 && (subY + static_cast<int16_t>(sbh)) <= SCREEN_HEIGHT)
            {
                // Draw title
                if (tfont)
                    _impl->oled.setFont(tfont);
                else
                    _impl->oled.setFont(nullptr);
                _impl->oled.setCursor(titleX, titleY);
                _impl->oled.print(_impl->splashTitle);

                // Draw subtitle
                if (sfont)
                    _impl->oled.setFont(sfont);
                else
                    _impl->oled.setFont(nullptr);
                _impl->oled.setCursor(subX, subY);
                _impl->oled.print(_impl->splashSubtitle);

                rendered = true;
                break;
            }
        }
        if (rendered)
            break;
    }

    // Fall back to built-in font sizes (allow larger sizes for title)
    if (!rendered)
    {
        // Determine title size preference: caller value preferredTitleSize
        // (if non-zero) or try a large size down to 1.
        uint8_t titleSize = preferredTitleSize ? preferredTitleSize : 4;
        int16_t titleW = 0;
        // Decrease until it fits (or reach 1)
        while (titleSize > 1)
        {
            titleW = static_cast<int16_t>(_impl->splashTitle.length()) * 6 * titleSize;
            if (titleW <= SCREEN_WIDTH)
                break;
            --titleSize;
        }
        if (titleSize < 1)
            titleSize = 1;

        // Subtitle size: prefer caller's setting, else try 2 then 1
        uint8_t subSize = preferredSubSize ? preferredSubSize : 2;
        int16_t subW = static_cast<int16_t>(_impl->splashSubtitle.length()) * 6 * subSize;
        while (subSize > 1 && subW > SCREEN_WIDTH)
        {
            --subSize;
            subW = static_cast<int16_t>(_impl->splashSubtitle.length()) * 6 * subSize;
        }

        int16_t titleH = 8 * titleSize;
        int16_t subH = 8 * subSize;
        int16_t totalH = titleH + gap + subH;
        int16_t startY = (SCREEN_HEIGHT - totalH) / 2;
        if (startY < 0)
            startY = 0;

        _impl->oled.setFont(nullptr);
        _impl->oled.setTextSize(titleSize);
        int16_t tx = (SCREEN_WIDTH - titleW) / 2;
        if (tx < 0)
            tx = 0;
        _impl->oled.setCursor(tx, startY + titleH - 2);
        _impl->oled.print(_impl->splashTitle);

        _impl->oled.setTextSize(subSize);
        int16_t sx = (SCREEN_WIDTH - subW) / 2;
        if (sx < 0)
            sx = 0;
        _impl->oled.setCursor(sx, startY + titleH + gap + subH - 2);
        _impl->oled.print(_impl->splashSubtitle);
    }

    // Reset to built-in font for other UI code
    _impl->oled.setFont(nullptr);
    _impl->oled.display();
}

bool Display::splashLoop()
{
    if (_disabled || !_impl)
        return false;

    if (!_impl->splashActive)
        return false;

    if (millis() >= _impl->splashEndMs)
    {
        _impl->splashActive = false;
        // Invoke callback if provided
        if (_impl->splashCallback)
        {
            // Call and clear to avoid repeated calls
            auto cb = _impl->splashCallback;
            _impl->splashCallback = nullptr;
            cb();
        }
        return false;
    }

    return true;
}

bool Display::isSplashActive() const
{
    if (_disabled || !_impl)
        return false;
    return _impl->splashActive;
}
