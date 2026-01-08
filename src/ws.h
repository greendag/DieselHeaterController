#pragma once
/**
 * @file ws.h
 * @brief Minimal singleton wrapper around the Arduino WebServer for serving HTTP.
 *
 * See ws.cpp for wildcard-aware static-file serving behavior.
 */

#include <Arduino.h>
#include <functional>
#include <memory>
#include <vector>
#include <WebServer.h>

class Ws
{
public:
    static Ws &instance();

    // Start/stop server
    bool begin(uint16_t port = 80);
    void stop();

    // Register a route handler
    void on(const String &uri, HTTPMethod method, std::function<void()> handler);

    // Register a handler that receives the underlying WebServer reference.
    // Useful for reading args/body or streaming files using WebServer APIs.
    void onRaw(const String &uri, HTTPMethod method, std::function<void(WebServer &)> handler);

    // Convenience response helper (call from handler)
    void send(int code, const String &contentType, const String &body);

    // Must be called from main loop
    void wsLoop();

    // Serve static files using wildcard/template mapping rules described in ws.cpp
    void serveStatic(const String &uriPrefix, const String &fsPathPrefix);

    bool isRunning() const;

private:
    Ws();
    ~Ws();

    // Static mapping structure used by ws.cpp
    struct StaticMapping
    {
        String uriPrefix; // original provided uriPrefix (normalized)
        String fsPrefix;  // original provided fsPrefix (normalized)
        // derived fields used for matching/substitution:
        String uriBase;     // uriPrefix without trailing "/*" (or normalized exact)
        String fsTemplate;  // fs prefix/template (may contain '*' placeholder)
        bool uriWildcard;   // true if uriPrefix ended with "/*"
        bool fsHasWildcard; // true if fsPrefix contains '*'
    };

    std::unique_ptr<WebServer> server_;
    bool running_;
    std::vector<StaticMapping> staticMappings_;
};
