#include <cassert>
#include <ota_fakes.h>
#include "ota_upload.h"
#include "ota_upgrade.h"

static WebClient upload(bool authorized = true) {
    WebClient client;
    client.req.headers["X-Token"] = authorized ? "test-token" : "wrong-token";
    client.req.headers["X-Check"] = String(64, 'a');
    client.req.length = 3;
    handleFWImageUpload(client);
    return client;
}

static void write(WebClient &client) {
    std::memcpy(client.body.buf, "bin", 3);
    client.body.currentSize = 3;
    client.body.totalSize = 3;
    client.body.status = RAW_WRITE;
    handleFWImageUpload(client);
}

static void finish(WebClient &client, RawStatus status = RAW_END) {
    client.body.status = status;
    handleFWImageUpload(client);
    assert(client.body.data == nullptr);
}

static void resetFilesystem() {
    LittleFS = FS{};
    SyncFsImpl = SynchronizedFS{};
    picoOTA = PicoOTA{};
    assert(!firmware_upgrade_pending());
}

int main() {
    // Idle checking must not reboot or sleep.
    handle_fw_upgrade();
    assert(delays.empty());

    // A request without authorization never changes a previous image, even on abort.
    LittleFS.files[csFWImageFilename] = "previous image";
    auto unauthorized = upload(false);
    write(unauthorized);
    finish(unauthorized, RAW_ABORTED);
    assert(LittleFS.files.at(csFWImageFilename) == "previous image");
    assert(SyncFsImpl.removes == 0);
    auto denied = upload(false);
    write(denied);
    finish(denied);
    assert(denied.response == 401);
    assert(SyncFsImpl.removes == 0);

    resetFilesystem();
    auto interrupted = upload();
    write(interrupted);
    finish(interrupted, RAW_ABORTED);
    assert(!LittleFS.exists(csFWImageFilename));

    resetFilesystem();
    SyncFsImpl.writeLimit = 1;
    auto full = upload();
    write(full);
    finish(full);
    assert(full.response == 507 && SyncFsImpl.stages == 0);
    assert(!LittleFS.exists(csFWImageFilename));

    resetFilesystem();
    SyncFsImpl.digest = "wrong checksum";
    auto corrupt = upload();
    write(corrupt);
    finish(corrupt);
    assert(corrupt.response == 406 && SyncFsImpl.stages == 0);

    resetFilesystem();
    SyncFsImpl.digest = "";
    WebClient missing;
    missing.req.headers["X-Token"] = "test-token";
    missing.req.length = 3;
    handleFWImageUpload(missing);
    write(missing);
    finish(missing);
    assert(missing.response == 406 && SyncFsImpl.stages == 0);

    resetFilesystem();
    auto wrongLength = upload();
    write(wrongLength);
    wrongLength.body.totalSize = 4;
    finish(wrongLength);
    assert(wrongLength.response == 406 && SyncFsImpl.stages == 0);

    resetFilesystem();
    LittleFS.files[csFWImageFilename] = "previous image";
    LittleFS.removeOk = false;
    auto cannotReplace = upload();
    write(cannotReplace);
    finish(cannotReplace);
    assert(cannotReplace.response == 507);
    assert(LittleFS.files.at(csFWImageFilename) == "previous image");

    resetFilesystem();
    picoOTA.addOk = false;
    auto badAdd = upload();
    write(badAdd);
    finish(badAdd);
    assert(badAdd.response == 500 && picoOTA.commits == 0);
    assert(!LittleFS.exists(_OTA_COMMAND_FILE));
    assert(!firmware_upgrade_pending());

    resetFilesystem();
    picoOTA.commitOk = false;
    auto badCommit = upload();
    write(badCommit);
    finish(badCommit);
    assert(badCommit.response == 500 && picoOTA.commits == 1);
    assert(!LittleFS.exists(_OTA_COMMAND_FILE));
    assert(!firmware_upgrade_pending());

    // A previous command that cannot be removed prevents staging.
    resetFilesystem();
    LittleFS.files[_OTA_COMMAND_FILE] = "old command";
    LittleFS.files[csFWImageFilename] = "bin";
    LittleFS.removeOk = false;
    assert(!stageOtaCommand(LittleFS, picoOTA, csFWImageFilename));
    assert(picoOTA.commits == 0);
    FS otherFilesystem;
    assert(!stageOtaCommand(otherFilesystem, picoOTA, csFWImageFilename));

    resetFilesystem();
    auto success = upload();
    write(success);
    finish(success);
    assert(success.response == 200 && firmware_upgrade_pending());
    assert(LittleFS.files.at(csFWImageFilename) == "bin");
    assert(LittleFS.files.at(_OTA_COMMAND_FILE) == "valid command");

    // Later clients in the same server pass cannot clobber the committed image.
    auto busy = upload();
    write(busy);
    finish(busy);
    assert(busy.response == 409);
    auto busyAbort = upload();
    finish(busyAbort, RAW_ABORTED);
    assert(LittleFS.files.at(csFWImageFilename) == "bin");
    assert(picoOTA.commits == 1);

    // FS operations after completion cannot consume the atomic pending flag.
    (void)SyncFsImpl.sha256(csFWImageFilename);
    assert(firmware_upgrade_pending());
    bool rebooted = false;
    try { handle_fw_upgrade(); } catch (const Reboot &) { rebooted = true; }
    assert(rebooted && delays.size() == 1 && delays[0] == 2000);
    assert(watchdog_hw->scratch[kResetMarkerScratchIndex] == kResetMarkerOta);
}
