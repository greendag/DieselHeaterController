#include "OnBoardLed.h"

OnBoardLed &OnBoardLed::instance()
{
    static OnBoardLed inst;
    return inst;
}

OnBoardLed::OnBoardLed()
    : strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800),
      isBlinking(false),
      lastToggle(0),
      onDuration(500),
      offDuration(500),
      isOn(false),
      currentColor(0x000000),
      currentBrightness(255)
{
    strip.begin();
    strip.setBrightness(currentBrightness);
    strip.show(); // ensure LED is off on init
}

uint32_t OnBoardLed::colorFromRGB(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

bool OnBoardLed::parseHexColor(const String &hex, uint8_t &r, uint8_t &g, uint8_t &b)
{
    String s = hex;
    s.trim();
    if (s.startsWith("#"))
        s = s.substring(1);
    if (s.length() != 6)
        return false;

    char buf[7] = {0};
    s.toCharArray(buf, sizeof(buf)); // ensures null-termination
    char *endptr = nullptr;
    long val = strtol(buf, &endptr, 16);
    if (endptr == buf)
        return false;

    r = (val >> 16) & 0xFF;
    g = (val >> 8) & 0xFF;
    b = val & 0xFF;
    return true;
}

void OnBoardLed::applyColor()
{
    strip.setBrightness(currentBrightness);
    uint8_t r = (currentColor >> 16) & 0xFF;
    uint8_t g = (currentColor >> 8) & 0xFF;
    uint8_t b = currentColor & 0xFF;
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
}

void OnBoardLed::rgb(uint8_t r, uint8_t g, uint8_t b)
{
    currentColor = colorFromRGB(r, g, b);
    isOn = true;
    applyColor();
}

bool OnBoardLed::setHexColor(const String &hexColor)
{
    uint8_t r, g, b;
    if (!parseHexColor(hexColor, r, g, b))
        return false;
    rgb(r, g, b);
    return true;
}

void OnBoardLed::off()
{
    strip.setPixelColor(0, 0);
    strip.show();
    isOn = false;
}

void OnBoardLed::intensity(uint8_t level)
{
    if (level > 100)
        level = 100;
    currentBrightness = map((int)level, 0, 100, 0, 255);
    strip.setBrightness(currentBrightness);
    if (isOn)
        applyColor();
}

bool OnBoardLed::startBlink(const String &hexColor, uint8_t intensity, uint32_t onMs, uint32_t offMs)
{
    uint8_t r, g, b;
    if (!parseHexColor(hexColor, r, g, b))
        return false;

    currentColor = colorFromRGB(r, g, b);
    if (intensity > 100)
        intensity = 100;
    currentBrightness = map((int)intensity, 0, 100, 0, 255);

    onDuration = onMs;
    offDuration = offMs;
    isBlinking = true;
    isOn = true;
    lastToggle = millis();

    applyColor();
    return true;
}

void OnBoardLed::stopBlink()
{
    isBlinking = false;
    // leave LED showing steady color at currentBrightness
    isOn = true;
    applyColor();
}

void OnBoardLed::blinkLoop()
{
    if (!isBlinking)
        return;

    unsigned long now = millis();
    if (isOn)
    {
        if ((now - lastToggle) >= onDuration)
        {
            // turn off
            strip.setPixelColor(0, 0);
            strip.show();
            isOn = false;
            lastToggle = now;
        }
    }
    else
    {
        if ((now - lastToggle) >= offDuration)
        {
            // turn on
            applyColor();
            isOn = true;
            lastToggle = now;
        }
    }
}