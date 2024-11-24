// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include <LittleFS.h>
#include <FreeRTOS.h>
#include <queue.h>
#include "SchedulerExt.h"
#include "filesystem.h"
#include "sysinfo.h"
#include "util.h"
#include "log.h"

#define FILE_BUF_SIZE   256
#define FILE_OPERATIONS_TIMEOUT pdMS_TO_TICKS(1000)     //1 second file operations timeout (plenty time)

//ahead definitions
void fsExecute();
void fsInit();
size_t prvReadTextFile(const char *fname, String *s);
size_t prvWriteTextFile(const char *fname, String *s);
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
    enum Action:uint8_t {READ_FILE, WRITE_FILE, DELETE_FILE} event;
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
    fsDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
    fsTask = Scheduler.startTask(&fsDef);
    TaskStatus_t tStat;
    vTaskGetTaskInfo(fsTask->getTaskHandle(), &tStat, pdFALSE, eReady);
    Log.infoln(F("Filesystem task [%s] - priority %d - has been setup id %X. Events are dispatching."), tStat.pcTaskName, tStat.uxCurrentPriority, tStat.xTaskNumber);
}

/**
 * Initializes the filesystem objects and the task responsible for handling file access
 */
void fsInit() {
//    LittleFSConfig cfg(true);
//    LittleFS.setConfig(cfg);  //default is autoFormat = true as well
    if (LittleFS.begin()) {
        sysInfo->setSysStatus(SYS_STATUS_FILESYSTEM);
        Log.infoln(F("Filesystem OK"));
    }

#ifndef DISABLE_LOGGING
    FSInfo fsInfo{};
    if (LittleFS.info(fsInfo)) {
        Log.infoln(F("Filesystem information (size in bytes): totalSize %u, used %u, maxOpenFiles %d, maxPathLength %s, pageSize %d, blockSize %d"),
                   fsInfo.totalBytes, fsInfo.usedBytes, fsInfo.maxOpenFiles, fsInfo.maxPathLength, fsInfo.pageSize, fsInfo.blockSize);
    } else {
        Log.errorln(F("Cannot retrieve filesystem (LittleFS) information"));
    }
    Log.info(F("Root FS [/] contents:\n"));
    Dir d = LittleFS.openDir("/");
    while (d.next()) {
        if (d.isFile())
            Log.info(F("  %s [%d]\n"), d.fileName().c_str(), d.fileSize());
        else
            Log.info(F("  %s <DIR>\n"), d.fileName().c_str());
    }
    Log.endContinuation();
    Log.infoln(F("Dir complete."));
#endif
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
        case fsTaskMessage::DELETE_FILE:
            sz = prvRemoveFile(msg->data->name);
            xTaskNotify(msg->task, sz, eSetValueWithOverwrite);
            break;
        default:
            Log.errorln(F("Event type %d not supported"), msg->event);
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
        Log.errorln(F("Text file %s was not found/could not read"), fname);
        return 0;
    }
    File f = LittleFS.open(fname, "r");
    size_t fSize = 0;
    char buf[FILE_BUF_SIZE]{};
    size_t charsRead = 1;
    while (charsRead = f.readBytes(buf, FILE_BUF_SIZE)) {
        s->concat(buf, charsRead);
        fSize += charsRead;
    }
    f.close();
    Log.infoln(F("Read %d bytes from %s file"), fSize, fname);
    Log.traceln(F("Read file %s content [%d]: %s"), fname, fSize, s->c_str());
    return fSize;
}

/**
 * Writes (overrides if already exists) a file using the string content. Private function, only to be called from the filesystem task
 * @param fname file name to write
 * @param s contents to write
 * @return number of bytes written
 */
size_t prvWriteTextFile(const char *fname, String *s) {
    size_t fSize = 0;
    File f = LittleFS.open(fname, "w");
    fSize = f.write(s->c_str());
    f.close();
    Log.infoln(F("File %s has been saved, size %d bytes"), fname, fSize);
    Log.traceln(F("Saved file %s content [%d]: %s"), fname, fSize, s->c_str());
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
        Log.infoln(F("File %s does not exist, no need to remove"), fname);
        return true;
    }
    bool bDel = LittleFS.remove(fname);
    if (bDel)
        Log.infoln(F("File %s successfully removed"), fname);
    else
        Log.errorln(F("File %s can NOT be removed"), fname);
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

    BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        //wait for the filesystem task to finish and notify us
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.errorln(F("Error sending READ_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

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

    BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        //wait for the filesystem task to finish and notify us
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.errorln(F("Error sending WRITE_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

/**
 * Blocking function that deletes a file with given name. Can be called from any task.
 * @param fname file path to delete - absolute path
 * @return true if successfully deleted, false otherwise
 */
bool removeFile(const char *fname) {
    auto *args = new fsOperationData {fname, nullptr};
    auto *msg = new fsTaskMessage{fsTaskMessage::DELETE_FILE, xTaskGetCurrentTaskHandle(), args};

    BaseType_t qResult = xQueueSend(fsQueue, &msg, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    size_t sz = 0;
    if (qResult == pdTRUE) {
        sz = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FILE_OPERATIONS_TIMEOUT));
    } else
        Log.errorln(F("Error sending DELETE_FILE message to filesystem task for file name %s - error %d"), fname, qResult);

    delete msg;
    delete args;
    return sz;
}

