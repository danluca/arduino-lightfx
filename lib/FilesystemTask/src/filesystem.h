// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2040_LIGHTFX_EARLE_FILESYSTEM_H
#define RP2040_LIGHTFX_EARLE_FILESYSTEM_H

#include <Arduino.h>
#include <FS.h>
#include <FSImpl.h>
#include <functional>
#include <deque>
#include "../../SchedulerExt/src/SchedulerExt.h"

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

class SynchronizedFS : public FSImpl {
    FS* fsPtr{};
    QueueHandle_t queue{};
    TaskWrapper* fsTask{};

public:
    SynchronizedFS();
    explicit SynchronizedFS(FS& fs);
    ~SynchronizedFS() override;

    bool setConfig(const FSConfig &cfg) override;
    bool begin() override;
    bool begin(FS &fs);
    void end() override;
    bool format() override;
    bool info(FSInfo &info) override;
    //not supported - we'd have to experiment with getting a File pointer out from another task and whether multiple file objects can be used concurrently
    //at first glance, the LittleFSFileImpl does make use of the LittleFS instance, indicating it may not be thread safe...
    FileImplPtr open(const char *path, OpenMode openMode, AccessMode accessMode) override {
        Serial.println("SynchronizedFS::open not supported");
        return nullptr;
    };
    DirImplPtr openDir(const char *path) override {
        Serial.println("SynchronizedFS::openDir not supported");
        return nullptr;
    };
    bool exists(const char *path) override;
    bool rename(const char *pathFrom, const char *pathTo) override;
    bool remove(const char *path) override;
    bool mkdir(const char *path) override;
    bool rmdir(const char *path) override;
    bool stat(const char *path, FSStat *st) override;
    bool stat(const char *path, FileInfo *info) const;

    size_t readFile(const char *fname, String *s) const;
    size_t writeFile(const char *fname, String *s) const;
    size_t appendFile(const char *fname, String *s) const;
    bool writeFileAsync(const char *fname, String *s) const;
    bool list(const char *path, std::deque<FileInfo*> *list) const;

protected:
    size_t prvReadFile(const char *fname, String *s) const;
    size_t prvWriteFile(const char *fname, const String *s) const;
    size_t prvAppendFile(const char *fname, const String *s) const;
    size_t prvWriteFileAndFreeMem(const char *fname, const String *s) const;
    bool prvRemove(const char *path) const;
    bool prvRename(const char *fromName, const String *toName) const;
    bool prvExists(const char *path) const;
    bool prvFormat() const;
    bool prvList(const char *path, std::deque<FileInfo *> *fiList) const;
    bool prvInfo(const char *path, FileInfo *fileInfo) const;
    bool prvStat(const char *path, FSStat *fs) const;
    bool prvMakeDir(const char *path) const;

    friend void fsInit();
    friend void fsExecute();
    friend void listFiles(Dir &dir, String &path, const std::function<void(FileInfo*)> &callback);
};

extern SynchronizedFS SyncFsImpl;


#endif //RP2040_LIGHTFX_EARLE_FILESYSTEM_H
