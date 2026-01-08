#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include <functional>
#include "fileSystem.h"
#include "provisioning.h"

/**
 * Lightweight console/command dispatcher.
 *
 * - Singleton via Console::instance()
 * - Inject input/output Stream (defaults to Serial) via init()
 * - Register commands with registerCommand() or use registerDefaultCommands()
 * - Call consoleLoop() from main loop() to process incoming serial data
 * - processLine() is public for unit testing
 */
class Console
{
public:
    using Handler = std::function<void(const std::vector<String> &args, Stream &out)>;

    static Console &instance();

    // Optional: set custom IO streams (default Serial)
    void init(Stream *inStream = &Serial, Stream *outStream = &Serial);

    // If true, echo input characters back to out stream (default true)
    void setEchoInput(bool enable);
    bool getEchoInput() const;

    // Register a command handler (name case-insensitive)
    void registerCommand(const String &name, Handler handler, const String &description = String());

    // Add built-in commands (help, echo, cat, dir, factoryreset, provision)
    void registerDefaultCommands();

    // Process incoming data from configured input Stream; call frequently from loop()
    void consoleLoop();

    // Parse a single line into tokens (supports quoted strings and escapes)
    std::vector<String> parseCommand(const String &input) const;

    // Process a single line (callable from tests)
    void processLine(const String &line);

private:
    Console();
    ~Console() = default;

    // non-copyable
    Console(const Console &) = delete;
    Console &operator=(const Console &) = delete;

    Stream *in_;
    Stream *out_;
    String lineBuffer_;
    bool echoInput_;

    struct CmdEntry
    {
        Handler handler;
        String description;
        String displayName; // original name/casing for help display
    };

    // Primary lookup by normalized key
    std::map<String, CmdEntry> commands_;

    // Preserve insertion order of normalized keys for deterministic help listing
    std::vector<String> commandOrder_;

    // helpers
    static String normalizeKey(const String &s);
    void handleHelp(const std::vector<String> &args, Stream &out);
};
