#pragma once

#include <Arduino.h>

/**
 * @file version.h
 * @brief Application version constants and helpers.
 *
 * Define semantic version pieces as compile-time constants and provide a
 * small helper to produce the human-readable "MAJOR.MINOR.BUILD" string.
 *
 * Update the constants below for each release/build.
 */
namespace Version
{
    // Semantic version components
    static constexpr uint8_t MAJOR = 0;
    static constexpr uint8_t MINOR = 1;
    static constexpr uint16_t BUILD = 76;
    
    /**
     * @brief Return the full version as a String in the form "MAJOR.MINOR.BUILD".
     *
     * This is implemented inline for convenience; call `Version::toString()` from
     * other modules (e.g., logging, web UI, /info endpoints).
     */
    inline String toString()
    {
        char buf[32];
        // BUILD is 16-bit; format as unsigned to avoid sign issues.
        snprintf(buf, sizeof(buf), "%u.%u.%u", (unsigned)MAJOR, (unsigned)MINOR, (unsigned)BUILD);
        return String(buf);
    }
}
