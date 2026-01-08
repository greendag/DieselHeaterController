/**
 * @file ws.cpp
 * @brief Implementation of the Ws singleton wrapper around Arduino WebServer.
 *
 * Lifecycle, route registration, per-loop handling and wildcard-aware static-file serving.
 */

#include "ws.h"
#include "Logger.h"

#include <LittleFS.h>

Ws &Ws::instance()
{
    static Ws inst;
    return inst;
}

Ws::Ws()
    : server_(nullptr), running_(false)
{
}

Ws::~Ws()
{
    stop();
}

static String contentTypeForPath(const String &path)
{
    if (path.endsWith(".html"))
        return "text/html";
    if (path.endsWith(".htm"))
        return "text/html";
    if (path.endsWith(".js"))
        return "application/javascript";
    if (path.endsWith(".css"))
        return "text/css";
    if (path.endsWith(".json"))
        return "application/json";
    if (path.endsWith(".png"))
        return "image/png";
    if (path.endsWith(".jpg"))
        return "image/jpeg";
    if (path.endsWith(".jpeg"))
        return "image/jpeg";
    if (path.endsWith(".gif"))
        return "image/gif";
    if (path.endsWith(".svg"))
        return "image/svg+xml";
    if (path.endsWith(".ico"))
        return "image/x-icon";
    return "application/octet-stream";
}

bool Ws::begin(uint16_t port)
{
    if (running_)
    {
        Logger::instance().debug(String("WS: already running on port ") + String(port));
        return true;
    }

    if (!LittleFS.begin())
    {
        Logger::instance().warn("WS: LittleFS.begin() failed or already mounted");
        // continue; static serving will just fail if FS unavailable
    }

    server_.reset(new WebServer(port));
    if (!server_)
    {
        Logger::instance().error("WS: failed to allocate WebServer");
        return false;
    }

    // NotFound handler: wildcard-aware static mapping first, then 404
    server_->onNotFound([this]()
                        {
        String uri = server_->uri();

        // Find best mapping: exact match preferred, otherwise longest prefix wildcard
        int bestIdx = -1;
        size_t bestScore = 0; // exact match => SIZE_MAX

        for (size_t i = 0; i < staticMappings_.size(); ++i)
        {
            const auto &m = staticMappings_[i];
            if (m.uriWildcard)
            {
                // prefix match: uriBase is the part before the "/*"
                if (uri.startsWith(m.uriBase))
                {
                    size_t score = m.uriBase.length();
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestIdx = (int)i;
                    }
                }
            }
            else
            {
                // exact match (normalize trailing slash rules)
                String u = m.uriBase;
                if (u == uri ||
                    (u.length() > 1 && u.endsWith("/") && uri == u.substring(0, u.length()-1)) ||
                    (uri.endsWith("/") && uri.substring(0, uri.length()-1) == u))
                {
                    bestIdx = (int)i;
                    bestScore = SIZE_MAX; // force exact preference
                    break;
                }
            }
        }

        if (bestIdx >= 0)
        {
            const auto &m = staticMappings_[bestIdx];
            String filePath;

            if (m.uriWildcard)
            {
                // suffix after the base prefix
                String relative = uri.substring(m.uriBase.length());
                // drop leading slash for substitution
                if (relative.startsWith("/")) relative = relative.substring(1);
                if (relative.length() == 0 || uri.endsWith("/")) relative = "index.html";

                if (m.fsHasWildcard)
                {
                    int pos = m.fsTemplate.indexOf('*');
                    if (pos >= 0)
                    {
                        String before = m.fsTemplate.substring(0, pos);
                        String after = m.fsTemplate.substring(pos + 1);
                        // normalize slashes
                        if (before.endsWith("/") && relative.startsWith("/"))
                            relative = relative.substring(1);
                        filePath = before + relative + after;
                    }
                    else
                    {
                        // append relative to template
                        if (m.fsTemplate.endsWith("/"))
                            filePath = m.fsTemplate + relative;
                        else
                            filePath = m.fsTemplate + "/" + relative;
                    }
                }
                else
                {
                    if (m.fsTemplate.endsWith("/"))
                        filePath = m.fsTemplate + relative;
                    else
                        filePath = m.fsTemplate + "/" + relative;
                }
            }
            else
            {
                // non-wildcard: serve fsTemplate as-is (if it's directory, add index.html)
                filePath = m.fsTemplate;
                if (filePath.endsWith("/"))
                    filePath += "index.html";
            }

            if (!filePath.startsWith("/"))
                filePath = "/" + filePath;

            if (LittleFS.exists(filePath))
            {
                File f = LittleFS.open(filePath, "r");
                if (f)
                {
                    String ctype = contentTypeForPath(filePath);
                    server_->streamFile(f, ctype.c_str());
                    f.close();
                    return;
                }
            }
            else
            {
                Logger::instance().debug(String("WS: static file not found ") + filePath);
            }
        }

        String msg = String("Not Found: ") + uri + " method=" + String(server_->method());
        Logger::instance().debug(msg);
        server_->send(404, "text/plain", "Not Found"); });

    server_->begin();
    running_ = true;
    Logger::instance().info(String("WS: started on port ") + String(port));
    return true;
}

void Ws::stop()
{
    if (!running_)
        return;

    if (server_)
    {
        server_->stop();
        server_.reset();
    }
    running_ = false;
    Logger::instance().info("WS: stopped");
}

void Ws::on(const String &uri, HTTPMethod method, std::function<void()> handler)
{
    if (!running_)
    {
        Logger::instance().warn(String("WS: on() called for '") + uri + "' but server not running");
        return;
    }

    server_->on(uri.c_str(), method, [handler]()
                {
        if (handler) handler(); });

    Logger::instance().debug(String("WS: registered route ") + uri + " method=" + String(method));
}

void Ws::onRaw(const String &uri, HTTPMethod method, std::function<void(WebServer &)> handler)
{
    if (!running_)
    {
        Logger::instance().warn(String("WS: onRaw() called for '") + uri + "' but server not running");
        return;
    }

    server_->on(uri.c_str(), method, [this, handler]()
                {
        if (handler)
            handler(*server_); });

    Logger::instance().debug(String("WS: registered raw route ") + uri + " method=" + String(method));
}

void Ws::send(int code, const String &contentType, const String &body)
{
    if (!running_ || !server_)
    {
        Logger::instance().warn("WS: send() called but server not running");
        return;
    }
    server_->send(code, contentType.c_str(), body);
}

void Ws::wsLoop()
{
    if (!running_ || !server_)
        return;
    server_->handleClient();
}

void Ws::serveStatic(const String &uriPrefix, const String &fsPathPrefix)
{
    // Normalize without corrupting wildcard forms like "/*"
    String u = uriPrefix;
    if (!u.startsWith("/"))
        u = "/" + u;

    bool uriWildcard = false;
    String uriBase = u;

    if (u.endsWith("/*"))
    {
        uriWildcard = true;
        // uriBase keeps the trailing '/' so substringing works (e.g. "/": base "/" )
        uriBase = u.substring(0, u.length() - 1); // keep trailing '/'
    }
    else
    {
        // normalize exact mappings: remove trailing slash except for root "/"
        if (uriBase.length() > 1 && uriBase.endsWith("/"))
            uriBase.remove(uriBase.length() - 1);
    }

    String f = fsPathPrefix;
    if (!f.startsWith("/"))
        f = "/" + f;

    bool fsHasWildcard = (f.indexOf('*') >= 0);
    String fsTemplate = f;

    // update existing mapping if same base & wildcard flag
    for (auto &m : staticMappings_)
    {
        if (m.uriBase == uriBase && m.uriWildcard == uriWildcard)
        {
            m.fsPrefix = f;
            m.fsTemplate = fsTemplate;
            m.fsHasWildcard = fsHasWildcard;
            Logger::instance().debug(String("WS: updated static mapping ") + uriPrefix + " -> " + fsPathPrefix);
            return;
        }
    }

    StaticMapping m;
    m.uriPrefix = u;
    m.fsPrefix = f;
    m.uriBase = uriBase;
    m.fsTemplate = fsTemplate;
    m.uriWildcard = uriWildcard;
    m.fsHasWildcard = fsHasWildcard;

    staticMappings_.push_back(m);
    Logger::instance().info(String("WS: added static mapping ") + uriPrefix + " -> " + fsPathPrefix);
}

bool Ws::isRunning() const
{
    return running_;
}
