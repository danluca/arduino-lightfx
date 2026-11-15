// Copyright (c) by Dan Luca. All rights reserved.
//

#ifndef OTA_UPGRADE_H
#define OTA_UPGRADE_H

void handle_fw_upgrade();
// Independent of FreeRTOS task notifications used by synchronous filesystem calls.
void request_fw_upgrade();
bool firmware_upgrade_pending();

#endif //OTA_UPGRADE_H
