// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <LittleFS.h>
#include <queue.h>
#include <functional>
#include <TimeLib.h>
#include "SchedulerExt.h"
#include "filesystem.h"
#include "sysinfo.h"
#include "util.h"
#include "log.h"
#include "stringutils.h"

#define FILE_BUF_SIZE   256
#define MAX_DIR_LEVELS  10          // maximum number of directory levels to list (limits the recursion in the list function)
#define FILE_OPERATIONS_TIMEOUT pdMS_TO_TICKS(1000)     //1 second file operations timeout (plenty time)

//ahead definitions
void fsExecute();
void fsInit();
size_t prvReadTextFile(const char *fname, String *s);
size_t prvWriteTextFile(const char *fname, const String *s);
size_t prvWriteTextFileAndFreeMem(const char *fname, String *s);
bool prvRemoveFile(const char *fname);

// filesystem task definition - priority is overwritten during setup, see fsSetup
TaskDef fsDef {fsInit, fsExecute, 1024, "FS", 1, CORE_0};

//task communication primitives
QueueHandle_t fsQueue;
TaskWrapper* fsTask;

/**
 * Structure of filesystem function arguments - the filesystem task receives a pointer to this structure in the
 * message and it uses the data to call the actual methods involved
 */
struct fsOperationData {
    const char* const name;
    String* const content;
};

/**
 * Structure of the message sent to the filesystem task - internal use only
 */
struct fsTaskMessage {
    enum Action:uint8_t {READ_FILE, WRITE_FILE, DELETE_FILE, WRITE_FILE_ASYNC} event;
    TaskHandle_t task;
    fsOperationData* data;
};

/**
 * See https://arduino-pico.readthedocs.io/en/latest/freertos.html#caveats - the LittleFS is not thread safe and
 * recommended to do all file operations in a single thread
 */
void fsSetup() {
    fsQueue = xQueueCreate(10, sizeof(fsTaskMessage*));
    //mirror the priority of the calling task - the filesystem task is intended to have the same priority
    fsDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle())+1;
    fsTask = Scheduler.startTask(&fsDef);
    TaskStatus_t tStat;
    vTaskGetTaskInfo(fsTask->getTaskHandle(), &tStat, pdFALSE, eReady);
    Log.info(F("Filesystem task [%s] - priority %d - has been setup id %u. Events are dispatching."), tStat.pcTaskName, tStat.uxCurrentPriority, tStat.xTaskNumber);
}

/**
 * Lists all the files - recursively - from the directory provided, using LittleFS
 * @param dir directory object to start from
 * @param s string that collects the listings
 * @param level the depth level of directory listing
 * @param callback callback to act on a directory entry as the tree is traversed
 */
void listFiles(Dir &dir, String &s, const uint8_t level, const std::function<void(Dir&)> &callback) {
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
            Dir d = LittleFS.openDir(dir.fileName());
            listFiles(d, s, level+2, callback);
        } else
            StringUtils::append(s, F("%*c????\t??%zu?? %s\n"), level, ' ', dir.fileSize(), dir.fileName().c_str());
    }
}

time_t fileTime() {
    return now();
}

/**
 * Initializes the filesystem objects and the task responsible for handling file access
 */
void fsInit() {
//    LittleFSConfig cfg(true);
//    LittleFS.setConfig(cfg);  //default is autoFormat = true as well
    LittleFS.setTimeCallback(fileTime);
    if (LittleFS.begin()) {
        sysInfo->setSysStatus(SYS_STATUS_FILESYSTEM);
        Log.info(F("Filesystem OK"));
    }

    if (FSInfo fsInfo{}; LittleFS.info(fsInfo)) {
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
    Dir d = LittleFS.openDir("/");
    StringUtils::append(dirContent, F("%*c<ROOT-DIR> %s\n"), 2, ' ', d.fileName());
    listFiles(d, dirContent, 2, collectCorruptedFiles);
    dirContent.concat(F("End of filesystem content.\n"));

    if (!corruptedFiles.empty()) {
        StringUtils::append(dirContent, F("Found %d (likely) corrupted files (size < 64 bytes), deleting\n"), corruptedFiles.size());
        while (!corruptedFiles.empty()) {
            prvRemoveFile(corruptedFiles.front().c_str());
            corruptedFiles.pop();
        }
    }

    Log.info(dirContent.c_str());
}

/**
 * The task scheduler executes this function in a loop, no need to account for that here
 */
void fsExecute() {
    fsTaskMessage *msg = nullptr;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(fsQueue, &msg, portMAX_DELAY))
        return;
    //the reception was successful, hence the msg is not null anymore
    size_t sz = 0;
    switch (msg->event) {
        case fsTaskMessage::READ_FILE:
            sz = prvReadTextFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::WRITE_FILE:
            sz = prvWriteTextFile(msg->data->name, msg->data->content);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        case fsTaskMessage::WRITE_FILE_ASYNC:
            sz = prvWriteTextFileAndFreeMem(msg->data->name, msg->data->content);
            if (!sz)
                Log.error(F("Failed to write file %s asynchronously. Data has is still available, may lead to memory leaks."), msg->data->name);
            //dispose the messaging data
            delete msg->data;
            delete msg;
            break;
        case fsTaskMessage::DELETE_FILE:
            sz = prvRemoveFile(msg->data->name);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        default:
            Log.error(F("Event type %hd not supported"), msg->event);
            break;
    }
}

/**
 * Reads a text file - if it exists - into a string object. Private function, only to be called from the filesystem task
 * @param fname name of the file to read
 * @param s string to store contents into
 * @return number of characters in the file; 0 if file is empty or doesn't exist
 */
size_t prvReadTextFile(const char *fname, String *s) {
    if (!LittleFS.exists(fname)) {
        Log.error(F("Text file %s was not found/could not read"), fname);
        return 0;
    }
    File f = LittleFS.open(fname, "r");
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
size_t prvWriteTextFile(const char *fname, const String *s) {
    size_t fSize = 0;
    File f = LittleFS.open(fname, "w");
    f.setTimeCallback(fileTime);
    fSize = f.write(s->c_str());
    f.close();
    //get the current last write timestamp
    f = LittleFS.open(fname, "r");
    const time_t lastWrite = f.getLastWrite();
    f.close();

    Log.info(F("File %s - %zu bytes - has been saved at %s"), fname, fSize, StringUtils::asString(lastWrite).c_str());
    Log.trace(F("Saved file %s content [%zu]: %s"), fname, fSize, s->c_str());
    return fSize;
}

size_t prvWriteTextFileAndFreeMem(const char *fname, String *s) {
    const size_t fSize = prvWriteTextFile(fname, s);
    if (fSize)
        delete s;
    return fSize;
}

/**
 * Deletes a file with given path. Private function, only to be called from the filesystem task
 * @param fname full file path to delete
 * @return true if file exist and deletion was successful or file does not exist (wrong path?); false if the deletion attempt failed
 */
bool prvRemoveFile(const char *fname) {
    if (!LittleFS.exists(fname)) {
        //file does not exist - return true to the caller, the intent is already fulfilled
        Log.info(F("File %s does not exist, no need to remove"), fname);
        return true;
    }
    const bool bDel = LittleFS.remove(fname);
    if (bDel)
        Log.info(F("File %s successfully removed"), fname);
    else
        Log.error(F("File %s can NOT be removed"), fname);
    return bDel;
}

/**
 * Blocking function that reads a text file leveraging the filesystem task. Can be called from any task.
 * @param fname file name to read
 * @param s content recipient
 * @return number of bytes read - 0 if file does not exist or cannot be read for some reason (e.g. timeout)
 */
size_t readTextFile(const char *fname, String *s) {
    auto *args = new fsOperationData {fname, s};
    auto *msg = new fsTaskMessage {fsTaskMessage::READ_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
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
size_t writeTextFile(const char *fname, String *s) {
    auto *args = new fsOperationData {fname, s};
    auto *msg = new fsTaskMessage {fsTaskMessage::WRITE_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
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
bool writeTextFileAsync(const char *fname, String *s) {
    auto *args = new fsOperationData {fname, s};
    auto *msg = new fsTaskMessage {fsTaskMessage::WRITE_FILE_ASYNC, nullptr, args};

    const BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
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
bool removeFile(const char *fname) {
    auto *args = new fsOperationData {fname, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::DELETE_FILE, xTaskGetCurrentTaskHandle(), args};

    const BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.error(F("Error sending DELETE_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

