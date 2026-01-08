/**
 * @file multicaseDns.cpp
 * @brief Implementation of the MulticaseDns singleton wrapper around ESPmDNS.
 *
 * Preserves existing behavior while fixing minor issues (string checks, naming consistency).
 */

#include "multicaseDns.h"

#include <ESPmDNS.h>
#include "Logger.h"
#include "networkController.h"

MulticaseDns &MulticaseDns::instance()
{
    static MulticaseDns inst;
    return inst;
}

MulticaseDns::MulticaseDns()
    : running_(false)
{
}

MulticaseDns::~MulticaseDns()
{
    stop();
}

bool MulticaseDns::begin(const String &hostname)
{
    if (hostname.length() == 0)
    {
        Logger::instance().warn("DNS: empty hostname provided");
        return false;
    }

    // Ensure we have a usable IP before starting mDNS.
    IPAddress ip = NetworkController::instance().ipAddress();
    if (ip == IPAddress(0, 0, 0, 0))
    {
        Logger::instance().warn("DNS: no IP available, cannot start mDNS");
        return false;
    }

    // Start mDNS; MDNS.begin returns false on failure.
    if (!MDNS.begin(hostname.c_str()))
    {
        Logger::instance().error(String("DNS: MDNS.begin failed for '") + hostname + "'");
        running_ = false;
        return false;
    }

    // Read the claimed hostname (may differ from requested if auto-renamed).
    String claimedHostname = this->hostname();
    if (claimedHostname.length() == 0)
    {
        claimedHostname = hostname;
        Logger::instance().debug("DNS: mDNS started but claimed hostname not immediately available");
    }
    else
    {
        Logger::instance().info(String("DNS: mDNS claimed hostname '") + claimedHostname + ".local'");
    }

    running_ = true;
    Logger::instance().info(String("DNS: mDNS started for requested name '") + claimedHostname + ".local' IP=" + ip.toString());
    return true;
}

void MulticaseDns::stop()
{
    if (!running_)
        return;

    MDNS.end();
    Logger::instance().info("DNS: mDNS stopped");
    running_ = false;
}

bool MulticaseDns::addService(const String &service, const String &proto, uint16_t port)
{
    if (!running_)
    {
        Logger::instance().warn("DNS: addService called but mDNS is not running");
        return false;
    }

    if (service.length() == 0 || proto.length() == 0 || port == 0)
    {
        Logger::instance().warn("DNS: invalid addService parameters");
        return false;
    }

    MDNS.addService(service.c_str(), proto.c_str(), port);
    Logger::instance().info(String("DNS: added service ") + service + "." + proto + " port=" + String(port));
    return true;
}

bool MulticaseDns::addServiceTxt(const String &service, const String &proto, const String &key, const String &value)
{
    if (!running_)
    {
        Logger::instance().warn("DNS: addServiceTxt called but mDNS is not running");
        return false;
    }

    if (service.length() == 0 || proto.length() == 0 || key.length() == 0)
    {
        Logger::instance().warn("DNS: invalid addServiceTxt parameters");
        return false;
    }

    MDNS.addServiceTxt(service.c_str(), proto.c_str(), key.c_str(), value.c_str());
    Logger::instance().debug(String("DNS: added TXT ") + key + "=" + value + " to " + service + "." + proto);
    return true;
}

bool MulticaseDns::isRunning() const
{
    return running_;
}

String MulticaseDns::hostname() const
{
    // Query MDNS for the claimed hostname; many cores return an Arduino String.
    String mdnsName = MDNS.hostname(0);
    return mdnsName.length() ? mdnsName : String();
}