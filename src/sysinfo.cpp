// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#include <ArduinoJson.h>
#include "filesystem.h"
#include "util.h"
#include "sysinfo.h"
#include "config.h"
#include "version.h"
#ifndef DISABLE_LOGGING
#include <SchedulerExt.h>
#endif

#define BUF_ID_SIZE  20

static const char unknown[] PROGMEM = "N/A";
#ifndef DISABLE_LOGGING
static const char threadInfoFmt[] PROGMEM = "Task[%u]:: name='%s' time=%s%%[%u] priority=%i state=%i id=%i core=%i stackSize=%u free=%u\n";
static const char threadInfoVerboseFmt[] PROGMEM = "Task[%u]:: name='%s' time=%s%% priority=%i state=%i id=%y \n  basePriority=%i stackBase=%X size=%u free=%u core=%i\n";
static const char heapStackInfoFmt[] PROGMEM = "HEAP/STACK INFO\n  Total Stack:: size=%u free: tasks=%u, system=%i;\n  Total Heap:: size=%u free=%u\n";
static const char heapStackVerboseFmt[] PROGMEM = "HEAP/STACK INFO\nTotal Stack:: size=%u free: tasks=%u, system=%i taskCount=%u;\n  Total Heap:: size=%u used=%u free=%u lowestFree=%u freeBlocks=%u [%u-%u] allocations=%u frees=%u\n";
static const char sysInfoFmt[] PROGMEM = "SYSTEM INFO\n  CPU ROM %d [%D MHz] CORE %d\n  FreeRTOS version %s\n  Arduino PICO version %s [SDK %s]\n  Board UID 0x%s name '%s'\n  MAC Address %s\n  Device name %s\n  Flash size %u";
#endif
static const char *const csBuildVersion PROGMEM = "buildVersion";
static const char *const csBoardName PROGMEM = "boardName";
static const char *const csBuildTime PROGMEM = "buildTime";
static const char *const csScmBranch PROGMEM = "scmBranch";
static const char *const csWdReboots PROGMEM = "wdReboots";
static const char *const csBoardId PROGMEM = "boardId";
static const char *const csSecElemId PROGMEM = "secElemId";
static const char *const csMacAddress PROGMEM = "macAddress";
static const char *const csWifiFwVersion PROGMEM = "wifiFwVersion";
static const char *const csIpAddress PROGMEM = "ipAddress";
static const char *const csGatewayAddress PROGMEM = "gatewayIpAddress";
static const char *const csHeapSize PROGMEM = "heapSize";
static const char *const csFreeHeap PROGMEM = "freeHeap";
static const char *const csStackSize PROGMEM = "stackSize";
static const char *const csFreeStack PROGMEM = "freeStack";
static const char *const csStatus PROGMEM = "status";

//#define STORAGE_CMD_TOTAL_BYTES 32

unsigned long prevStatTime = 0;
unsigned long prevIdleTime = 0;
SysInfo *sysInfo;

void logTaskStats() {
#ifndef DISABLE_LOGGING
    // Refs: https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/01-uxTaskGetSystemState
    unsigned long ulTotalRunTime=0, ulTotalStack=0, ulFreeStack=0;
    /* Take a snapshot of the number of tasks in case it changes while this function is executing. */
    volatile UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    /* Allocate a TaskStatus_t structure for each task. An array could be allocated statically at compile time. */
    if(auto *pxTaskStatusArray = static_cast<TaskStatus_t *>(pvPortMalloc(uxArraySize * sizeof(TaskStatus_t))); pxTaskStatusArray != nullptr ) {
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );
        Log.infoln("Total run time reported by system: %u", ulTotalRunTime);

        /* Avoid divide by zero errors. */
        if( ulTotalRunTime > 100 ) {
            // need to figure out task run time per core - raw results show that the total runtime above does not equal with all tasks runtime counter added up
            uint64_t coresTotalRunTime[2]{};    //using 64 bit longs, summing up all the runtime counters for tasks overflows the 32 bit numbers
            for( volatile UBaseType_t x = 0; x < uxArraySize; x++ ) {
                const TaskStatus_t ts = pxTaskStatusArray[x];
                coresTotalRunTime[0] += (ts.uxCoreAffinityMask & CORE_0 ? ts.ulRunTimeCounter : 0);
                coresTotalRunTime[1] += (ts.uxCoreAffinityMask & CORE_1 ? ts.ulRunTimeCounter : 0);
            }
            Log.infoln("Total run time reported by individual tasks per core: CORE 0=%u; CORE 1=%u", coresTotalRunTime[0], coresTotalRunTime[1]);
            ulTotalRunTime /= 100UL;    // For percentage calculations
            coresTotalRunTime[0] /= 100UL;
            coresTotalRunTime[1] /= 100UL;
            Log.info(F("THREADS/TASKS INFO\n"));
            /* For each populated position in the pxTaskStatusArray array, format the raw data as human-readable ASCII data. */
            for( volatile UBaseType_t x = 0; x < uxArraySize; x++ ) {
                const TaskStatus_t ts = pxTaskStatusArray[x];
                // What percentage of the total run time has the task used? ulTotalRunTimeDiv100 has already been divided by 100.
                // these are huge numbers (ticks?) - we'll divide both by 256 to have more reasonable values
                uint64_t ref = ((ts.uxCoreAffinityMask & CORE_0 ? coresTotalRunTime[0] : 0) + (ts.uxCoreAffinityMask & CORE_1 ? coresTotalRunTime[1] : 0)) >> 8;
                double fStatsAsPercentage = (ts.ulRunTimeCounter >> 8) / (double) ref;
                char strStat[7];
                size_t szStat = sprintf(strStat, "%.2f", fStatsAsPercentage);
                strStat[szStat] = 0;    //ensure null terminator
                //is this a task we created? if so, we have extra information - like stack size
                const TaskWrapper *tw = Scheduler.getTask(ts.xTaskNumber);
                uint32_t stackSize = tw ? tw->getStackSize() : 0;
                if (LOG_LEVEL_TRACE > Log.getLevel())
                    Log.info(threadInfoFmt, x, ts.pcTaskName, strStat, ts.ulRunTimeCounter, ts.uxCurrentPriority, ts.eCurrentState, ts.xTaskNumber, ts.uxCoreAffinityMask,
                        stackSize, ts.usStackHighWaterMark);
                else
                    Log.trace(threadInfoVerboseFmt, x, ts.pcTaskName, strStat, ts.uxCurrentPriority, ts.eCurrentState, ts.xTaskNumber, ts.uxBasePriority,
                        ts.pxStackBase, stackSize, ts.usStackHighWaterMark, ts.uxCoreAffinityMask);
                ulTotalStack += stackSize;
                ulFreeStack += ts.usStackHighWaterMark;
            }
            Log.endContinuation();
        }
        /* The array is no longer needed, free the memory it consumes. */
        vPortFree( pxTaskStatusArray );
    }
    //memory stats - NOTE: vPortGetHeapStats is not implemented in the RP2040 port with the heap memory strategy adopted (seemingly heap3)
//    HeapStats_t heapStats;
//    vPortGetHeapStats(&heapStats);
    if (LOG_LEVEL_TRACE > Log.getLevel()) {
        //Log.info(heapStackInfoFmt, ulTotalStack, ulFreeStack, configTOTAL_HEAP_SIZE, heapStats.xAvailableHeapSpaceInBytes);
        Log.info(heapStackInfoFmt, ulTotalStack, ulFreeStack, rp2040.getFreeStack(), rp2040.getTotalHeap(), rp2040.getFreeHeap());
    } else {
        Log.trace(heapStackVerboseFmt, ulTotalStack, ulFreeStack, rp2040.getFreeStack(), uxArraySize, rp2040.getTotalHeap(), rp2040.getUsedHeap(),
                  rp2040.getFreeHeap(), rp2040.getPSRAMSize(), rp2040.getTotalPSRAMHeap(), rp2040.getUsedPSRAMHeap(), rp2040.getFreePSRAMHeap(), 0, 0);
    }
    Log.endContinuation();
#endif
}

void logSystemInfo() {
#ifndef DISABLE_LOGGING
    Log.infoln(sysInfoFmt, rp2040_rom_version(), RP2040::f_cpu()/1000000.0, RP2040::cpuid(), tskKERNEL_VERSION_NUMBER, ARDUINO_PICO_VERSION_STR, PICO_SDK_VERSION_STRING,
               sysInfo->getBoardId().c_str(), BOARD_NAME, sysInfo->getMacAddress().c_str(), DEVICE_NAME, sysInfo->get_flash_capacity());
    Log.infoln(F("System reset reason %d"), rp2040.getResetReason());
#endif
}

// SysInfo
SysInfo::SysInfo() : boardName(DEVICE_NAME), buildVersion(BUILD_VERSION), buildTime(BUILD_TIME), scmBranch(GIT_BRANCH),
    ipAddress({IP_ADDR}), ipGateway({IP_GW}) {
    boardId.reserve(BUF_ID_SIZE);       // flash unique ID in hex, \0 terminator
    secElemId.reserve(BUF_ID_SIZE);     // 18 from ECCX08Class::serialNumber() implementation
    macAddress.reserve(BUF_ID_SIZE);    // 6 (WL_MAC_ADDR_LENGTH) groups of 2 hex digits and ':' separator, includes \0 terminator
    strIpAddress.reserve(BUF_ID_SIZE);     // 4 groups of 3 digits, 3 '.' separators, \0 terminator
    wifiFwVersion.reserve(BUF_ID_SIZE); // typical semantic version e.g. v1.5.0, 3 groups of 2 digits, '.' separator, \0 terminator
    ssid.reserve(BUF_ID_SIZE);          // initial space, most networks are short names
    status = 0;
    cleanBoot = true;
}

/**
 * Capture Flash UID as board unique identifier in HEX string format
 */
void SysInfo::fillBoardId() {
    boardId = rp2040.getChipID();
}

/**
 * <p>Per AT25SF128A Flash specifications, command ox9F returns 0x1F8901, where last byte 0x01 should represent density (size)
 * Trial/error shows the command returns 0xFF1F89011F</p>
 * <p>We won't use the flash chip SPI commands - very chip specific - but instead leverage the constant PICO_FLASH_SIZE_BYTES already tailored to the board we use, Nano RP2040</p>
 * @return Board flash size in bytes - currently fixed at PICO_FLASH_SIZE_BYTES
 */
uint SysInfo::get_flash_capacity() const {
//    uint8_t txbuf[STORAGE_CMD_TOTAL_BYTES] = {0x9f};
//    uint8_t rxbuf[STORAGE_CMD_TOTAL_BYTES] = {0};
//    flash_do_cmd(txbuf, rxbuf, STORAGE_CMD_TOTAL_BYTES);

//    return 1 << rxbuf[3];
    return PICO_FLASH_SIZE_BYTES;
}

uint8_t SysInfo::setSysStatus(uint8_t bitMask) {
    status |= bitMask;
    return status;
}

uint8_t SysInfo::resetSysStatus(uint8_t bitMask) {
    status &= (~bitMask);
    return status;
}

bool SysInfo::isSysStatus(uint8_t bitMask) const {
    return (status & bitMask);
}

uint8_t SysInfo::getSysStatus() const {
    return status;
}

void SysInfo::setWiFiInfo(::WiFiClass &wifi) {
    ssid = wifi.SSID();
    wifiFwVersion = ::WiFiClass::firmwareVersion();
    // strIpAddress = wifi.localIP().toString();
    // strGatewayIpAddress = wifi.gatewayIP().toString();
    strIpAddress = ipAddress.toString();
    strGatewayIpAddress = ipGateway.toString();

    //MAC address - Formats the MAC address into the character buffer provided, space for 20 chars is needed (includes nul terminator)
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    wifi.macAddress(mac);

    char buf[BUF_ID_SIZE];
    int x = 0;
    for (auto &b : mac)
        x += sprintf(buf+x, "%02X:", b);
    //last character - at index x-1 is a ':', make it null to trim the last colon character
    buf[x-1] = 0;
    macAddress = buf;
}

void SysInfo::setSecureElementId(const String &secId) {
    secElemId = secId;
}

/**
 * Read the saved sys info file - no op, if there is no file
 * Note the time this is performed, the WiFi is not ready, nor other fields even populated yet
 */
void readSysInfo() {
    auto json = new String();
    json->reserve(512);  // approximation
    size_t sysSize = readTextFile(sysFileName, json);
    if (sysSize > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *json);
        if (error) {
            Log.errorln(F("Error reading the system information JSON file %s [%d bytes]: %s - system information state NOT restored. Content read:\n%s"), sysFileName, sysSize, error.c_str(), json->c_str());
            delete json;
            return;
        }
        //const fields
        String bldVersion = doc[csBuildVersion];
        String brdName = doc[csBoardName];
        String bldTime = doc[csBuildTime];
        String gitBranch = doc[csScmBranch].as<String>();
        if (bldVersion.equals(sysInfo->buildVersion) && doc[csWdReboots].is<JsonArray>()) {
            JsonArray wdReboots = doc[csWdReboots].as<JsonArray>();
            for (JsonVariant i: wdReboots)
                sysInfo->wdReboots.push(i.as<time_t>());
        } else
            Log.warningln(F("Build version change detected - previous watchdog reboot timestamps %s have been discarded"), doc[csWdReboots].as<String>().c_str());
        sysInfo->boardId = doc[csBoardId].as<String>();
        sysInfo->secElemId = doc[csSecElemId].as<String>();
        sysInfo->macAddress = doc[csMacAddress].as<String>();
        sysInfo->wifiFwVersion = doc[csWifiFwVersion].as<String>();
        sysInfo->strIpAddress = doc[csIpAddress].as<String>();
        sysInfo->strGatewayIpAddress = doc[csGatewayAddress].as<String>();
        sysInfo->heapSize = doc[csHeapSize];
        sysInfo->freeHeap = doc[csFreeHeap];
        sysInfo->stackSize = doc[csStackSize];
        sysInfo->freeStack = doc[csFreeStack];
        //do not override current status (in progress of populating) with last run status
        uint8_t lastStatus = doc[csStatus];
#ifndef DISABLE_LOGGING
        Log.infoln(F("System Information restored from %s [%d bytes]: boardName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%X (last %X), IP=%s, Gateway=%s"),
                   sysFileName, sysSize, brdName.c_str(), bldVersion.c_str(), bldTime.c_str(), gitBranch.c_str(), sysInfo->boardId.c_str(), sysInfo->secElemId.c_str(), sysInfo->macAddress.c_str(), sysInfo->status, lastStatus,
                   sysInfo->strIpAddress.c_str(), sysInfo->strGatewayIpAddress.c_str());
#endif
    }
    delete json;
}

/**
 * Saves the sys info to the file
 */
void saveSysInfo() {
    JsonDocument doc;
    doc[csBuildVersion] = sysInfo->buildVersion;
    doc[csBoardName] = sysInfo->boardName;
    doc[csBuildTime] = sysInfo->buildTime;
    doc[csScmBranch] = sysInfo->scmBranch;
    doc[csBoardId] = sysInfo->boardId;
    doc[csSecElemId] = sysInfo->secElemId;
    doc[csMacAddress] = sysInfo->macAddress;
    doc[csWifiFwVersion] = sysInfo->wifiFwVersion;
    doc[csIpAddress] = sysInfo->strIpAddress;
    doc[csGatewayAddress] = sysInfo->strGatewayIpAddress;
    doc[csHeapSize] = sysInfo->heapSize;
    doc[csFreeHeap] = sysInfo->freeHeap;
    doc[csStackSize] = sysInfo->stackSize;
    doc[csFreeStack] = sysInfo->freeStack;
    doc[csStatus] = sysInfo->status;
    JsonArray reboots = doc[csWdReboots].to<JsonArray>();
    for (auto & t : sysInfo->wdReboots)
        reboots.add(t);

    auto str = new String();    //larger temporary string, put it on the heap
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!writeTextFile(sysFileName, str))
        Log.errorln(F("Failed to create/write the system information file %s"), sysFileName);
    delete str;
}
