#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define F(value) value
#define FW_AUTH_TOKEN "test-token"
#define log_info(...) ((void)0)
#define log_error(...) ((void)0)
#define _OTA_COMMAND_FILE "/otacommand.bin"
inline constexpr auto csFWImageFilename = "/fw.bin";
inline constexpr unsigned kResetMarkerScratchIndex = 0;
inline constexpr uint32_t kResetMarkerOta = 0xA11CE502;

struct String : std::string {
    using std::string::string;
    String(const std::string &s) : std::string(s) {}
    bool equals(const String &s) const { return *this == s; }
    void toLowerCase() {
        std::transform(begin(), end(), begin(), [](unsigned char c) { return std::tolower(c); });
    }
};

enum RawStatus { RAW_START, RAW_WRITE, RAW_END, RAW_ABORTED };
struct HTTPRaw {
    RawStatus status = RAW_START;
    size_t totalSize = 0;
    size_t currentSize = 0;
    uint8_t buf[16]{};
    void *data = nullptr;
};
struct WebRequest {
    std::map<std::string, String> headers;
    size_t length = 0;
    String header(const char *name) const {
        const auto it = headers.find(name);
        return it == headers.end() ? String{} : it->second;
    }
    size_t contentLength() const { return length; }
};
class WebClient {
public:
    WebRequest req;
    HTTPRaw body;
    int response = 0;
    const WebRequest &request() const { return req; }
    HTTPRaw &raw() { return body; }
    void send(int code, const char *, const char *) { response = code; }
};
namespace mime {
    enum { txt };
    struct Entry { const char *mimeType; };
    inline Entry mimeTable[] = {{"text/plain"}};
}

class FS {
public:
    std::map<std::string, std::string> files;
    bool removeOk = true;
    bool exists(const char *path) const { return files.count(path) != 0; }
    bool remove(const char *path) {
        if (!removeOk) return false;
        files.erase(path);
        return true;
    }
};
inline FS LittleFS;
class PicoOTA {
public:
    bool addOk = true;
    bool commitOk = true;
    int commits = 0;
    void begin() {}
    bool addFile(const char *path) { return addOk && LittleFS.exists(path); }
    bool commit() {
        ++commits;
        LittleFS.files[_OTA_COMMAND_FILE] = commitOk ? "valid command" : "partial command";
        return commitOk;
    }
};
inline PicoOTA picoOTA;

#include "fs_ota.h"

struct SynchronizedFS {
    size_t writeLimit = 1000;
    String digest = String(64, 'a');
    int removes = 0;
    int stages = 0;
    bool remove(const char *path) { ++removes; return LittleFS.remove(path); }
    size_t appendFile(const char *path, uint8_t *buf, size_t len) {
        const auto written = std::min(len, writeLimit);
        LittleFS.files[path].append(reinterpret_cast<char *>(buf), written);
        return written;
    }
    String sha256(const char *) { return digest; }
    bool stageFirmware(const char *path) {
        ++stages;
        return stageOtaCommand(LittleFS, picoOTA, path);
    }
};
inline SynchronizedFS SyncFsImpl;

struct Watchdog { uint32_t scratch[8]{}; };
inline Watchdog testWatchdog;
inline Watchdog *watchdog_hw = &testWatchdog;
struct Reboot {};
inline std::vector<uint32_t> delays;
inline void taskDelay(uint32_t ms) { delays.push_back(ms); }
inline void watchdog_reboot(uint32_t, uint32_t, uint32_t) { throw Reboot{}; }
inline void tight_loop_contents() {}
