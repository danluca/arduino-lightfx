// Copyright (c) by Dan Luca. All rights reserved.
//

#include <Arduino.h>
#include "constants.hpp"
#include "ota_upload.h"
#include "ota_upgrade.h"
#include <filesystem.h>
#include <WebClient.h>
#include "web_server.h"
#include <detail/mimetable.h>
#include "log.h"
#ifndef FW_AUTH_TOKEN
#include "secrets.h"
#endif

static constexpr auto authToken = FW_AUTH_TOKEN;

namespace {
    struct FWUploadData {
        bool auth{};
        bool ownsFile{};
        bool failed{};
        bool busy{};
        String checkSum;
        String fileName;
    };
}

/**
 * Handles FW image buffered upload leveraging raw handling. This method is called multiple times, where the raw status \code client.raw().status\endcode
 * varies in this order:
 * RAW_START --> RAW_WRITE * n --> RAW_END or RAW_ABORTED
 * @param client web client
 */
void handleFWImageUpload(WebClient &client) {
    const WebRequest &req = client.request();
    switch (HTTPRaw &raw = client.raw(); raw.status) {
        case RAW_START: {
            //check auth token; determine the file name and prepare to stream into it
            const auto fwData = new FWUploadData();
            raw.data = fwData;
            fwData->auth = req.header("X-Token").equals(authToken);
            fwData->busy = firmware_upgrade_pending();
            fwData->checkSum = req.header("X-Check");
            fwData->checkSum.toLowerCase();
            fwData->fileName = csFWImageFilename;
            if (fwData->auth && !fwData->busy) {
                // remove() also succeeds when no previous image exists.
                fwData->ownsFile = SyncFsImpl.remove(fwData->fileName.c_str());
                fwData->failed = !fwData->ownsFile;
                web::markFwUpgradeInitiated();  //mark that FW upgrade stream has started - prevent Core0 from running comms
            }
            log_info(F("FW upload auth %s, size read %zu, size expected %zu, sha-256 expected %s"), fwData->auth ? "OK" : "failed",
                raw.totalSize, req.contentLength(), fwData->checkSum.c_str());
            break;
        }
        case RAW_WRITE: {
            //append one raw buffer at a time into the file if auth succeeded
            auto *fwData = static_cast<FWUploadData *>(raw.data);
            if (fwData->ownsFile && !fwData->failed) {
                fwData->failed = SyncFsImpl.appendFile(fwData->fileName.c_str(), raw.buf, raw.currentSize) != raw.currentSize;
            }
            break;
        }
        case RAW_END: {
            //this is called at the regular end of raw data processing (i.e., when the amount of data read from the client matches the content-length) - either successful or not
            //close file; send the response to the client as successful receive

            // The terminal callback owns and frees the per-request state.
            const std::unique_ptr<FWUploadData> fwData(static_cast<FWUploadData *>(raw.data));
            raw.data = nullptr;
            if (!fwData->auth) {
                client.send(401, mime::mimeTable[mime::txt].mimeType, R"({"error": "Unauthorized call"})");
            } else if (fwData->busy) {
                client.send(409, mime::mimeTable[mime::txt].mimeType, R"({"error": "Firmware upgrade already staged"})");
            } else if (fwData->failed) {
                client.send(507, mime::mimeTable[mime::txt].mimeType, R"({"error": "Firmware image storage failed"})");
                web::clearFwUpgradeInitiated();
            } else {
                const String sha256 = SyncFsImpl.sha256(fwData->fileName.c_str());
                // An empty digest signals a filesystem error; it must never match a missing header.
                if (raw.totalSize != req.contentLength() || sha256.length() != 64 || !sha256.equals(fwData->checkSum)) {
                    client.send(406, mime::mimeTable[mime::txt].mimeType, R"({"error": "Upload data integrity failed - Checksum does not match"})");
                    web::clearFwUpgradeInitiated();
                } else if (!SyncFsImpl.stageFirmware(fwData->fileName.c_str())) {
                    client.send(500, mime::mimeTable[mime::txt].mimeType, R"({"error": "Firmware staging failed"})");
                    web::clearFwUpgradeInitiated();
                } else {
                    // Latch before handling any more clients. No subsequent upload may replace this image.
                    request_fw_upgrade();
                    client.send(200, mime::mimeTable[mime::txt].mimeType, R"({"status": "OK"})");
                    log_info(F("FW image %s verified and OTA command committed (%zu bytes)"), fwData->fileName.c_str(), raw.totalSize);
                }
            }
            if (fwData->ownsFile && !firmware_upgrade_pending())
                SyncFsImpl.remove(fwData->fileName.c_str());
            break;
        }
        case RAW_ABORTED: {
            //This is called when the raw data cannot be read from the client any-longer (interrupted). It ends the processing, skips the call for RAW_END
            //close file; delete the file; send the response to the client as an error in receiving
            const std::unique_ptr<FWUploadData> fwData(static_cast<FWUploadData *>(raw.data));
            raw.data = nullptr;
            // Unauthenticated and rejected requests never own the destination file.
            if (fwData && fwData->ownsFile)
                SyncFsImpl.remove(fwData->fileName.c_str());
            web::clearFwUpgradeInitiated();
            client.send(400, mime::mimeTable[mime::txt].mimeType, R"({"error": "Bad Request or Read - Aborted"})");
            log_error(F("FW upload aborted (size read/expected %zu/%zu bytes)"), raw.totalSize, req.contentLength());
            break;
        }
        default: break;
    }
}

