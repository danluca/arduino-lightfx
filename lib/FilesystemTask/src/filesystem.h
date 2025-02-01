// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2040_LIGHTFX_EARLE_FILESYSTEM_H
#define RP2040_LIGHTFX_EARLE_FILESYSTEM_H

#include <Arduino.h>
#include <FS.h>
#include <functional>
#include "../../SchedulerExt/src/SchedulerExt.h"

inline constexpr auto FS_PATH_SEPARATOR PROGMEM = "/";

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

class SynchronizedFS {
    FS* fsPtr{};
    QueueHandle_t queue{};
    TaskWrapper* fsTask{};

public:
    SynchronizedFS();
    ~SynchronizedFS();

    void begin(FS& fs);
    size_t readFile(const char *fname, String *s) const;
    size_t writeFile(const char *fname, String *s) const;
    bool writeFileAsync(const char *fname, String *s) const;
    bool remove(const char *fname) const;
    bool exists(const char *fname) const;
    bool format() const;
    void list(const char *path, const std::function<void(const FileInfo&)> &callback) const;
    FileInfo info(const char *path) const;

protected:
    size_t prvReadFile(const char *fname, String *s) const;
    size_t prvWriteFile(const char *fname, const String *s) const;
    size_t prvWriteFileAndFreeMem(const char *fname, const String *s) const;
    bool prvRemove(const char *fname) const;
    bool prvExists(const char *fname) const;
    bool prvFormat() const;
    bool prvList(const char *path, const std::function<void(const FileInfo&)> &callback) const;
    bool prvInfo(const char *path, const std::function<void(const FileInfo&)> &callback) const;

    friend void fsInit();
    friend void fsExecute();
    friend void listFiles(Dir &dir, String &path, const std::function<void(const FileInfo&)> &callback);
};

extern SynchronizedFS SyncFs;


#endif //RP2040_LIGHTFX_EARLE_FILESYSTEM_H
