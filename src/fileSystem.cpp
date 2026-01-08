#include "fileSystem.h"

#include <LittleFS.h>
#include <FS.h>
#include <algorithm>

/**
 * @file fileSystem.cpp
 * @brief Implementation of FileSystem wrapper for LittleFS.
 *
 * Implementation notes:
 *  - Meyers singleton via FileSystem::instance()
 *  - Public APIs auto-attempt mount() when not mounted
 *  - Callback invocation copies callables before invoking to avoid iterator invalidation
 *  - Not thread-safe; caller must provide synchronization if called from multiple tasks/ISRs
 */

/**
 * @brief Normalize a file path to ensure it begins with '/'.
 * @param path Input path (may or may not start with '/').
 * @return Normalized path starting with '/'.
 */
static String normalizePath(const String &path)
{
    if (path.length() == 0)
        return String("/");
    if (path.charAt(0) == '/')
        return path;
    return String("/") + path;
}

/**
 * @brief Obtain the singleton instance.
 * @return Reference to the single FileSystem instance.
 */
FileSystem &FileSystem::instance()
{
    static FileSystem inst;
    return inst;
}

/**
 * @brief Construct the FileSystem (private).
 *
 * Initializes internal callback id counter and mounted flag.
 */
FileSystem::FileSystem()
    : nextCallbackId(1), mounted(false)
{
}

/**
 * @brief Register a file event callback.
 * @param callback Callable invoked as callback(path, action).
 * @return Non-zero id used to remove the callback later.
 *
 * Note: IDs never return 0; wrap-around is handled.
 */
uint32_t FileSystem::addFileEventCallback(FileEventCallback callback)
{
    uint32_t id;
    do
    {
        id = nextCallbackId++;
        if (nextCallbackId == 0) // wrapped
            nextCallbackId = 1;
    } while (id == 0 ||
             std::any_of(fileEventCallbacks.begin(), fileEventCallbacks.end(),
                         [id](const std::pair<uint32_t, FileEventCallback> &p)
                         { return p.first == id; }));

    fileEventCallbacks.emplace_back(id, std::move(callback));
    return id;
}

/**
 * @brief Remove a previously registered callback.
 * @param id Id returned from addFileEventCallback.
 * @return true if removed, false if not found.
 */
bool FileSystem::removeFileEventCallback(uint32_t id)
{
    for (auto it = fileEventCallbacks.begin(); it != fileEventCallbacks.end(); ++it)
    {
        if (it->first == id)
        {
            fileEventCallbacks.erase(it);
            return true;
        }
    }
    return false;
}

/**
 * @brief Mount LittleFS.
 * @return true on success.
 */
bool FileSystem::mount()
{
    mounted = LittleFS.begin();
    return mounted;
}

/**
 * @brief Unmount LittleFS and clear mounted flag.
 */
void FileSystem::unmount()
{
    LittleFS.end();
    mounted = false;
}

/**
 * @brief Check file existence.
 * @param path File path.
 * @return true if file exists and filesystem is mounted.
 *
 * Auto-attempts mount() if not mounted.
 */
bool FileSystem::exists(const String &path)
{
    if (!mounted && !mount())
        return false;

    String p = normalizePath(path);

    return LittleFS.exists(p.c_str());
}

/**
 * @brief Write text to a file (create or overwrite).
 * @param path File path.
 * @param content Text content.
 * @return true on success.
 *
 * Delegates to binary write overload.
 */
bool FileSystem::write(const String &path, const String &content)
{
    return write(path, reinterpret_cast<const uint8_t *>(content.c_str()), content.length());
}

/**
 * @brief Write binary data to a file (create or overwrite).
 * @param path File path.
 * @param data Pointer to bytes (may be nullptr if len == 0).
 * @param len Number of bytes to write.
 * @return true on success.
 *
 * Emits CREATED if file did not exist, otherwise UPDATED.
 * Auto-attempts mount() if not mounted.
 */
bool FileSystem::write(const String &path, const uint8_t *data, size_t len)
{
    if (!mounted && !mount())
        return false;

    String p = normalizePath(path);

    bool existed = LittleFS.exists(p.c_str());

    File f = LittleFS.open(p.c_str(), "w");
    if (!f)
        return false;

    size_t written = 0;
    if (len > 0 && data != nullptr)
        written = f.write(data, len);
    f.close();

    if (written != len)
        return false;

    notifyCallbacks(fileEventCallbacks, p, existed ? FileAction::UPDATED : FileAction::CREATED);
    return true;
}

/**
 * @brief Convenience write overload for std::vector<uint8_t>.
 */
bool FileSystem::write(const String &path, const std::vector<uint8_t> &data)
{
    return write(path, data.empty() ? nullptr : data.data(), data.size());
}

/**
 * @brief Read file as text.
 * @param path File path.
 * @return Contents as String, or empty String on error/not found.
 *
 * Reserves file size to reduce reallocations.
 */
String FileSystem::read(const String &path)
{
    if (!mounted && !mount())
        return String();

    String p = normalizePath(path);

    if (!LittleFS.exists(p.c_str()))
        return String();

    File f = LittleFS.open(p.c_str(), "r");
    if (!f)
        return String();

    String result;
    size_t fsize = f.size();
    
    if (fsize > 0)
        result.reserve(fsize);

    const size_t bufSize = 128;
    char buf[bufSize];
    
    while (f.available())
    {
        size_t len = f.readBytes(buf, bufSize);
        if (len > 0)
            result.concat(buf, len);
    }

    f.close();
    
    return result;
}

/**
 * @brief Read file as binary.
 * @param path File path.
 * @return vector<uint8_t> with file data; empty on error or empty file.
 *
 * Reserves capacity when file size is known.
 */
std::vector<uint8_t> FileSystem::readBinary(const String &path)
{
    std::vector<uint8_t> out;
    if (!mounted && !mount())
        return out;

    String p = normalizePath(path);
    
    if (!LittleFS.exists(p.c_str()))
        return out;

    File f = LittleFS.open(p.c_str(), "r");
    if (!f)
        return out;

    size_t fsize = f.size();
    if (fsize > 0)
        out.reserve(fsize);

    const size_t bufSize = 128;
    uint8_t buf[bufSize];

    while (f.available())
    {
        size_t len = f.read(buf, bufSize);
        if (len > 0)
            out.insert(out.end(), buf, buf + len);
    }

    f.close();
    return out;
}

/**
 * @brief Remove a file.
 * @param path File path.
 * @return true on success.
 *
 * Emits REMOVED event on success.
 */
bool FileSystem::remove(const String &path)
{
    if (!mounted && !mount())
        return false;

    String p = normalizePath(path);

    bool ok = LittleFS.remove(p.c_str());
    if (ok)
    {
        notifyCallbacks(fileEventCallbacks, p, FileAction::REMOVED); 
    }

    return ok;
}

/**
 * @brief List directory contents.
 * @param path Directory path (e.g. "/").
 * @return Vector of FileInfo entries; empty on error.
 *
 * Note: lastWriteDate is not provided by LittleFS and is set to 0.
 */
std::vector<FileInfo> FileSystem::dir(const String &path)
{
    std::vector<FileInfo> list;

    if (!mounted && !mount())
        return list;

    FS &fs = LittleFS;

    String p = normalizePath(path);

    File root = fs.open(p.c_str());
    if (!root || !root.isDirectory())
        return list;

    File file = root.openNextFile();
    while (file)
    {
        FileInfo info;
        info.name = String(file.name());
        info.size = file.size();
        info.type = file.isDirectory() ? String("dir") : String("file");
        info.lastWriteDate = 0; // not available in LittleFS API here
        list.push_back(info);
        file = root.openNextFile();
    }
    root.close();
    return list;
}

/**
 * @brief Notify registered callbacks about a file action.
 * @param callbacks Snapshot of id/callable pairs.
 * @param path Path associated with the event.
 * @param action Action that occurred.
 *
 * Copies only callable objects before invoking to reduce copy cost and avoid iterator invalidation.
 */
void FileSystem::notifyCallbacks(const std::vector<std::pair<uint32_t, FileEventCallback>> &callbacks, const String &path, FileAction action)
{
    std::vector<FileEventCallback> cbs;
    cbs.reserve(callbacks.size());
    for (const auto &p : callbacks)
    {
        if (p.second)
            cbs.push_back(p.second);
    }

    for (const auto &cb : cbs)
    {
        if (cb)
            cb(path, action);
    }
}