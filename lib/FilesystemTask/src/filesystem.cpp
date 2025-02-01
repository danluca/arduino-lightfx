// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <LittleFS.h>
#include <queue.h>
#include <functional>
#include <TimeLib.h>
#include "SchedulerExt.h"
#include "filesystem.h"
#include "../../PicoLog/src/PicoLog.h"
#include "../../StringUtils/src/stringutils.h"

#define FILE_BUF_SIZE   256
#define MAX_DIR_LEVELS  10          // maximum number of directory levels to list (limits the recursion in the list function)
#define FILE_OPERATIONS_TIMEOUT pdMS_TO_TICKS(1000)     //1 second file operations timeout (plenty time)

SynchronizedFS SyncFs;
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
    const std::function<void(const FileInfo&)> &callback;
};

/**
 * Structure of the message sent to the filesystem task - internal use only
 */
struct fsTaskMessage {
    enum Action:uint8_t {READ_FILE, WRITE_FILE, DELETE_FILE, WRITE_FILE_ASYNC, FILE_EXISTS, FORMAT, LIST_FIlES, FILE_INFO} event;
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
void listFiles(Dir &dir, String &path, const std::function<void(const FileInfo&)> &callback) {
    while (dir.next()) {
        FileInfo fInfo {dir.fileName(), path, dir.fileSize(), dir.fileTime(), dir.isDirectory()};
        callback(fInfo);
        if (fInfo.isDir) {
            path.concat(FS_PATH_SEPARATOR);
            path.concat(fInfo.name);
            Dir d = SyncFs.fsPtr->openDir(path);
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
    SyncFs.fsPtr->setTimeCallback(now);
    if (SyncFs.fsPtr->begin())
        Log.info(F("Filesystem OK"));

    if (FSInfo fsInfo{}; SyncFs.fsPtr->info(fsInfo)) {
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
    Dir d = SyncFs.fsPtr->openDir(rootDir);
    StringUtils::append(dirContent, F("%*c<ROOT-DIR> %s\n"), 2, ' ', d.fileName());
    logFiles(*SyncFs.fsPtr, d, dirContent, 2, collectCorruptedFiles);
    dirContent.concat(F("End of filesystem content.\n"));

    if (!corruptedFiles.empty()) {
        StringUtils::append(dirContent, F("Found %d (likely) corrupted files (size < 64 bytes), deleting\n"), corruptedFiles.size());
        while (!corruptedFiles.empty()) {
            const bool removed = SyncFs.prvRemove(corruptedFiles.front().c_str());
            corruptedFiles.pop();
        }
    }

    size_t sz = Log.info(dirContent.c_str());
}

/**
 * The task scheduler executes this function in a loop, no need to account for that here
 */
void fsExecute() {
    fsTaskMessage *msg = nullptr;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(SyncFs.queue, &msg, portMAX_DELAY))
        return;
    //the reception was successful, hence the msg is not null anymore
    size_t sz = 0;
    bool success = false;
    switch (msg->event) {
        case fsTaskMessage::READ_FILE:
            sz = SyncFs.prvReadFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::WRITE_FILE:
            sz = SyncFs.prvWriteFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::WRITE_FILE_ASYNC:
            sz = SyncFs.prvWriteFileAndFreeMem(msg->data->name, msg->data->content);
            if (!sz)
                Log.error(F("Failed to write file %s asynchronously. Data is still available, may lead to memory leaks."), msg->data->name);
            //dispose the messaging data
            delete msg->data;
            delete msg;
            break;
        case fsTaskMessage::DELETE_FILE:
            sz = SyncFs.prvRemove(msg->data->name);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::FILE_EXISTS:
            success = SyncFs.prvExists(msg->data->name);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::FORMAT:
            success = SyncFs.prvFormat();
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::LIST_FIlES:
            success = SyncFs.prvList(msg->data->name, msg->data->callback);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::FILE_INFO:
            success = SyncFs.prvInfo(msg->data->name, msg->data->callback);
            xTaskNotify(msg->task, success, eSetValueWithOverwrite);
            break;
        default:
            Log.error(F("Event type %hd not supported"), msg->event);
            break;
    }
}

SynchronizedFS::SynchronizedFS() = default;

SynchronizedFS::~SynchronizedFS() {
    if (fsPtr)
        fsPtr->end();
}

/**
 * See https://arduino-pico.readthedocs.io/en/latest/freertos.html#caveats - the LittleFS is not thread safe and
 * recommended to do all file operations in a single thread
 * This method can be called from any task
 */
void SynchronizedFS::begin(FS &fs) {
    fsPtr = &fs;
    queue = xQueueCreate(10, sizeof(fsTaskMessage*));
    //mirror the priority of the calling task - the filesystem task is intended to have the same priority
    fsDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle())+1;
    fsTask = Scheduler.startTask(&fsDef);
    TaskStatus_t tStat;
    vTaskGetTaskInfo(fsTask->getTaskHandle(), &tStat, pdFALSE, eReady);
    Log.info(F("Filesystem task [%s] - priority %d - has been setup id %u. Events are dispatching."), tStat.pcTaskName, tStat.uxCurrentPriority, tStat.xTaskNumber);
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
 * @param fname file path to delete - absolute path
 * @param s the content to write
 * @return true if successfully deleted, false otherwise
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
 * Blocking function that deletes a file with given name. Can be called from any task.
 * @param fname file path to delete - absolute path
 * @return true if successfully deleted, false otherwise
 */
bool SynchronizedFS::remove(const char *fname) const {
    auto *args = new fsOperationData {fname, nullptr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::DELETE_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool success = false;
    if (qResult == pdTRUE) {
        success = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending DELETE_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return success;
}

/**
 * Blocking function that checks whether a file exists. Can be called from any task.
 * @param fname file path to check - absolute path
 * @return true if file exists, false otherwise
 */
bool SynchronizedFS::exists(const char *fname) const {
    auto *args = new fsOperationData {fname, nullptr, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::FILE_EXISTS, xTaskGetCurrentTaskHandle(), args};

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
bool SynchronizedFS::format() const {
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
 * @param callback callback for each file/dir encountered - note the callback will be executed on the filesystem task
 */
void SynchronizedFS::list(const char *path, const std::function<void(const FileInfo&)> &callback) const {
    auto *args = new fsOperationData {path, nullptr, callback};
    auto *msg = new fsTaskMessage{fsTaskMessage::LIST_FIlES, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool completed = false;
    if (qResult == pdTRUE) {
        completed = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending LIST_FIlES message to filesystem task for path %s - error %d"), path, qResult);

    delete msg;
    delete args;
}

/**
 * Blocking function that retrieves file information from the file system if it exists. Can be called from any task
 * @param path path to get info for
 * @return file information; if file doesn't exist the fields {@code size} and {@code modTime} are both 0
 */
FileInfo SynchronizedFS::info(const char *path) const {
    FileInfo fInfo{};
    auto catchInfo = [&fInfo](const FileInfo& f) {fInfo = f;};
    auto *args = new fsOperationData {path, nullptr, catchInfo};
    auto *msg = new fsTaskMessage{fsTaskMessage::FILE_INFO, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(queue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    bool successful = false;
    if (qResult == pdTRUE)
        successful = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    else
        Log.error(F("Error sending FILE_INFO message to filesystem task for path %s - error %d"), path, qResult);
    if (!successful)
        Log.error(F("Failed to retrieve file info for path %s"), path);
    delete msg;
    delete args;
    return fInfo;
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
    fSize = f.write(s->c_str());
    f.close();
    //get the current last write timestamp
    f = fsPtr->open(fname, "r");
    const time_t lastWrite = f.getLastWrite();
    f.close();

    Log.info(F("File %s - %zu bytes - has been saved at %s"), fname, fSize, StringUtils::asString(lastWrite).c_str());
    Log.trace(F("Saved file %s content [%zu]: %s"), fname, fSize, s->c_str());
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
 * @param fname full file path to delete
 * @return true if file exist and deletion was successful or file does not exist (wrong path?); false if the deletion attempt failed
 */
bool SynchronizedFS::prvRemove(const char *fname) const {
    if (!fsPtr->exists(fname)) {
        //file does not exist - return true to the caller, the intent is already fulfilled
        Log.info(F("File %s does not exist, no need to remove"), fname);
        return true;
    }
    const bool bDel = fsPtr->remove(fname);
    if (bDel)
        Log.info(F("File %s successfully removed"), fname);
    else
        Log.error(F("File %s can NOT be removed"), fname);
    return bDel;
}

bool SynchronizedFS::prvExists(const char *fname) const {
    return fsPtr->exists(fname);
}

bool SynchronizedFS::prvFormat() const {
    fsPtr->end();
    const bool formatted = fsPtr->format();
    fsPtr->begin();
    return formatted;
}

bool SynchronizedFS::prvList(const char *path, const std::function<void(const FileInfo&)> &callback) const {
    Dir d = fsPtr->openDir(path);
    String dirPath = path;
    listFiles(d, dirPath, callback);
    return true;
}

bool SynchronizedFS::prvInfo(const char *path, const std::function<void(const FileInfo&)> &callback) const {
    if (!fsPtr->exists(path)) {
        Log.error(F("File %s does not exist, no info retrieved"), path);
        return false;
    }
    FileInfo fInfo{};
    fInfo.name = StringUtils::fileName(path);
    fInfo.path = StringUtils::fileDir(path);
    FSStat fsStat{};
    fsPtr->stat(path, &fsStat);
    fInfo.size = fsStat.size;
    fInfo.modTime = fsStat.ctime;
    fInfo.isDir = fsStat.isDir;
    callback(fInfo);
    return true;
}

