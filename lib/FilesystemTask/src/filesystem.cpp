// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <LittleFS.h>
#include <queue.h>
#include <functional>
#include <TimeLib.h>
#include "hardware/watchdog.h"
#include "SchedulerExt.h"
#include "filesystem.h"
#include <PicoLog.h>
#include "stringutils.h"
#include "TimeFormat.h"

#define FILE_BUF_SIZE   512
#define MAX_DIR_LEVELS  10          // maximum number of directory levels to list (limits the recursion in the list function)
#define FILE_BLOCK_DETECT_TICKS pdMS_TO_TICKS(1000)
#define FILE_BLOCK_WARN_EVERY_INTERVALS 2
static constexpr size_t kFsRequestPoolSize = 32u;
static constexpr uint8_t kFsBlockedScratchIndex = 2u;
static constexpr uint32_t kFsBlockedMagic = 0xFB000000u;

#define OTA_COMMAND_FILE "/otacommand.bin"     // must match the _OTA_COMMAND_FILE name in the ../include/rp2040/pico_base/pico/ota_command.h
#define FW_BIN_FILE "/fw.bin"                  // must match the csFWImageFilename name in the app include/constants.hpp
#define FIRMWARE_BIN_FILE "/firmware.bin"                  // additional FW upgrade file related to OTA_COMMAND_FILE

SynchronizedFS SyncFsImpl;
FS SyncLittleFS(FSImplPtr(&SyncFsImpl));
static auto rootDir = FS_PATH_SEPARATOR;

//ahead definitions
void fsExecute();
void fsInit();

// filesystem task definition - priority is overwritten during setup, see fsSetup
TaskDef fsDef {fsInit, fsExecute, 1536, "FS", 1, CORE_0};

enum class FsPayloadKind : uint8_t { NONE, STRING, BYTES, FILE_INFO_LIST, FILE_INFO, FS_STAT, };

struct FsPayload {
    const void *ptr = nullptr;
    size_t size = 0;
    FsPayloadKind kind = FsPayloadKind::NONE;
    bool writable = false;

    void reset() {
        ptr = nullptr;
        size = 0;
        kind = FsPayloadKind::NONE;
        writable = false;
    }

    template<typename T> [[nodiscard]] const T *as() const {
        return static_cast<const T *>(ptr);
    }

    template<typename T> [[nodiscard]] T *asMutable() const {
        if (!writable)
            return nullptr;

        return static_cast<T *>(const_cast<void *>(ptr));
    }
};

struct FsRequest {
    enum class Action : uint8_t { READ_FILE, WRITE_FILE, WRITE_FILE_ASYNC, APPEND_FILE, APPEND_FILE_BIN, RENAME,
        DELETE, EXISTS, FORMAT, LIST_FILES, INFO, STAT, MAKE_DIR, SHA256 };

    enum class PayloadStorage : uint8_t { NONE, BORROWED, REQUEST_STRING };

    Action action = Action::FORMAT;
    TaskHandle_t replyTask = nullptr;
    String path{};
    FsPayload payload{};
    PayloadStorage payloadStorage = PayloadStorage::NONE;
    QueueHandle_t completionQueue = nullptr;
    uint16_t completionId = 0;
    String ownedString{};

    void reset() {
        action = Action::INFO;
        replyTask = nullptr;
        path = "";
        payload.reset();
        payloadStorage = PayloadStorage::NONE;
        completionQueue = nullptr;
        completionId = 0;
        ownedString = "";
    }
};

class FsRequestPool {
    FsRequest requests[kFsRequestPoolSize]{};
    bool inUse[kFsRequestPoolSize]{};

public:
    FsRequest *acquire() {
        taskENTER_CRITICAL();
        for (size_t i = 0; i < kFsRequestPoolSize; ++i) {
            if (!inUse[i]) {
                inUse[i] = true;
                requests[i].reset();
                taskEXIT_CRITICAL();
                return &requests[i];
            }
        }
        taskEXIT_CRITICAL();
        return nullptr;
    }

    void release(FsRequest *request) {
        if (request == nullptr)
            return;

        taskENTER_CRITICAL();
        for (size_t i = 0; i < kFsRequestPoolSize; ++i) {
            if (&requests[i] == request) {
                requests[i].reset();
                inUse[i] = false;
                break;
            }
        }
        taskEXIT_CRITICAL();
    }
};

static FsRequestPool gFsRequestPool;

[[maybe_unused]] static const char* fsActionToString(const FsRequest::Action action) {
    switch (action) {
        case FsRequest::Action::READ_FILE: return "READ_FILE";
        case FsRequest::Action::WRITE_FILE: return "WRITE_FILE";
        case FsRequest::Action::WRITE_FILE_ASYNC: return "WRITE_FILE_ASYNC";
        case FsRequest::Action::APPEND_FILE: return "APPEND_FILE";
        case FsRequest::Action::APPEND_FILE_BIN: return "APPEND_FILE_BIN";
        case FsRequest::Action::RENAME: return "RENAME";
        case FsRequest::Action::DELETE: return "DELETE";
        case FsRequest::Action::EXISTS: return "EXISTS";
        case FsRequest::Action::FORMAT: return "FORMAT";
        case FsRequest::Action::LIST_FILES: return "LIST_FILES";
        case FsRequest::Action::INFO: return "INFO";
        case FsRequest::Action::STAT: return "STAT";
        case FsRequest::Action::MAKE_DIR: return "MAKE_DIR";
        case FsRequest::Action::SHA256: return "SHA256";
        default: return "UNKNOWN";
    }
}

static FsRequest *acquireFsRequest() {
    auto *request = gFsRequestPool.acquire();
    if (request == nullptr) {
        log_error(F("FS request pool exhausted (%u entries)"), static_cast<unsigned>(kFsRequestPoolSize));
    }
    return request;
}

static FsRequest *makeBorrowedRequest(const FsRequest::Action action, const TaskHandle_t replyTask, const char *path,
        const void *payload = nullptr, const FsPayloadKind payloadKind = FsPayloadKind::NONE, const size_t size = 0,
        const bool writablePayload = false) {
    auto *request = acquireFsRequest();
    if (request == nullptr)
        return nullptr;

    request->action = action;
    request->replyTask = replyTask;
    request->path = path ? path : "";
    request->payload = FsPayload{payload, size, payloadKind, writablePayload};
    request->payloadStorage = payload != nullptr ? FsRequest::PayloadStorage::BORROWED : FsRequest::PayloadStorage::NONE;
    return request;
}

static FsRequest *makeOwnedStringRequest(const FsRequest::Action action, const char *path, const String &value,
        const QueueHandle_t completionQueue = nullptr, const uint16_t completionId = 0, const TaskHandle_t replyTask = nullptr) {
    auto *request = acquireFsRequest();
    if (request == nullptr)
        return nullptr;

    request->action = action;
    request->replyTask = replyTask;
    request->path = path ? path : "";
    request->ownedString = value;
    request->payload = FsPayload{&request->ownedString, request->ownedString.length(), FsPayloadKind::STRING, false};
    request->payloadStorage = FsRequest::PayloadStorage::REQUEST_STRING;
    request->completionQueue = completionQueue;
    request->completionId = completionId;
    return request;
}

static bool enqueueFsRequest(const QueueHandle_t queue, FsRequest *request) {
    if (request == nullptr)
        return false;

    auto *queuedRequest = request;
    const BaseType_t qResult = xQueueSend(queue, &queuedRequest, 0);
    if (qResult != pdTRUE) {
        log_error(F("Failed to enqueue FS request %s for %s - error %d"), fsActionToString(request->action), request->path.c_str(), qResult);
        gFsRequestPool.release(request);
        return false;
    }
    return true;
}

static bool validatePayloadKind(const FsRequest &request, const FsPayloadKind expected) {
    if (request.payload.kind == expected)
        return true;

    log_error(F("FS request %s for %s has invalid payload kind %u, expected %u"),
              fsActionToString(request.action), request.path.c_str(),
              static_cast<unsigned>(request.payload.kind), static_cast<unsigned>(expected));
    return false;
}

template<typename T>
static T *getMutablePayload(const FsRequest &request) {
    auto *payload = request.payload.template asMutable<T>();
    if (payload != nullptr)
        return payload;

    log_error(F("FS request %s for %s requires writable payload"), fsActionToString(request.action), request.path.c_str());
    return nullptr;
}

/**
 * Waits for FS task completion without a hard timeout. It emits periodic warnings if an operation appears blocked.
 */
static uint32_t waitForFsCompletion(const FsRequest::Action action, const char *path, const QueueHandle_t queue) {
    constexpr TickType_t minTicks = 1;
    const TickType_t detectTicks = FILE_BLOCK_DETECT_TICKS > 0 ? FILE_BLOCK_DETECT_TICKS : minTicks;

    uint32_t intervals = 0;
    for (;;) {
        uint32_t result = 0;
        if (xTaskNotifyWait(0u, 0xFFFFFFFFu, &result, detectTicks) == pdTRUE) {
            watchdog_hw->scratch[kFsBlockedScratchIndex] = 0u;
            if (intervals > 0) {
                log_error(F("FS operation %s for %s resumed after %lu ms. caller=%s"), fsActionToString(action), (path && *path) ? path : "<none>",
                    intervals * detectTicks * portTICK_PERIOD_MS, pcTaskGetName(nullptr));
            }
            return result;
        }

        ++intervals;
        const uint32_t waitedSeconds = (intervals * detectTicks * portTICK_PERIOD_MS) / 1000u;
        watchdog_hw->scratch[kFsBlockedScratchIndex] =
            kFsBlockedMagic | (static_cast<uint32_t>(action) << 16) | (waitedSeconds & 0xFFFFu);
        if (intervals == 1 || (intervals % FILE_BLOCK_WARN_EVERY_INTERVALS) == 0) {
            log_error(F("FS operation %s for %s blocked for %lu ms. caller=%s queueDepth=%u"), fsActionToString(action), (path && *path) ? path : "<none>",
                intervals * detectTicks * portTICK_PERIOD_MS, pcTaskGetName(nullptr), static_cast<unsigned>(uxQueueMessagesWaiting(queue)));
        }
    }
}

/**
 * Lists all the files - recursively - from the directory provided for logging purposes
 * Maximum depth of recursive calls is limited to MAX_DIR_LEVELS (to protect from stack overflow)
 * @param fs the filesystem object
 * @param path directory path to start from
 * @param s string that collects the listings
 * @param depth the depth level of directory listing
 * @param callback callback to act on a directory entry as the tree is traversed
 */
static void logFilesRecurse(FS &fs, const String &path, String &s, const uint8_t depth, const std::function<void(Dir&)> &callback) {
    if (depth > MAX_DIR_LEVELS) return;

    const uint8_t level = depth * 2;
    Dir dir = fs.openDir(path.c_str());
    while (dir.next()) {
        callback(dir);
        if (dir.isFile()) {
            File f = dir.openFile("r");
            String ts = TimeFormat::asString(f.getLastWrite());
            const size_t sz = dir.fileSize();
            String name = dir.fileName();
            f.close();
            StringUtils::append(s, F("%*c%s\t[%zu]  %s\n"), level, ' ', ts.c_str(), sz, name.c_str());
        } else if (dir.isDirectory()) {
            String ts = TimeFormat::asString(dir.fileCreationTime());
            String name = dir.fileName();
            StringUtils::append(s, F("%*c%s\t<DIR>  %s\n"), level, ' ', ts.c_str(), name.c_str());

            // Build child absolute path
            String childPath = path;
            if (!(childPath.length() == 1 && childPath[0] == '/')) childPath.concat(FS_PATH_SEPARATOR);
            childPath.concat(name);

            logFilesRecurse(fs, childPath, s, depth + 1, callback);
        } else {
            StringUtils::append(s, F("%*c????\t???????? %s\n"), level, ' ', dir.fileName().c_str());
        }
    }
}

static void logFiles(FS& fs, const char *startPath, String &s, const std::function<void(Dir&)> &callback) {
    String base = (startPath && *startPath) ? String(startPath) : String(FS_PATH_SEPARATOR);
    // Normalize: remove trailing slash except root
    if (base.length() > 1 && base.endsWith(FS_PATH_SEPARATOR)) {
        base.remove(base.length()-1);
    }

    if (!fs.exists(base.c_str())) {
        StringUtils::append(s, F("Path does not exist: %s\n"), base.c_str());
        return;
    }

    // Header for the starting directory
    StringUtils::append(s, F("%*c<DIR>  %s\n"), 0, ' ', base.c_str());
    logFilesRecurse(fs, base, s, 1, callback);
}

/**
 * Lists all files recursively from the directory provided. No maximum level of depth for recursive calls is enforced.
 * Ensure enough stack is available if the provided structure has a deep nested structure
 * @param fs file system object
 * @param path accumulated absolute path of the directory
 * @param callback callback to execute for each directory entry
 */
static void listFiles(FS& fs, String &path, const std::function<void(const FileInfo&)> &callback) {
    Dir dir = fs.openDir(path);
    while (dir.next()) {
        FileInfo fInfo {dir.fileName(), path, dir.fileSize(), dir.fileTime(), dir.isDirectory()};
        callback(fInfo);
        if (fInfo.isDir) {
            path.concat(FS_PATH_SEPARATOR);
            path.concat(fInfo.name);
            Dir d = fs.openDir(path);
            listFiles(fs, path, callback);
            path.remove(path.length()-fInfo.name.length()-1);
        }
    }
}

/**
 * Initializes the filesystem objects and the task responsible for handling file access
 * Runs on FS dedicated task
 */
void fsInit() {
    SyncFsImpl.fsPtr->setTimeCallback(now);
    if (SyncFsImpl.fsPtr->begin())
        log_info(F("Filesystem OK"));
    else
        log_error(F("Filesystem initialization FAILED!"));

    if (FSInfo fsInfo{}; SyncFsImpl.fsPtr->info(fsInfo)) {
        log_info(F("Filesystem information (size in bytes): totalSize %llu, used %llu, maxOpenFiles %zu, maxPathLength %zu, pageSize %zu, blockSize %zu"),
                   fsInfo.totalBytes, fsInfo.usedBytes, fsInfo.maxOpenFiles, fsInfo.maxPathLength, fsInfo.pageSize, fsInfo.blockSize);
    } else {
        log_error(F("Cannot retrieve filesystem (LittleFS) information"));
    }

    // collect saved files under 64 bytes - a sign of corruption - and delete them
    std::queue<String> corruptedFiles;
    auto collectCorruptedFiles = [&l=corruptedFiles](Dir &d) {
        if (d.isFile() && d.fileSize() < 64) {
            l.push(d.fileName());
        }
    };

    String dirContent;
    dirContent.reserve(640);
    dirContent.concat(F("Filesystem content:\n"));
    logFiles(*SyncFsImpl.fsPtr, rootDir, dirContent, collectCorruptedFiles);
    dirContent.concat(F("End of filesystem content.\n"));

    if (!corruptedFiles.empty()) {
        StringUtils::append(dirContent, F("Found %d (likely) corrupted files (size < 64 bytes), deleting\n"), corruptedFiles.size());
        while (!corruptedFiles.empty()) {
            (void)SyncFsImpl.prvRemove(corruptedFiles.front().c_str());
            corruptedFiles.pop();
        }
    }

    //check for otacommand.bin and fw.bin - this means a FW upgrade just occurred; delete the files
    if (SyncFsImpl.prvExists(OTA_COMMAND_FILE)) {
        log_info(F("=== FW Upgrade has completed!! Welcome to the other side! Cleaning up the FW files (%s) ==="), OTA_COMMAND_FILE);
        (void)SyncFsImpl.prvRemove(OTA_COMMAND_FILE);
    }
    if (SyncFsImpl.prvExists(FW_BIN_FILE)) {
        log_info(F("=== FW Upgrade has completed!! Welcome to the other side! Cleaning up the FW files (%s) ==="), FW_BIN_FILE);
        (void)SyncFsImpl.prvRemove(FW_BIN_FILE);
    }
    if (SyncFsImpl.prvExists(FIRMWARE_BIN_FILE)) {
        log_info(F("=== FW Upgrade has completed!! Welcome to the other side! Cleaning up the FW files (%s) ==="), FIRMWARE_BIN_FILE);
        (void)SyncFsImpl.prvRemove(FIRMWARE_BIN_FILE);
    }

    log_info(dirContent.c_str());
}

/**
 * The task scheduler executes this function in a loop, no need to account for that here
 */
void fsExecute() {
    FsRequest *request = nullptr;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(SyncFsImpl.queue, &request, portMAX_DELAY))
        return;
    // the reception was successful, hence the request pointer is valid
    uint32_t result = 0;
    bool completionSuccess = false;
    switch (request->action) {
        case FsRequest::Action::READ_FILE:
            if (validatePayloadKind(*request, FsPayloadKind::STRING)) {
                if (auto *payload = getMutablePayload<String>(*request))
                    result = static_cast<uint32_t>(SyncFsImpl.prvReadFile(request->path.c_str(), payload));
            }
            break;
        case FsRequest::Action::WRITE_FILE:
            if (validatePayloadKind(*request, FsPayloadKind::STRING))
                result = static_cast<uint32_t>(SyncFsImpl.prvWriteFile(request->path.c_str(), request->payload.as<String>()));
            break;
        case FsRequest::Action::WRITE_FILE_ASYNC:
            if (validatePayloadKind(*request, FsPayloadKind::STRING))
                result = static_cast<uint32_t>(SyncFsImpl.prvWriteFile(request->path.c_str(), request->payload.as<String>()));
            if (result == 0u) {
                log_error(F("Failed to write file %s asynchronously"), request->path.c_str());
            }
            break;
        case FsRequest::Action::APPEND_FILE:
            if (validatePayloadKind(*request, FsPayloadKind::STRING))
                result = static_cast<uint32_t>(SyncFsImpl.prvAppendFile(request->path.c_str(), request->payload.as<String>()));
            break;
        case FsRequest::Action::APPEND_FILE_BIN:
            if (validatePayloadKind(*request, FsPayloadKind::BYTES))
                result = static_cast<uint32_t>(SyncFsImpl.prvAppendFile(request->path.c_str(), request->payload.as<uint8_t>(), request->payload.size));
            break;
        case FsRequest::Action::DELETE:
            result = SyncFsImpl.prvRemove(request->path.c_str()) ? 1u : 0u;
            break;
        case FsRequest::Action::RENAME:
            if (validatePayloadKind(*request, FsPayloadKind::STRING))
                result = SyncFsImpl.prvRename(request->path.c_str(), request->payload.as<String>()) ? 1u : 0u;
            break;
        case FsRequest::Action::EXISTS:
            result = SyncFsImpl.prvExists(request->path.c_str()) ? 1u : 0u;
            break;
        case FsRequest::Action::FORMAT:
            result = SyncFsImpl.prvFormat() ? 1u : 0u;
            break;
        case FsRequest::Action::LIST_FILES:
            if (validatePayloadKind(*request, FsPayloadKind::FILE_INFO_LIST)) {
                if (auto *payload = getMutablePayload<std::deque<FileInfo>>(*request))
                    result = SyncFsImpl.prvList(request->path.c_str(), payload) ? 1u : 0u;
            }
            break;
        case FsRequest::Action::INFO:
            if (validatePayloadKind(*request, FsPayloadKind::FILE_INFO)) {
                if (auto *payload = getMutablePayload<FileInfo>(*request))
                    result = SyncFsImpl.prvInfo(request->path.c_str(), payload) ? 1u : 0u;
            }
            break;
        case FsRequest::Action::STAT:
            if (validatePayloadKind(*request, FsPayloadKind::FS_STAT)) {
                if (auto *payload = getMutablePayload<FSStat>(*request))
                    result = SyncFsImpl.prvStat(request->path.c_str(), payload) ? 1u : 0u;
            }
            break;
        case FsRequest::Action::MAKE_DIR:
            result = SyncFsImpl.prvMakeDir(request->path.c_str()) ? 1u : 0u;
            break;
        case FsRequest::Action::SHA256:
            if (validatePayloadKind(*request, FsPayloadKind::STRING)) {
                if (auto *payload = getMutablePayload<String>(*request))
                    result = SyncFsImpl.prvSha256(request->path.c_str(), payload) ? 1u : 0u;
            }
            break;
        default:
            log_error(F("FileSystem Event type %u not supported"), static_cast<unsigned>(request->action));
            break;
    }

    completionSuccess = result > 0;
    if (request->replyTask != nullptr) {
        xTaskNotify(request->replyTask, result, eSetValueWithOverwrite);
    }
    if (request->completionQueue != nullptr && completionSuccess) {
        if (xQueueSend(request->completionQueue, &request->completionId, 0) != pdTRUE) {
            log_error(F("Failed to post async write completion for %s"), request->path.c_str());
        }
    }

    gFsRequestPool.release(request);
}

SynchronizedFS::SynchronizedFS() = default;

/**
 * Constructor that sets the underlying filesystem to use
 * Given that the typical usage of SynchronizedFS is to instantiate it in global space, this constructor is impractical:
 * a statement of e.g. <code>SynchronizedFS fs(LittleFS);</code> in global space renders a warning from IDE:
 * "Initializing non-local variable with non-const expression depending on uninitialized non-local variable 'LittleFS'"
 * @param fs file system to use
 */
SynchronizedFS::SynchronizedFS(FS &fs) : fsPtr(&fs) {
}

SynchronizedFS::~SynchronizedFS() {
    // same implementation as end(), but we don't want to call a virtual method in the destructor
    Scheduler.stopTask(fsTask);
    if (fsPtr)
        fsPtr->end();
}

bool SynchronizedFS::setConfig(const FSConfig &cfg) {
    if (fsPtr)
        return fsPtr->setConfig(cfg);
    return false;
}

bool SynchronizedFS::begin() {
    if (fsPtr)
        return begin(*fsPtr);
    return false;
}

/**
 * See https://arduino-pico.readthedocs.io/en/latest/freertos.html#caveats - the LittleFS is not thread safe and
 * recommended to do all file operations in a single thread
 * This method can be called from any task
 */
bool SynchronizedFS::begin(FS &fs) {
    fsPtr = &fs;
    queue = xQueueCreate(kFsRequestPoolSize, sizeof(FsRequest *));
    //mirror the priority of the calling task - the filesystem task is intended to have the same priority
    fsDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle())+1;
    fsTask = Scheduler.startTask(&fsDef);
    TaskStatus_t tStat;
    vTaskGetTaskInfo(fsTask->getTaskHandle(), &tStat, pdFALSE, eReady);
    log_info(F("Filesystem task [%s] - priority %d - has been setup id %u. Events are dispatching."), tStat.pcTaskName, tStat.uxCurrentPriority, tStat.xTaskNumber);
    return true;
}

/**
 * Blocking function that reads a text file leveraging the filesystem task. Can be called from any task.
 * NOTE: the request object comes from a fixed pool. Blocking calls borrow caller-owned payloads; async
 * string calls copy their content into request-owned storage before enqueue.
 *
 * @param fname file name to read
 * @param s content recipient
 * @return number of bytes read - 0 if file does not exist or cannot be read for some reason (e.g. timeout)
 */
size_t SynchronizedFS::readFile(const char *fname, String *s) const {
    auto *request = makeBorrowedRequest(FsRequest::Action::READ_FILE, xTaskGetCurrentTaskHandle(), fname, s, FsPayloadKind::STRING, 0, true);
    if (request == nullptr) {
        log_error(F("Failed to create request to read file - request pool likely full: fname=%s"), fname);
        return 0;
    }
    log_info(F("Sending READ_FILE message to filesystem task for file name %s"), fname);

    size_t sz = 0;
    if (enqueueFsRequest(queue, request)) {
        //wait for the filesystem task to finish and notify us
        sz = waitForFsCompletion(FsRequest::Action::READ_FILE, fname, queue);
        if (sz == 0)
            log_error(F("Error reading file %s - error waiting for completion; size %d"), fname, sz);
        else
            log_info(F("File %s read successfully - size %d"), fname, sz);
    }
    return sz;
}

/**
 * Blocking function that writes the string content into a file with provided name. Can be called from any task.
 * @param fname file name to write into
 * @param s content to write
 * @return number of bytes written - 0 if there was an error (e.g. timeout)
 */
size_t SynchronizedFS::writeFile(const char *fname, const String *s) const {
    auto *request = makeBorrowedRequest(FsRequest::Action::WRITE_FILE, xTaskGetCurrentTaskHandle(), fname, s, FsPayloadKind::STRING);
    if (request == nullptr) {
        log_error(F("Failed to create request to write file - request pool likely full: fname=%s"), fname);
        return 0;
    }
    log_info(F("Sending WRITE_FILE message to filesystem task for file name %s"), fname);

    size_t sz = 0;
    if (enqueueFsRequest(queue, request)) {
        //wait for the filesystem task to finish and notify us
        sz = waitForFsCompletion(FsRequest::Action::WRITE_FILE, fname, queue);
        if (sz == 0)
            log_error(F("Error writing file %s - error waiting for completion; size %d"), fname, sz);
        else
            log_info(F("File %s written successfully - size %d"), fname, sz);
    }
    return sz;
}

/**
 * Non-blocking function that writes the string content into a file with provided name. Can be called from any task.
 * @param fname file path to write - absolute path
 * @param s the content to write
 * @param completionQueue
 * @param completionId
 * @return true if successfully enqueued to write, false otherwise
 */
bool SynchronizedFS::writeFileAsync(const char *fname, const String *s, const QueueHandle_t completionQueue, const uint16_t completionId) const {
    if (s == nullptr || fname == nullptr) {
        log_error(F("Invalid parameters for async file write: fname=%p, s=%p"), fname, s);
        return false;
    }

    auto *request = makeOwnedStringRequest(FsRequest::Action::WRITE_FILE_ASYNC, fname, *s, completionQueue, completionId);
    if (request == nullptr) {
        log_error(F("Failed to create ASYNC request to write file - request pool likely full: fname=%s, s=%s"), fname, s->c_str());
        return false;
    }

    log_info(F("Sending WRITE_FILE_ASYNC message to filesystem task for file name %s"), request->path.c_str());
    return enqueueFsRequest(queue, request);
}

/**
 * Blocking function that appends the string content into a file with given name. Can be called from any task.
 * The file is created if it doesn't already exist (similar with writeFile); if it exists the content is appended at the end of the file.
 * @param fname file path to append to - does not have to exist
 * @param s content to write
 * @return true if successfully appended to, false otherwise
 */
size_t SynchronizedFS::appendFile(const char *fname, const String *s) const {
    auto *request = makeBorrowedRequest(FsRequest::Action::APPEND_FILE, xTaskGetCurrentTaskHandle(), fname, s, FsPayloadKind::STRING);
    if (request == nullptr) {
        log_error(F("Failed to create request to append file - request pool likely full: fname=%s"), fname);
        return 0;
    }
    log_info(F("Sending APPEND_FILE message to filesystem task for file name %s"), fname);

    size_t sz = 0;
    if (enqueueFsRequest(queue, request)) {
        //wait for the filesystem task to finish and notify us
        sz = waitForFsCompletion(FsRequest::Action::APPEND_FILE, fname, queue);
        if (sz == 0)
            log_error(F("Error appending file %s - error waiting for completion; size %zu"), fname, sz);
        else
            log_info(F("File %s appended successfully - size %zu"), fname, sz);
    }
    return sz;
}

size_t SynchronizedFS::appendFile(const char *fname, const uint8_t *buffer, const size_t size) const {
    auto *request = makeBorrowedRequest(FsRequest::Action::APPEND_FILE_BIN, xTaskGetCurrentTaskHandle(), fname, buffer, FsPayloadKind::BYTES, size);
    if (request == nullptr) {
        log_error(F("Failed to create request to append file - request pool likely full: fname=%s"), fname);
        return 0;
    }
    log_info(F("Sending APPEND_FILE_BIN message to filesystem task for file name %s for %zu bytes"), fname, size);

    size_t sz = 0;
    if (enqueueFsRequest(queue, request)) {
        //wait for the filesystem task to finish and notify us
        sz = waitForFsCompletion(FsRequest::Action::APPEND_FILE_BIN, fname, queue);
        if (sz == 0)
            log_error(F("Error appending file %s - error waiting for completion; size %zu"), fname, sz);
        else
            log_info(F("File %s appended successfully - size %zu"), fname, sz);
    }
    return sz;
}

/**
 * Blocking function that deletes a file with given name. Can be called from any task.
 * @param path file path to delete - absolute path
 * @return true if successfully deleted, false otherwise
 */
bool SynchronizedFS::remove(const char *path) {
    auto *request = makeBorrowedRequest(FsRequest::Action::DELETE, xTaskGetCurrentTaskHandle(), path);
    if (request == nullptr) {
        log_error(F("Failed to create DELETE request to delete file - request pool likely full: path=%s"), path);
        return false;
    }
    log_info(F("Sending DELETE_FILE message to filesystem task for file name %s"), path);

    bool success = false;
    if (enqueueFsRequest(queue, request)) {
        success = waitForFsCompletion(FsRequest::Action::DELETE, path, queue);
        if (!success)
            log_error(F("Error deleting file %s - error waiting for completion"), path);
        else
            log_info(F("File %s deleted successfully"), path);
    }
    return success;
}

/**
 * Rename a file/directory - blocking call, can be called from any task.
 * @param pathFrom old path to rename
 * @param pathTo new path name
 * @return true if rename was successful (old path exists, rename succeeded); false otherwise
 */
bool SynchronizedFS::rename(const char *pathFrom, const char *pathTo) {
    auto *request = makeOwnedStringRequest(FsRequest::Action::RENAME, pathFrom, String(pathTo ? pathTo : ""), nullptr, 0,
                        xTaskGetCurrentTaskHandle());
    if (request == nullptr) {
        log_error(F("Failed to create RENAME request to rename file - request pool likely full: pathFrom=%s, pathTo=%s"), pathFrom, pathTo);
        return false;
    }
    log_info(F("Sending RENAME message to filesystem task for file name %s"), pathFrom);

    bool success = false;
    if (enqueueFsRequest(queue, request)) {
        success = waitForFsCompletion(FsRequest::Action::RENAME, pathFrom, queue);
        if (!success)
            log_error(F("Error renaming file %s - error waiting for completion"), pathFrom);
        else
            log_info(F("File %s renamed successfully to %s"), pathFrom, pathTo);
    }
    return success;
}

/**
 * Blocking function that checks whether a file exists. Can be called from any task.
 * @param fname file path to check - absolute path
 * @return true if file exists, false otherwise
 */
bool SynchronizedFS::exists(const char *fname) {
    auto *request = makeBorrowedRequest(FsRequest::Action::EXISTS, xTaskGetCurrentTaskHandle(), fname);
    if (request == nullptr) {
        log_error(F("Failed to create EXISTS request to check file existence - request pool likely full: fname=%s"), fname);
        return false;
    }
    log_info(F("Sending FILE_EXISTS message to filesystem task for file name %s"), fname);

    bool exists = false;
    if (enqueueFsRequest(queue, request)) {
        exists = waitForFsCompletion(FsRequest::Action::EXISTS, fname, queue);
        if (!exists)
            log_info(F("File %s does not exist"), fname);
        else
            log_info(F("File %s exists"), fname);
    }
    return exists;
}

/**
 * Blocking function that formats the file system. Can be called from any task
 * @return true if successful
 */
bool SynchronizedFS::format() {
    auto *request = makeBorrowedRequest(FsRequest::Action::FORMAT, xTaskGetCurrentTaskHandle(), nullptr);
    if (request == nullptr) {
        log_error(F("Failed to create FORMAT request to format file system - request pool likely full"));
        return false;
    }
    log_info(F("Sending FORMAT message to filesystem task"));

    bool formatted = false;
    if (enqueueFsRequest(queue, request)) {
        formatted = waitForFsCompletion(FsRequest::Action::FORMAT, nullptr, queue);
        if (!formatted)
            log_error(F("Error formatting file system - error waiting for completion"));
        else
            log_info(F("File system formatted successfully"));
    }
    return formatted;
}

/**
 * Blocking function that traverses the file system entries recursively starting from a path, and calls
 * the given callback for each entry found (file and dir). Can be called from any task
 * @param path path to list files from (recursively)
 * @param list list to collect all file info
 */
bool SynchronizedFS::list(const char *path, std::deque<FileInfo> *list) const {
    auto *request = makeBorrowedRequest(FsRequest::Action::LIST_FILES, xTaskGetCurrentTaskHandle(), path, list, FsPayloadKind::FILE_INFO_LIST, 0, true);
    if (request == nullptr) {
        log_error(F("Failed to create LIST_FILES request to list files - request pool likely full: path=%s"), path);
        return false;
    }
    log_info(F("Sending LIST_FILES message to filesystem task for path %s"), path);

    bool completed = false;
    if (enqueueFsRequest(queue, request)) {
        completed = waitForFsCompletion(FsRequest::Action::LIST_FILES, path, queue);
        if (!completed)
            log_error(F("Error listing files from path %s - error waiting for completion"), path);
        else
            log_info(F("Listing files from path %s completed successfully"), path);
    }
    return completed;
}

/**
 * Blocking function that retrieves file information from the file system if it exists. Can be called from any task
 * @param path path to get info for
 * @param info file info object to populate
 * @return file information; if file doesn't exist the fields \code size\endcode and \code modTime\endcode are both 0
 */
bool SynchronizedFS::stat(const char *path, FileInfo *info) const {
    auto *request = makeBorrowedRequest(FsRequest::Action::INFO, xTaskGetCurrentTaskHandle(), path, info, FsPayloadKind::FILE_INFO, 0, true);
    if (request == nullptr) {
        log_error(F("Failed to create INFO request to retrieve file info - request pool likely full: path=%s"), path);
        return false;
    }
    log_info(F("Sending INFO message to filesystem task for path %s"), path);

    bool successful = false;
    if (enqueueFsRequest(queue, request)) {
        successful = waitForFsCompletion(FsRequest::Action::INFO, path, queue);
        if (!successful)
            log_error(F("Error retrieving file info for path %s - error waiting for completion"), path);
        else
            log_info(F("File info retrieved successfully for path %s"), path);
    }
    if (!successful)
        log_error(F("Failed to retrieve file info for path %s"), path);
    return successful;
}

bool SynchronizedFS::stat(const char *path, FSStat *st) {
    auto *request = makeBorrowedRequest(FsRequest::Action::STAT, xTaskGetCurrentTaskHandle(), path, st, FsPayloadKind::FS_STAT, 0, true);
    if (request == nullptr) {
        log_error(F("Failed to create STAT request to retrieve file info - request pool likely full: path=%s"), path);
        return false;
    }
    log_info(F("Sending STAT message to filesystem task for path %s"), path);

    bool successful = false;
    if (enqueueFsRequest(queue, request)) {
        successful = waitForFsCompletion(FsRequest::Action::STAT, path, queue);
        if (!successful)
            log_error(F("Error retrieving file info for path %s - error waiting for completion"), path);
        else
            log_info(F("File info retrieved successfully for path %s"), path);
    }
    if (!successful)
        log_error(F("Failed to retrieve file info for path %s"), path);
    return successful;
}

String SynchronizedFS::sha256(const char *path) const {
    String strSha2;
    strSha2.reserve(65);
    auto *request = makeBorrowedRequest(FsRequest::Action::SHA256, xTaskGetCurrentTaskHandle(), path, &strSha2, FsPayloadKind::STRING, 0, true);
    if (request == nullptr) {
        log_error(F("Failed to create SHA256 request to calculate file hash - request pool likely full: path=%s"), path);
        return "";
    }
    log_info(F("Sending SHA256 message to filesystem task for path %s"), path);

    bool successful = false;
    if (enqueueFsRequest(queue, request)) {
        successful = waitForFsCompletion(FsRequest::Action::SHA256, path, queue);
        if (!successful)
            log_error(F("Error calculating SHA-256 hash for path %s - error waiting for completion"), path);
        else
            log_info(F("SHA-256 hash calculated successfully for path %s"), path);
    }
    if (!successful)
        log_error(F("Failed to calculate SHA-256 hash for path %s (does not exist or not a file)"), path);
    return strSha2;
}

void SynchronizedFS::end() {
    Scheduler.stopTask(fsTask);
    if (fsPtr)
        fsPtr->end();
}

bool SynchronizedFS::info(FSInfo &info) {
    if (fsPtr)
        return fsPtr->info(info);
    return false;
}

bool SynchronizedFS::mkdir(const char *path) {
    auto *request = makeBorrowedRequest(FsRequest::Action::MAKE_DIR, xTaskGetCurrentTaskHandle(), path);
    bool success = false;
    if (enqueueFsRequest(queue, request)) {
        success = waitForFsCompletion(FsRequest::Action::MAKE_DIR, path, queue);
        if (!success)
            log_error(F("Error creating directory %s - error waiting for completion"), path);
        else
            log_info(F("Directory %s created successfully"), path);
    }
    return success;
}

bool SynchronizedFS::rmdir(const char *path) {
    return remove(path);
}

/**
 * Reads a text file - if it exists - into a string object. Private function, only to be called from the filesystem task
 * @param fname name of the file to read
 * @param s string to store contents into
 * @return number of characters in the file; 0 if file is empty or doesn't exist
 */
size_t SynchronizedFS::prvReadFile(const char *fname, String *s) const {
    if (!fname || !s) {
        log_error(F("Invalid parameters for file read: fname=%p, s=%p"), fname, s);
        return 0;
    }
    if (!fsPtr->exists(fname)) {
        log_error(F("Text file %s was not found/could not read"), fname);
        return 0;
    }
    File f = fsPtr->open(fname, "r");
    if (!f) {
        log_error(F("Failed to open file %s for reading - invalid file handle, possible FS corruption"), fname);
        return 0;
    }
    s->reserve(f.size());
    size_t fSize = 0;
    char buf[FILE_BUF_SIZE]{};
    while (const size_t charsRead = f.readBytes(buf, FILE_BUF_SIZE)) {
        s->concat(buf, charsRead);
        fSize += charsRead;
    }
    f.close();
    log_info(F("Read %d bytes from %s file"), fSize, fname);
    log_debug(F("Read file %s content [%zu]: %s"), fname, fSize, s->c_str());
    return fSize;
}

/**
 * Writes (overrides if already exists) a file using the string content. Private function, only to be called from the filesystem task
 * @param fname file name to write
 * @param s contents to write
 * @return number of bytes written
 */
size_t SynchronizedFS::prvWriteFile(const char *fname, const String *s) const {
    if (!fname || !s) {
        log_error(F("Invalid parameters for file write: fname=%p, s=%p"), fname, s);
        return 0;
    }
    size_t fSize = 0;
    File f = fsPtr->open(fname, "w");
    if (!f) {  // Integrity check: Ensure file handle is valid after open
        log_error(F("Failed to open file %s for writing - invalid file handle, possible FS corruption"), fname);
        return 0;
    }
    f.setTimeCallback(now);
    fSize = f.write(s->c_str(), s->length());
    const time_t lastWrite = f.getLastWrite();
    f.close();
    // Integrity check: Verify write completed fully (size matches expected)
    if (fSize != s->length()) {
        log_error(F("Write size mismatch for %s: expected %zu bytes, wrote %zu bytes - possible FS corruption or write failure"), fname, s->length(), fSize);
        return 0;  // Early return on mismatch to avoid logging invalid data
    }
    log_info(F("File %s - %zu bytes - has been saved at %s"), fname, fSize, TimeFormat::asString(lastWrite).c_str());
    log_debug(F("Saved file %s content [%zu]: %s"), fname, fSize, s->c_str());

    return fSize;
}

size_t SynchronizedFS::prvAppendFile(const char *fname, const String *s) const {
    if (!fname || !s) {
        log_error(F("Invalid parameters for file append: fname=%p, s=%p"), fname, s);
        return 0;
    }
    size_t fSize = 0;
    File f = fsPtr->open(fname, "a");
    if (!f) {
        log_error(F("Failed to open file %s for appending - invalid file handle, possible FS corruption"), fname);
        return 0;
    }
    f.setTimeCallback(now);
    fSize = f.write(s->c_str(), s->length());
    const time_t lastWrite = f.getLastWrite();  //get the current last write timestamp
    const size_t totalSize = f.size();
    f.close();
    // Integrity check: Verify write completed fully (size matches expected)
    if (fSize != s->length()) {
        log_error(F("Write size mismatch for appending %s: expected %zu bytes, wrote %zu bytes - possible FS corruption or write failure"), fname, s->length(), fSize);
        return 0;  // Early return on mismatch to avoid logging invalid data
    }

    log_info(F("File %s - size increased by %zu bytes to %zu bytes - has been saved at %s"), fname, fSize, totalSize, TimeFormat::asString(lastWrite).c_str());
    log_debug(F("Appended file %s content [%zu]: %s"), fname, fSize, s->c_str());
    return fSize;
}

size_t SynchronizedFS::prvAppendFile(const char *fname, const uint8_t *buffer, const size_t size) const {
    if (!fname || !buffer || size == 0) {
        log_error(F("Invalid parameters for file append: fname=%p, buffer=%p, size=%zu"), fname, buffer, size);
        return 0;
    }
    size_t fSize = 0;
    File f = fsPtr->open(fname, "a");
    if (!f) {
        log_error(F("Failed to open file %s for appending - invalid file handle, possible FS corruption"), fname);
        return 0;
    }
    f.setTimeCallback(now);
    fSize = f.write(buffer, size);
    const time_t lastWrite = f.getLastWrite();  //get the current last write timestamp
    const size_t totalSize = f.size();
    f.close();
    // Integrity check: Verify write completed fully (size matches expected)
    if (fSize != size) {
        log_error(F("Write size mismatch for appending %s: expected %zu bytes, wrote %zu bytes - possible FS corruption or write failure"), fname, size, fSize);
        return 0;  // Early return on mismatch to avoid logging invalid data
    }

    log_info(F("File %s (binary) - size increased by %zu bytes to %zu bytes - has been saved at %s"), fname, fSize, totalSize, TimeFormat::asString(lastWrite).c_str());
    log_debug(F("Appended file %s binary content %zu bytes"), fname, fSize);    //this is superfluous, perhaps logging binary content in hex would be helpful but quite a bit of overhead on flip side
    return fSize;
}

/**
 * Deletes a file with given path. Private function, only to be called from the filesystem task
 * @param path full file path to delete
 * @return true if file exist and deletion was successful or file does not exist (wrong path?); false if the deletion attempt failed
 */
bool SynchronizedFS::prvRemove(const char *path) const {
    if (!fsPtr->exists(path)) {
        //file does not exist - return true to the caller, the intent is already fulfilled
        log_info(F("File %s does not exist, no need to remove"), path);
        return true;
    }
    //TODO: does remove succeeds if path is a folder and it is not empty?
    const bool bDel = fsPtr->remove(path); //on LittleFS (core implementation of this filesystem) the remove dir and remove file are same call
    if (bDel)
        log_info(F("File %s successfully removed"), path);
    else
        log_error(F("File %s can NOT be removed"), path);
    return bDel;
}

bool SynchronizedFS::prvRename(const char *fromName, const String *toName) const {
    if (!fromName || !toName || toName->length() == 0) {
        log_error(F("Invalid parameters for file rename: fromName=%p, toName=%p, toName length=%zu"), fromName, toName, toName->length());
        return false;
    }
    if (!fsPtr->exists(fromName)) {
        log_error(F("File %s does not exist, no need to rename"), fromName);
        return false;
    }
    const bool bRenamed = fsPtr->rename(fromName, toName->c_str());
    if (bRenamed)
        log_info(F("File %s successfully renamed to %s"), fromName, toName);
    else
        log_error(F("File %s can NOT be renamed to %s"), fromName, toName);
    return bRenamed;
}

bool SynchronizedFS::prvExists(const char *path) const {
    return fsPtr->exists(path);
}

bool SynchronizedFS::prvFormat() const {
    fsPtr->end();
    const bool formatted = fsPtr->format();
    fsPtr->begin();
    return formatted;
}

bool SynchronizedFS::prvList(const char *path, std::deque<FileInfo> *fiList) const {
    if (!fiList || !path) {
        log_error(F("Invalid parameters for file list: fiList=%p, path=%p"), fiList, path);
        return false;
    }
    if (!fsPtr->exists(path)) {
        log_error(F("Path %s does not exist, no files listed"), path);
        return false;
    }
    String dirPath = path;
    auto captureFile = [&fiList](const FileInfo &info) { fiList->push_back(info); };
    listFiles(*fsPtr, dirPath, captureFile);
    return true;
}

bool SynchronizedFS::prvInfo(const char *path, FileInfo *fileInfo) const {
    if (!fsPtr->exists(path)) {
        log_error(F("File %s does not exist, no info retrieved"), path);
        return false;
    }
    fileInfo->name = StringUtils::fileName(path);
    fileInfo->path = StringUtils::fileDir(path);
    FSStat fsStat{};
    fsPtr->stat(path, &fsStat);
    fileInfo->size = fsStat.size;
    fileInfo->modTime = fsStat.ctime;
    fileInfo->isDir = fsStat.isDir;
    return true;
}

bool SynchronizedFS::prvStat(const char *path, FSStat *fs) const {
    if (!fsPtr->exists(path)) {
        log_error(F("Path %s does not exist, no stat retrieved"), path);
        return false;
    }
    fsPtr->stat(path, fs);
    return true;
}

bool SynchronizedFS::prvMakeDir(const char *path) const {
    return fsPtr->mkdir(path);
}

bool SynchronizedFS::prvSha256(const char *path, String *sha256) const {
    if (!path || !sha256) {
        log_error(F("Invalid parameters for file SHA256: path=%p, sha256=%p"), path, sha256);
        return false;
    }
    if (!fsPtr->exists(path)) {
        log_error(F("File %s does not exist, no SHA256 hash calculated"), path);
        return false;
    }
    const ulong start = millis();
    File f = fsPtr->open(path, "r");
    if (!f) {
        log_error(F("Failed to open file %s for reading - invalid file handle, possible FS corruption"), path);
        return false;
    }
    if (f.isDirectory()) {
        log_error(F("File %s is or a directory, no SHA-256 hash calculated"), path);
        f.close();
        return false;
    }
    pico_sha256_state_t* ctx = sha256_init();
    size_t fSize = 0;
    uint8_t buf[FILE_BUF_SIZE]{};
    while (const size_t charsRead = f.read(buf, FILE_BUF_SIZE)) {
        sha256_update(ctx, buf, charsRead);
        fSize += charsRead;
    }
    f.close();
    *sha256 = sha256_final(ctx);
    log_info(F("Read %d bytes from %s file, SHA-256 %s computed in %ldms"), fSize, path, sha256->c_str(), millis() - start);
    return true;
}
