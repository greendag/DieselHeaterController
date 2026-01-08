#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>
#include <utility>
#include <cstdint>

/**
 * @file fileSystem.h
 * @brief Singleton wrapper around LittleFS for text and binary file operations.
 *
 * Responsibilities:
 *  - Mount/unmount LittleFS.
 *  - Read/write binary and text files (write = create or overwrite).
 *  - Track file event subscribers and notify them on CREATED / UPDATED / REMOVED.
 *
 * Usage:
 *  FileSystem &fs = FileSystem::instance();
 *  fs.mount();
 *  fs.write("/cfg.bin", data, len);
 *  auto buff = fs.readBinary("/cfg.bin");
 *
 * Thread-safety: NOT thread-safe. Add external synchronization if used from multiple tasks/ISRs.
 *
 * Notes:
 *  - write() emits CREATED when a file did not previously exist, otherwise UPDATED.
 *  - remove() emits REMOVED on success.
 */
struct FileInfo
{
    String name;                 /**< filename or path */
    String type;                 /**< "file" or "dir" */
    size_t size;                 /**< size in bytes */
    unsigned long lastWriteDate; /**< reserved (LittleFS does not expose) */
};

/**
 * @enum FileAction
 * @brief Event types passed to file event callbacks.
 */
enum class FileAction : uint8_t
{
    CREATED, /**< file created */
    UPDATED, /**< existing file overwritten */
    REMOVED  /**< file removed */
};

using FileEventCallback = std::function<void(const String &path, FileAction action)>;

/**
 * @class FileSystem
 * @brief Singleton filesystem helper for LittleFS.
 *
 * Use FileSystem::instance() to obtain the single instance.
 */
class FileSystem
{
public:
    /**
     * @brief Get the singleton instance.
     * @return Reference to the single FileSystem instance.
     */
    static FileSystem &instance();

    /**
     * @brief Register a file event callback.
     * @param callback Callable invoked as callback(path, action).
     * @return Non-zero id used to remove the callback.
     *
     * Notes:
     *  - Callbacks are invoked after a successful write/remove.
     *  - Caller must ensure thread-safety when adding/removing callbacks.
     */
    uint32_t addFileEventCallback(FileEventCallback callback);

    /**
     * @brief Remove a previously registered callback.
     * @param id Id returned from addFileEventCallback.
     * @return true if removed, false if id not found.
     */
    bool removeFileEventCallback(uint32_t id);

    /**
     * @brief Mount the underlying LittleFS.
     * @return true on success.
     */
    bool mount();

    /**
     * @brief Unmount LittleFS.
     */
    void unmount();

    /**
     * @brief Check whether a path exists.
     * @param path Path string (e.g. "/config.json").
     * @return true if the file exists and FS is mounted.
     */
    bool exists(const String &path);

    /**
     * @brief Write text to a file (create or overwrite).
     * @param path File path.
     * @param content Text to write.
     * @return true on success.
     *
     * Semantics: always overwrites existing file. Emits CREATED if file did not exist,
     * otherwise UPDATED.
     */
    bool write(const String &path, const String &content);

    /**
     * @brief Write binary data to a file (create or overwrite).
     * @param path File path.
     * @param data Pointer to bytes (may be nullptr if len==0).
     * @param len Number of bytes.
     * @return true on success.
     */
    bool write(const String &path, const uint8_t *data, size_t len);

    /**
     * @brief Write from a std::vector of bytes (create or overwrite).
     * @param path File path.
     * @param data Byte vector.
     * @return true on success.
     */
    bool write(const String &path, const std::vector<uint8_t> &data);

    /**
     * @brief Read file as text.
     * @param path File path.
     * @return File contents as String, or empty String on error / not found.
     */
    String read(const String &path);

    /**
     * @brief Read file as binary.
     * @param path File path.
     * @return vector<uint8_t> containing file bytes; empty on error or empty file.
     */
    std::vector<uint8_t> readBinary(const String &path);

    /**
     * @brief Remove a file.
     * @param path File path.
     * @return true on success; emits REMOVED event on success.
     */
    bool remove(const String &path);

    /**
     * @brief List directory contents.
     * @param path Directory path (e.g. "/").
     * @return Vector of FileInfo entries; empty on error.
     */
    std::vector<FileInfo> dir(const String &path);

private:
    // Private constructor for singleton
    FileSystem();
    ~FileSystem() = default;

    // non-copyable, non-movable
    FileSystem(const FileSystem &) = delete;
    FileSystem &operator=(const FileSystem &) = delete;
    FileSystem(FileSystem &&) = delete;
    FileSystem &operator=(FileSystem &&) = delete;

    uint32_t nextCallbackId;
    std::vector<std::pair<uint32_t, FileEventCallback>> fileEventCallbacks;

    bool mounted;

    /**
     * @brief Invoke registered callbacks.
     * @param callbacks Snapshot of callback id/function pairs.
     * @param path Path associated with the event.
     * @param action Action type.
     *
     * Implementation note: copies callable objects before invoking to avoid
     * iterator invalidation if callbacks modify registration.
     */
    void notifyCallbacks(const std::vector<std::pair<uint32_t, FileEventCallback>> &callbacks, const String &path, FileAction action);
};
