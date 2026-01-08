#include "console.h"
#include "Logger.h"
#include "config.h"
#include "provisioning.h"

Console &Console::instance()
{
    static Console inst;
    return inst;
}

Console::Console()
    : in_(&Serial), out_(&Serial), lineBuffer_(), echoInput_(true)
{
    registerDefaultCommands();
}

void Console::init(Stream *inStream, Stream *outStream)
{
    in_ = inStream ? inStream : &Serial;
    out_ = outStream ? outStream : &Serial;
}

void Console::setEchoInput(bool enable)
{
    echoInput_ = enable;
}

bool Console::getEchoInput() const
{
    return echoInput_;
}

String Console::normalizeKey(const String &s)
{
    String r = s;
    r.trim();
    r.toLowerCase();
    return r;
}

void Console::registerCommand(const String &name, Handler handler, const String &description)
{
    String key = normalizeKey(name);
    auto it = commands_.find(key);
    if (it == commands_.end())
    {
        // new insertion: record order and store display name
        CmdEntry e;
        e.handler = handler;
        e.description = description;
        e.displayName = name;
        commands_[key] = e;
        commandOrder_.push_back(key);
    }
    else
    {
        // replace handler/description/displayName but preserve original order
        it->second.handler = handler;
        it->second.description = description;
        it->second.displayName = name;
    }
}

void Console::registerDefaultCommands()
{
    registerCommand("help", [this](const std::vector<String> &a, Stream &o)
                    { handleHelp(a, o); }, "Show this help");

    registerCommand("echo", [](const std::vector<String> &args, Stream &out)
                    {
                        bool first = true;
                        for (const auto &p : args)
                        {
                            if (!first)
                                out.print(' ');
                            out.print(p);
                            first = false;
                        }
                        out.println(); }, "Echo arguments");

    registerCommand("cat", [](const std::vector<String> &args, Stream &out)
                    {
                        if (args.empty())
                        {
                            out.println(F("Usage: cat <path>"));
                            return;
                        }
                        String path = args[0];
                        FileSystem &fs = FileSystem::instance();
                        if (!fs.exists(path))
                        {
                            out.println(F("File not found"));
                            return;
                        }
                        String content = fs.read(path);
                        out.print(content);
                        out.println(); }, "Print file contents");

    registerCommand("dir", [](const std::vector<String> &args, Stream &out)
                    {
                        String path = "/";
                        if (!args.empty())
                            path = args[0];
                        FileSystem &fs = FileSystem::instance();
                        auto entries = fs.dir(path);
                        for (const auto &fi : entries)
                        {
                            out.print(fi.name);
                            out.print(F("\t"));
                            out.print(fi.size);
                            out.print(F("\t"));
                            out.println(fi.type);
                        } }, "List directory");

    registerCommand("factoryreset", [](const std::vector<String> & /*args*/, Stream &out)
                    {
                        out.println(F("Performing factory reset..."));
                        Provisioning::instance().reset();
                        out.println(F("Factory reset requested.")); }, "Reset device to factory defaults");

    registerCommand("provision", [](const std::vector<String> &args, Stream &out)
                    {
                        if (args.size() < 2)
                        {
                            out.println(F("Usage: provision <ssid> <password> [deviceName]"));
                            return;
                        }
                        String ssid = args[0];
                        String pwd = args[1];
                        String devName = (args.size() >= 3) ? args[2] : String();
                        Provisioning::instance().provision(ssid, pwd, devName);
                        out.println(F("Provisioning data saved.")); }, "Save WiFi credentials");
}

// parseCommand: supports quoted strings, escaped quotes (\") and escaped backslash (\\)
std::vector<String> Console::parseCommand(const String &input) const
{
    std::vector<String> tokens;
    String cur;
    bool inQuote = false;
    bool escape = false;

    for (size_t i = 0; i < input.length(); ++i)
    {
        char c = input.charAt(i);

        if (escape)
        {
            // handle simple escapes: \", \\ and any other char passes through
            cur += c;
            escape = false;
            continue;
        }

        if (c == '\\')
        {
            escape = true;
            continue;
        }

        if (c == '"')
        {
            inQuote = !inQuote;
            continue;
        }

        if (!inQuote && isspace(c))
        {
            if (cur.length() > 0)
            {
                tokens.push_back(cur);
                cur = "";
            }
            continue;
        }

        cur += c;
    }

    if (cur.length() > 0)
        tokens.push_back(cur);

    return tokens;
}

void Console::processLine(const String &line)
{
    auto parts = parseCommand(line);
    if (parts.empty())
        return;

    String cmd = normalizeKey(parts[0]);
    std::vector<String> args;
    for (size_t i = 1; i < parts.size(); ++i)
        args.push_back(parts[i]);

    auto it = commands_.find(cmd);
    if (it == commands_.end())
    {
        out_->print(F("Unknown command: "));
        out_->println(parts[0]);
        return;
    }

    try
    {
        it->second.handler(args, *out_);
    }
    catch (...)
    {
        out_->println(F("Command handler exception"));
    }
}

void Console::consoleLoop()
{
    if (!in_ || !out_)
        return;

    while (in_->available())
    {
        int c = in_->read();
        if (c < 0)
            break;

        // Echo input if enabled
        if (echoInput_)
        {
            if (c == '\r')
            {
                // ignore CR but echo CRLF when newline follows
            }
            else if (c == '\n')
            {
                out_->print("\r\n");
            }
            else if (c == 127 || c == '\b')
            {
                // handle backspace echo: erase char on terminal
                if (lineBuffer_.length() > 0)
                {
                    // typical sequence: backspace, space, backspace
                    out_->print("\b \b");
                }
                else
                {
                    // no char to erase, just ignore (optional bell)
                }
            }
            else
            {
                out_->print((char)c);
            }
        }

        if (c == '\r')
            continue;
        if (c == '\n')
        {
            String line = lineBuffer_;
            line.trim();
            lineBuffer_.clear();
            if (line.length() > 0)
                processLine(line);
        }
        else if (c == 127 || c == '\b')
        {
            if (lineBuffer_.length() > 0)
                lineBuffer_.remove(lineBuffer_.length() - 1);
        }
        else
        {
            lineBuffer_.concat((char)c);
        }
    }
}

void Console::handleHelp(const std::vector<String> & /*args*/, Stream &out)
{
    out.println(F("Available commands:"));
    for (const auto &key : commandOrder_)
    {
        auto it = commands_.find(key);
        if (it == commands_.end())
            continue;
        String name = it->second.displayName;
        String desc = it->second.description;
        out.print("  ");
        out.print(name);
        if (desc.length() > 0)
        {
            out.print(F(" - "));
            out.print(desc);
        }
        out.println();
    }
}
