// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2040_LIGHTFX_EARLE_FILESYSTEM_H
#define RP2040_LIGHTFX_EARLE_FILESYSTEM_H

#include <Arduino.h>
#include <FS.h>
#include <functional>
#include <deque>
#include "SchedulerExt.h"
#include "fs_sha256.h"

inline constexpr auto FS_PATH_SEPARATOR PROGMEM = "/";
using namespace fs;

/**
 * File Information - name, directory, size, last modified, dir or file
 */
struct FileInfo {
    String name;
    String path;
    size_t size = 0;
    time_t modTime = 0;
    bool isDir = false;
};

/**
 * Thread-safe filesystem wrapper that routes all operations through a dedicated FreeRTOS task.
 * Direct file handle access (open/openDir) is intentionally unsupported — file handles cannot
 * safely cross task boundaries with LittleFS.
 */
class SynchronizedFS {
    FS* fsPtr{};
    QueueHandle_t queue{};
    TaskWrapper* fsTask{};

public:
    SynchronizedFS();
    explicit SynchronizedFS(FS& fs);
    ~SynchronizedFS();

    bool begin();
    bool begin(FS &fs);
    void end();
    bool format();
    bool info(FSInfo &info);
    bool exists(const char *path);
    bool rename(const char *pathFrom, const char *pathTo);
    bool remove(const char *path);
    bool mkdir(const char *path);
    bool rmdir(const char *path);
    bool stat(const char *path, FSStat *st);
    bool stat(const char *path, FileInfo *info) const;
    String sha256(const char *path) const;

    size_t readFile(const char *fname, String *s) const;
    size_t writeFile(const char *fname, const String *s) const;
    size_t appendFile(const char *fname, const String *s) const;
    size_t appendFile(const char *fname, const uint8_t *buffer, size_t size) const;
    bool writeFileAsync(const char *fname, const String *s, QueueHandle_t completionQueue = nullptr, uint16_t completionId = 0) const;
    bool list(const char *path, std::deque<FileInfo> *list) const;

protected:
    size_t prvReadFile(const char *fname, String *s) const;
    size_t prvWriteFile(const char *fname, const String *s) const;
    size_t prvAppendFile(const char *fname, const String *s) const;
    size_t prvAppendFile(const char *fname, const uint8_t *buffer, size_t size) const;
    bool prvRemove(const char *path) const;
    bool prvRename(const char *fromName, const String *toName) const;
    bool prvExists(const char *path) const;
    [[nodiscard]] bool prvFormat() const;
    bool prvList(const char *path, std::deque<FileInfo> *fiList) const;
    bool prvInfo(const char *path, FileInfo *fileInfo) const;
    bool prvStat(const char *path, FSStat *fs) const;
    bool prvMakeDir(const char *path) const;
    bool prvSha256(const char *path, String *sha256) const;

    friend void fsInit();
    friend void fsExecute();
};

extern SynchronizedFS SyncFsImpl;


#endif //RP2040_LIGHTFX_EARLE_FILESYSTEM_H
