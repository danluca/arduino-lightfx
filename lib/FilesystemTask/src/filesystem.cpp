// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <LittleFS.h>
#include <queue.h>
#include <functional>
#include <TimeLib.h>
#include "SchedulerExt.h"
#include "filesystem.h"

#include <LogProxy.h>

#include "PicoLog.h"
#include "stringutils.h"

#define FILE_BUF_SIZE   512
#define MAX_DIR_LEVELS  10          // maximum number of directory levels to list (limits the recursion in the list function)
#define FILE_OPERATIONS_TIMEOUT pdMS_TO_TICKS(1000)     //1 second file operations timeout (plenty time)

#define OTA_COMMAND_FILE "/ota_command.bin"     // must match the _OTA_COMMAND_FILE name in the ../include/rp2040/pico_base/pico/ota_command.h
#define FW_BIN_FILE "/fw.bin"                   // must match the csFWImageFilename name in the app include/constants.hpp

SynchronizedFS SyncFsImpl;
FS SyncLittleFS(FSImplPtr(&SyncFsImpl));
static auto rootDir = FS_PATH_SEPARATOR;

//ahead definitions
void fsExecute();
void fsInit();

// filesystem task definition - priority is overwritten during setup, see fsSetup
TaskDef fsDef {fsInit, fsExecute, 1024, "FS", 1, CORE_0};

/**
 * Structure of filesystem function arguments - the filesystem task receives a pointer to this structure in the
 * message and it uses the data to call the actual methods involved
 */
struct fsOperationData {
    const char* const name;
    String* const content;
    void* const data;
    size_t size=0;
};

/**
 * Structure of the message sent to the filesystem task - internal use only
 */
struct fsTaskMessage {
    enum Action:uint8_t {READ_FILE, WRITE_FILE, WRITE_FILE_ASYNC, APPEND_FILE, APPEND_FILE_BIN, RENAME, DELETE, EXISTS, FORMAT, LIST_FIlES, INFO, STAT, MAKE_DIR, SHA256} event;
    TaskHandle_t task;
    fsOperationData* data;
};

/**
 * Lists all the files - recursively - from the directory provided for logging purposes
 * Maximum depth of recursive calls is limited to MAX_DIR_LEVELS (to protect from stack overflow)
 * @param fs the filesystem object
 * @param dir directory object to start from
 * @param s string that collects the listings
 * @param level the depth level of directory listing
 * @param callback callback to act on a directory entry as the tree is traversed
 */
void logFiles(FS& fs, Dir &dir, String &s, const uint8_t level, const std::function<void(Dir&)> &callback) {
    if (level/2 > MAX_DIR_LEVELS)
        return;    //prevent stack overflow - the recursion is limited to MAX_DIR_LEVELS levels (level increments by 2)
    while (dir.next()) {
        callback(dir);
        if (dir.isFile()) {
            File f = dir.openFile("r");
            String ts = StringUtils::asString(f.getLastWrite());
            f.close();
            StringUtils::append(s, F("%*c%s\t[%zu]  %s\n"), level, ' ', ts.c_str(), dir.fileSize(), dir.fileName().c_str());
        } else if (dir.isDirectory()) {
            String ts = StringUtils::asString(dir.fileCreationTime());
            StringUtils::append(s, F("%*c%s\t<DIR>  %s\n"), level, ' ', ts.c_str(), dir.fileName().c_str());
            Dir d = fs.openDir(dir.fileName());
            logFiles(fs, d, s, level+2, callback);
        } else
            StringUtils::append(s, F("%*c????\t??%zu?? %s\n"), level, ' ', dir.fileSize(), dir.fileName().c_str());
    }
}

/**
 * Lists all files recursively from the directory provided. No maximum level of depth for recursive calls is enforced.
 * Ensure enough stack is available if the provided structure has a deep nested structure
 * @param dir directory object to start from
 * @param path accumulated absolute path of the directory
 * @param callback callback to execute for each directory entry
 */
void listFiles(Dir &dir, String &path, const std::function<void(FileInfo*)> &callback) {
    while (dir.next()) {
        FileInfo fInfo {dir.fileName(), path, dir.fileSize(), dir.fileTime(), dir.isDirectory()};
        callback(&fInfo);
        if (fInfo.isDir) {
            path.concat(FS_PATH_SEPARATOR);
            path.concat(fInfo.name);
            Dir d = SyncFsImpl.fsPtr->openDir(path);
            listFiles(d, path, callback);
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
        Log.info(F("Filesystem OK"));

    if (FSInfo fsInfo{}; SyncFsImpl.fsPtr->info(fsInfo)) {
        Log.info(F("Filesystem information (size in bytes): totalSize %llu, used %llu, maxOpenFiles %zu, maxPathLength %zu, pageSize %zu, blockSize %zu"),
                   fsInfo.totalBytes, fsInfo.usedBytes, fsInfo.maxOpenFiles, fsInfo.maxPathLength, fsInfo.pageSize, fsInfo.blockSize);
    } else {
        Log.error(F("Cannot retrieve filesystem (LittleFS) information"));
    }

    // collect saved files under 64 bytes - a sign of corruption - and delete them
    std::queue<String> corruptedFiles;
    auto collectCorruptedFiles = [&l=corruptedFiles](Dir &d) {
        if (d.isFile() && d.fileSize() < 64) {
            l.push(d.fileName());
        }
    };

    String dirContent;
    dirContent.reserve(512);
    dirContent.concat(F("Filesystem content:\n"));
    Dir d = SyncFsImpl.fsPtr->openDir(rootDir);
    StringUtils::append(dirContent, F("%*c<ROOT-DIR> %s\n"), 2, ' ', d.fileName());
    logFiles(*SyncFsImpl.fsPtr, d, dirContent, 2, collectCorruptedFiles);
    dirContent.concat(F("End of filesystem content.\n"));

    if (!corruptedFiles.empty()) {
        StringUtils::append(dirContent, F("Found %d (likely) corrupted files (size < 64 bytes), deleting\n"), corruptedFiles.size());
        while (!corruptedFiles.empty()) {
            const bool removed = SyncFsImpl.prvRemove(corruptedFiles.front().c_str());
            corruptedFiles.pop();
        }
    }

    //check for otacommand.bin and fw.bin - this means a FW upgrade just occurred; delete the files
    if (SyncFsImpl.fsPtr->exists(OTA_COMMAND_FILE)) {
        log_info(F("=== FW Upgrade has completed!! Welcome to the other side! Cleaning up the FW files ==="));
        (void)SyncFsImpl.prvRemove(OTA_COMMAND_FILE);
        (void)SyncFsImpl.prvRemove(FW_BIN_FILE);
    }

    size_t sz = Log.info(dirContent.c_str());
}

/**
 * The task scheduler executes this function in a loop, no need to account for that here
 */
void fsExecute() {
    fsTaskMessage *msg = nullptr;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(SyncFsImpl.queue, &msg, portMAX_DELAY))
        return;
    //the reception was successful, hence the msg is not null anymore
    size_t sz = 0;
    bool success = false;
    switch (msg->event) {
        case fsTaskMessage::READ_FILE:
            sz = SyncFsImpl.prvReadFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::WRITE_FILE:
            sz = SyncFsImpl.prvWriteFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::WRITE_FILE_ASYNC:
            sz = SyncFsImpl.prvWriteFileAndFreeMem(msg->data->name, msg->data->content);
            if (!sz)
                Log.error(F("Failed to write file %s asynchronously. Data is still available, may lead to memory leaks."), msg->data->name);
            //dispose the messaging data
            delete msg->data;
            delete msg;
            break;
        case fsTaskMessage::APPEND_FILE:
            sz = SyncFsImpl.prvAppendFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::APPEND_FILE_BIN:
            sz = SyncFsImpl.prvAppendFile(msg->data->name, static_cast<uint8_t*>(msg->data->data), msg->data->size);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::DELETE:
            sz = SyncFsImpl.prvRemove(msg->data->name);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::RENAME:
            success = SyncFsImpl.prvRename(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::EXISTS:
            success = SyncFsImpl.prvExists(msg->data->name);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::FORMAT:
            success = SyncFsImpl.prvFormat();
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::LIST_FIlES:
            success = SyncFsImpl.prvList(msg->data->name, static_cast<std::deque<FileInfo *> *>(msg->data->data));
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::INFO:
            success = SyncFsImpl.prvInfo(msg->data->name, static_cast<FileInfo *>(msg->data->data));
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::STAT:
            success = SyncFsImpl.prvStat(msg->data->name, static_cast<FSStat *>(msg->data->data));
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::MAKE_DIR:
            success = SyncFsImpl.prvMakeDir(msg->data->name);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::SHA256:
            success = SyncFsImpl.prvSha256(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        default:
            Log.error(F("Event type %hd not supported"), msg->event);
            break;
    }
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
    queue = xQueueCreate(10, sizeof(fsTaskMessage*));
    //mirror the priority of the calling task - the filesystem task is intended to have the same priority
    fsDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle())+1;
    fsTask = Scheduler.startTask(&fsDef);
    TaskStatus_t tStat;
    vTaskGetTaskInfo(fsTask->getTaskHandle(), &tStat, pdFALSE, eReady);
    Log.info(F("Filesystem task [%s] - priority %d - has been setup id %u. Events are dispatching."), tStat.pcTaskName, tStat.uxCurrentPriority, tStat.xTaskNumber);
    return true;
}

/**
 * Blocking function that reads a text file leveraging the filesystem task. Can be called from any task.
 * @param fname file name to read
 * @param s content recipient
 * @return number of bytes read - 0 if file does not exist or cannot be read for some reason (e.g. timeout)
 */
size_t SynchronizedFS::readFile(const char *fname, String *s) const {
    auto *args = new fsOperationData {fname, s, nullptr};
    auto *msg = new fsTaskMessage {fsTaskMessage::READ_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        //wait for the filesystem task to finish and notify us
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending READ_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

/**
 * Blocking function that writes the string content into a file with provided name. Can be called from any task.
 * @param fname file name to write into
 * @param s content to write
 * @return number of bytes written - 0 if there was an error (e.g. timeout)
 */
size_t SynchronizedFS::writeFile(const char *fname, String *s) const {
    auto *args = new fsOperationData {fname, s, nullptr};
    auto *msg = new fsTaskMessage {fsTaskMessage::WRITE_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        //wait for the filesystem task to finish and notify us
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending WRITE_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

/**
 * Non-blocking function that writes the string content into a file with provided name. Can be called from any task.
 * @param fname file path to write - absolute path
 * @param s the content to write
 * @return true if successfully enqueued to write, false otherwise
 */
bool SynchronizedFS::writeFileAsync(const char *fname, String *s) const {
    auto *args = new fsOperationData {fname, s, nullptr};
    auto *msg = new fsTaskMessage {fsTaskMessage::WRITE_FILE_ASYNC, nullptr, args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    if (qResult != pdTRUE) {
        Log.error(F("Error sending WRITE_FILE_ASYNC message to filesystem task for file name %s - error %d"), fname, qResult);
        delete msg;
        delete args;
    }
    return qResult == pdTRUE;
}

/**
 * Blocking function that appends the string content into a file with given name. Can be called from any task.
 * The file is created if it doesn't already exist (similar with writeFile); if it exists the content is appended at the end of the file.
 * @param fname file path to append to - does not have to exist
 * @param s content to write
 * @return true if successfully appended to, false otherwise
 */
size_t SynchronizedFS::appendFile(const char *fname, String *s) const {
    auto *args = new fsOperationData {fname, s, nullptr};
    auto *msg = new fsTaskMessage {fsTaskMessage::APPEND_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        //wait for the filesystem task to finish and notify us
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending APPEND_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

size_t SynchronizedFS::appendFile(const char *fname, uint8_t *buffer, const size_t size) const {
    auto *args = new fsOperationData {fname, nullptr, buffer, size};
    auto *msg = new fsTaskMessage {fsTaskMessage::APPEND_FILE_BIN, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        //wait for the filesystem task to finish and notify us
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending APPEND_FILE_BIN message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

/**
 * Blocking function that deletes a file with given name. Can be called from any task.
 * @param path file path to delete - absolute path
 * @return true if successfully deleted, false otherwise
 */
bool SynchronizedFS::remove(const char *path) {
    auto *args = new fsOperationData {path, nullptr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::DELETE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool success = false;
    if (qResult == pdTRUE) {
        success = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending DELETE_FILE message to filesystem task for file name %s - error %d"), path, qResult);

    delete msg;
    delete args;
    return success;
}

/**
 * Rename a file/directory - blocking call, can be called from any task.
 * @param pathFrom old path to rename
 * @param pathTo new path name
 * @return true if rename was successful (old path exists, rename succeeded); false otherwise
 */
bool SynchronizedFS::rename(const char *pathFrom, const char *pathTo) {
    String pathToStr(pathTo);
    auto *args = new fsOperationData {pathFrom, &pathToStr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::RENAME, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool success = false;
    if (qResult == pdTRUE) {
        success = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending RENAME message to filesystem task for file name %s - error %d"), pathFrom, qResult);

    delete msg;
    delete args;
    return success;
}

/**
 * Blocking function that checks whether a file exists. Can be called from any task.
 * @param fname file path to check - absolute path
 * @return true if file exists, false otherwise
 */
bool SynchronizedFS::exists(const char *fname) {
    auto *args = new fsOperationData {fname, nullptr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::EXISTS, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool exists = false;
    if (qResult == pdTRUE) {
        exists = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending FILE_EXISTS message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return exists;
}

/**
 * Blocking function that formats the file system. Can be called from any task
 * @return true if successful
 */
bool SynchronizedFS::format() {
    auto *args = new fsOperationData {nullptr, nullptr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::FORMAT, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool formatted = false;
    if (qResult == pdTRUE) {
        formatted = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending FORMAT message to filesystem task - error %d"), qResult);

    delete msg;
    delete args;
    return formatted;
}

/**
 * Blocking function that traverses the file system entries recursively starting from a path, and calls
 * the given callback for each entry found (file and dir). Can be called from any task
 * @param path path to list files from (recursively)
 * @param list list to collect all file info
 */
bool SynchronizedFS::list(const char *path, std::deque<FileInfo*> *list) const {
    auto *args = new fsOperationData {path, nullptr, list};
    auto *msg = new fsTaskMessage{fsTaskMessage::LIST_FIlES, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool completed = false;
    if (qResult == pdTRUE) {
        completed = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending LIST_FIlES message to filesystem task for path %s - error %d"), path, qResult);

    delete msg;
    delete args;
    return completed;
}

/**
 * Blocking function that retrieves file information from the file system if it exists. Can be called from any task
 * @param path path to get info for
 * @param info file info object to populate
 * @return file information; if file doesn't exist the fields \code size\endcode and \code modTime\endcode are both 0
 */
bool SynchronizedFS::stat(const char *path, FileInfo *info) const {
    auto *args = new fsOperationData {path, nullptr, info};
    auto *msg = new fsTaskMessage{fsTaskMessage::INFO, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool successful = false;
    if (qResult == pdTRUE)
        successful = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    else
        Log.error(F("Error sending INFO message to filesystem task for path %s - error %d"), path, qResult);
    if (!successful)
        Log.error(F("Failed to retrieve file info for path %s"), path);
    delete msg;
    delete args;
    return successful;
}

bool SynchronizedFS::stat(const char *path, FSStat *st) {
    auto *args = new fsOperationData {path, nullptr, st};
    auto *msg = new fsTaskMessage{fsTaskMessage::STAT, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool successful = false;
    if (qResult == pdTRUE)
        successful = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    else
        Log.error(F("Error sending STAT message to filesystem task for path %s - error %d"), path, qResult);
    if (!successful)
        Log.error(F("Failed to retrieve file info for path %s"), path);
    delete msg;
    delete args;
    return successful;
}

String SynchronizedFS::sha256(const char *path) const {
    String strSha2;
    strSha2.reserve(65);
    auto *args = new fsOperationData {path, &strSha2, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::SHA256, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool successful = false;
    if (qResult == pdTRUE)
        successful = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    else
        Log.error(F("Error sending SHA256 message to filesystem task for path %s - error %d"), path, qResult);
    if (!successful)
        Log.error(F("Failed to calculate SHA-256 hash for path %s (does not exist or not a file)"), path);
    delete msg;
    delete args;
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
    auto *args = new fsOperationData {path, nullptr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::MAKE_DIR, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool success = false;
    if (qResult == pdTRUE) {
        success = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending MAKE_DIR message to filesystem task for path name %s - error %d"), path, qResult);

    delete msg;
    delete args;
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
    if (!fsPtr->exists(fname)) {
        Log.error(F("Text file %s was not found/could not read"), fname);
        return 0;
    }
    File f = fsPtr->open(fname, "r");
    size_t fSize = 0;
    char buf[FILE_BUF_SIZE]{};
    while (const size_t charsRead = f.readBytes(buf, FILE_BUF_SIZE)) {
        s->concat(buf, charsRead);
        fSize += charsRead;
    }
    f.close();
    Log.info(F("Read %d bytes from %s file"), fSize, fname);
    Log.trace(F("Read file %s content [%zu]: %s"), fname, fSize, s->c_str());
    return fSize;
}

/**
 * Writes (overrides if already exists) a file using the string content. Private function, only to be called from the filesystem task
 * @param fname file name to write
 * @param s contents to write
 * @return number of bytes written
 */
size_t SynchronizedFS::prvWriteFile(const char *fname, const String *s) const {
    size_t fSize = 0;
    File f = fsPtr->open(fname, "w");
    f.setTimeCallback(now);
    fSize = f.write(s->c_str(), s->length());
    f.close();
    //get the current last write timestamp - note that stat function does not make the distinction between creation and last access time in LittleFS; we'll use file API
    // FSStat fstat{};
    // fsPtr->stat(fname, &fstat);
    f = fsPtr->open(fname, "r");
    const time_t lastWrite = f.getLastWrite();
    f.close();

    Log.info(F("File %s - %zu bytes - has been saved at %s"), fname, fSize, StringUtils::asString(lastWrite).c_str());
    Log.trace(F("Saved file %s content [%zu]: %s"), fname, fSize, s->c_str());
    return fSize;
}

size_t SynchronizedFS::prvAppendFile(const char *fname, const String *s) const {
    size_t fSize = 0;
    File f = fsPtr->open(fname, "a");
    f.setTimeCallback(now);
    fSize = f.write(s->c_str(), s->length());
    const time_t lastWrite = f.getLastWrite();  //get the current last write timestamp
    const size_t totalSize = f.size();
    f.close();

    Log.info(F("File %s - size increased by %zu bytes to %zu bytes - has been saved at %s"), fname, fSize, totalSize, StringUtils::asString(lastWrite).c_str());
    Log.trace(F("Appended file %s content [%zu]: %s"), fname, fSize, s->c_str());
    return fSize;
}

size_t SynchronizedFS::prvAppendFile(const char *fname, const uint8_t *buffer, const size_t size) const {
    size_t fSize = 0;
    File f = fsPtr->open(fname, "a");
    f.setTimeCallback(now);
    fSize = f.write(buffer, size);
    const time_t lastWrite = f.getLastWrite();  //get the current last write timestamp
    const size_t totalSize = f.size();
    f.close();

    Log.info(F("File %s (binary) - size increased by %zu bytes to %zu bytes - has been saved at %s"), fname, fSize, totalSize, StringUtils::asString(lastWrite).c_str());
    Log.trace(F("Appended file %s binary content %zu bytes"), fname, fSize);    //this is superfluous, perhaps logging binary content in hex would be helpful but quite a bit of overhead on flip side
    return fSize;
}

size_t SynchronizedFS::prvWriteFileAndFreeMem(const char *fname, const String *s) const {
    const size_t fSize = prvWriteFile(fname, s);
    if (fSize)
        delete s;
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
        Log.info(F("File %s does not exist, no need to remove"), path);
        return true;
    }
    //TODO: does remove succeeds if path is a folder and it is not empty?
    const bool bDel = fsPtr->remove(path); //on LittleFS (core implementation of this filesystem) the remove dir and remove file are same call
    if (bDel)
        Log.info(F("File %s successfully removed"), path);
    else
        Log.error(F("File %s can NOT be removed"), path);
    return bDel;
}

bool SynchronizedFS::prvRename(const char *fromName, const String *toName) const {
    if (!fsPtr->exists(fromName)) {
        Log.error(F("File %s does not exist, no need to rename"), fromName);
        return false;
    }
    const bool bRenamed = fsPtr->rename(fromName, toName->c_str());
    if (bRenamed)
        Log.info(F("File %s successfully renamed to %s"), fromName, toName);
    else
        Log.error(F("File %s can NOT be renamed to %s"), fromName, toName);
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

bool SynchronizedFS::prvList(const char *path, std::deque<FileInfo *> *fiList) const {
    Dir d = fsPtr->openDir(path);
    String dirPath = path;
    auto captureFile = [&fiList](FileInfo *info) {fiList->push_back(info);};
    listFiles(d, dirPath, captureFile);
    return true;
}

bool SynchronizedFS::prvInfo(const char *path, FileInfo *fileInfo) const {
    if (!fsPtr->exists(path)) {
        Log.error(F("File %s does not exist, no info retrieved"), path);
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
        Log.error(F("Path %s does not exist, no stat retrieved"), path);
        return false;
    }
    fsPtr->stat(path, fs);
    return true;
}

bool SynchronizedFS::prvMakeDir(const char *path) const {
    return fsPtr->mkdir(path);
}

bool SynchronizedFS::prvSha256(const char *path, String *sha256) const {
    if (!fsPtr->exists(path)) {
        Log.error(F("File %s does not exist, no SHA256 hash calculated"), path);
        return false;
    }
    const ulong start = millis();
    File f = fsPtr->open(path, "r");
    if (f.isDirectory()) {
        Log.error(F("File %s is a directory, no SHA-256 hash calculated"), path);
        return false;
    }
    br_sha256_context* ctx = sha256_init();
    size_t fSize = 0;
    uint8_t buf[FILE_BUF_SIZE]{};
    while (const size_t charsRead = f.read(buf, FILE_BUF_SIZE)) {
        sha256_update(ctx, buf, charsRead);
        fSize += charsRead;
    }
    f.close();
    *sha256 = sha256_final(ctx);
    Log.info(F("Read %d bytes from %s file, SHA-256 %s computed in %ldms"), fSize, path, sha256->c_str(), millis() - start);
    return true;
}
