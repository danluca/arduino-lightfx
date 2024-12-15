// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#include <ArduinoJson.h>
#include <SchedulerExt.h>
#include "filesystem.h"
#include "util.h"
#include "sysinfo.h"
#include "config.h"
#include "timeutil.h"
#include "version.h"
#include "stringutils.h"

#define BUF_ID_SIZE  20

static const char unknown[] PROGMEM = "N/A";
static constexpr char threadInfoFmt[] PROGMEM = "Task[%u]:: name='%s' time=%s%%[%u] priority=%i state=%s id=%i core=%i stackSize=%u free=%u\n";
static constexpr char threadInfoVerboseFmt[] PROGMEM = "Task[%u]:: name='%s' time=%s%% priority=%i state=%s id=%s \n  basePriority=%i stackBase=%X size=%u free=%u core=%i\n";
static constexpr char heapStackInfoFmt[] PROGMEM = "HEAP/STACK INFO\n  Total Stack:: size=%u free: tasks=%u, system=%x;\n  Total Heap:: size=%u free=%u\n";
static constexpr char heapStackVerboseFmt[] PROGMEM = "HEAP/STACK INFO\nTotal Stack:: size=%u free: tasks=%u, system=%x taskCount=%u;\n  Total Heap:: size=%u used=%u free=%u lowestFree=%u freeBlocks=%u [%u-%u] allocations=%u frees=%u\n";
static constexpr char sysInfoFmt[] PROGMEM = "SYSTEM INFO\n  CPU ROM %d [%D MHz] CORE %d\n  FreeRTOS version %s\n  Arduino PICO version %s [SDK %s]\n  Board UID 0x%s name '%s'\n  MAC Address %s\n  Device name %s\n  Flash size %u";
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

const char *taskStatusToString(const eTaskState state) {
    switch (state) {
        case eReady: return "Ready";
        case eBlocked: return "Blocked";
        case eSuspended: return "Suspended";
        case eDeleted: return "Deleted";
        case eRunning: return "Running";
        case eInvalid: return "Invalid";
        default: return "Unknown";
    }
}
void logTaskStats() {
    if (!Log.isEnabled(INFO))
        return;
    // Refs: https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/01-uxTaskGetSystemState
    unsigned long ulTotalRunTime=0, ulTotalStack=0, ulFreeStack=0;
    /* Take a snapshot of the number of tasks in case it changes while this function is executing. */
    volatile UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    /* Allocate a TaskStatus_t structure for each task. An array could be allocated statically at compile time. */
    if(auto *pxTaskStatusArray = static_cast<TaskStatus_t *>(pvPortMalloc(uxArraySize * sizeof(TaskStatus_t))); pxTaskStatusArray != nullptr ) {
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );
        Log.info("Total run time reported by system: %u", ulTotalRunTime);

        /* Avoid divide by zero errors. */
        if( ulTotalRunTime > 100 ) {
            // need to figure out task run time per core - raw results show that the total runtime above does not equal with all tasks runtime counter added up
            uint64_t coresTotalRunTime[2]{};    //using 64 bit longs, summing up all the runtime counters for tasks overflows the 32 bit numbers
            for( volatile UBaseType_t x = 0; x < uxArraySize; x++ ) {
                const TaskStatus_t ts = pxTaskStatusArray[x];
                coresTotalRunTime[0] += (ts.uxCoreAffinityMask & CORE_0 ? ts.ulRunTimeCounter : 0);
                coresTotalRunTime[1] += (ts.uxCoreAffinityMask & CORE_1 ? ts.ulRunTimeCounter : 0);
            }
            Log.info("Total run time reported by individual tasks per core: CORE 0=%lu; CORE 1=%lu", coresTotalRunTime[0], coresTotalRunTime[1]);
            ulTotalRunTime /= 100UL;    // For percentage calculations
            coresTotalRunTime[0] /= 100UL;
            coresTotalRunTime[1] /= 100UL;
            Log.info(F("THREADS/TASKS INFO - max priority %d\n"), configMAX_PRIORITIES-1);
            String strTaskInfo;
            strTaskInfo.reserve(1024);  //ensure enough space to avoid reallocations for each thread
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
                if (!Log.isEnabled(DEBUG)) {
                    StringUtils::append(strTaskInfo, threadInfoFmt, x, ts.pcTaskName, strStat, ts.uxCurrentPriority, taskStatusToString(ts.eCurrentState), ts.xTaskNumber, ts.uxCoreAffinityMask,
                        stackSize, ts.usStackHighWaterMark);
                    StringUtils::append(strTaskInfo,"\n  stack end %X begin %X free %i min %i\n", (*ts.pxEndOfStack), (*ts.pxStackBase), ((*ts.pxEndOfStack)-(*ts.pxStackBase)), ts.usStackHighWaterMark);
                } else
                    StringUtils::append(strTaskInfo, threadInfoVerboseFmt, x, ts.pcTaskName, strStat, ts.uxCurrentPriority, taskStatusToString(ts.eCurrentState), ts.xTaskNumber, ts.uxBasePriority,
                        ts.pxStackBase, stackSize, ts.usStackHighWaterMark, ts.uxCoreAffinityMask);
                ulTotalStack += stackSize;
                ulFreeStack += ts.usStackHighWaterMark;
            }
            Log.info(strTaskInfo.c_str());
        }
        /* The array is no longer needed, free the memory it consumes. */
        vPortFree( pxTaskStatusArray );
    }
    //memory stats - NOTE: vPortGetHeapStats is not implemented in the RP2040 port with the heap memory strategy adopted (seemingly heap3)
//    HeapStats_t heapStats;
//    vPortGetHeapStats(&heapStats);
    if (Log.isEnabled(DEBUG)) {
        Log.debug(heapStackVerboseFmt, ulTotalStack, ulFreeStack, rp2040.getFreeStack(), uxArraySize, rp2040.getTotalHeap(), rp2040.getUsedHeap(),
                  rp2040.getFreeHeap(), rp2040.getPSRAMSize(), rp2040.getTotalPSRAMHeap(), rp2040.getUsedPSRAMHeap(), rp2040.getFreePSRAMHeap(), 0, 0);
    } else {
        //StringUtils::append(strHeapInfo, heapStackInfoFmt, ulTotalStack, ulFreeStack, configTOTAL_HEAP_SIZE, heapStats.xAvailableHeapSpaceInBytes);
        Log.info(heapStackInfoFmt, ulTotalStack, ulFreeStack, abs(rp2040.getFreeStack()), rp2040.getTotalHeap(), rp2040.getFreeHeap());
        Log.info("  Stack pointer: start %X, free %X\n", rp2040.getStackPointer(), rp2040.getFreeStack());
    }

    auto *stats = new String();
    stats->reserve(1024);       // doc states ~40bytes per task, we have 12 tasks
    stats->concat(F("\nName\tSt\tPr\tStk\tNum\tCore\n"));
    vTaskList(stats->begin());
    Log.info(stats->c_str());

    // Log.info(F("Current watchdog remaining value %u us"), watchdog_get_time_remaining_ms());
}

const char *resetReasonToString(const uint8_t reason) {
    switch (reason) {
        case 0: return "unknown";
        case 1: return "power on";
        case 2: return "pin reset";
        case 3: return "soft";
        case 4: return "watchdog";
        case 5: return "debug";
        case 6: return "glitch";
        case 7: return "brownout";
        default: return "n/r";
    }
}

/**
 * Logs detailed system information for debugging and diagnostic purposes.
 * This function outputs various system-level details, including:
 * - CPU ROM version and core speed
 * - FreeRTOS, Arduino PICO, and SDK version details
 * - Board identifier, board name, and MAC address
 * - Device name and flash memory size
 * - System reset reason with detailed status codes
 */
void logSystemInfo() {
    Log.info(sysInfoFmt, rp2040_rom_version(), RP2040::f_cpu()/1000000.0, RP2040::cpuid(), tskKERNEL_VERSION_NUMBER, ARDUINO_PICO_VERSION_STR, PICO_SDK_VERSION_STRING,
               sysInfo->getBoardId().c_str(), BOARD_NAME, sysInfo->getMacAddress().c_str(), DEVICE_NAME, sysInfo->get_flash_capacity());
    Log.info(F("System reset reason %s"), resetReasonToString(rp2040.getResetReason()));
}

/**
 * Logs the current system state, including system status and formatted uptime.
 */
void logSystemState() {
    char buf[20];
    const unsigned long uptime = millis();
    snprintf(buf, 16, "%3dD %2dH %2dm", uptime/86400000l, (uptime/3600000l%24), (uptime/60000%60));
    Log.info(F("System state: %X; uptime %s"), sysInfo->getSysStatus(), buf);
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

void SysInfo::setWiFiInfo(nina::WiFiClass &wifi) {
    ssid = wifi.SSID();
    wifiFwVersion = nina::WiFiClass::firmwareVersion();
    // strIpAddress = wifi.localIP().toString();
    // strGatewayIpAddress = wifi.gatewayIP().toString();
    strIpAddress = ipAddress.toString();
    strGatewayIpAddress = ipGateway.toString();

    //MAC address - Formats the MAC address into the character buffer provided, space for 20 chars is needed (includes nul terminator)
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    wifi.macAddress(mac);

    char buf[BUF_ID_SIZE];
    int x = 0;
    for (const auto &b : mac)
        x += sprintf(buf+x, "%02X:", b);
    //last character - at index x-1 is a ':', make it null to trim the last colon character
    buf[x-1] = 0;
    macAddress = buf;
}

void SysInfo::setSecureElementId(const String &secId) {
    secElemId = secId;
}

void SysInfo::sysConfig(JsonDocument &doc) {
    char buf[20];
    doc["arduinoPicoVersion"] = ARDUINO_PICO_VERSION_STR;
    doc["freeRTOSVersion"] = tskKERNEL_VERSION_NUMBER;
    doc["boardName"] = sysInfo->getBoardName();
    doc["boardUid"] = sysInfo->getBoardId();
    doc["secElemId"] = sysInfo->getSecureElementId();
    doc["fwVersion"] = sysInfo->getBuildVersion();
    doc["fwBranch"] = sysInfo->getScmBranch();
    doc["buildTime"] = sysInfo->getBuildTime();
    doc["watchdogRebootsCount"] = sysInfo->watchdogReboots().size();
    doc["cleanBoot"] = sysInfo->isCleanBoot();
    if (!sysInfo->watchdogReboots().empty()) {
        formatDateTime(buf, sysInfo->watchdogReboots().back());
        doc["lastWatchdogReboot"] = buf;
    }
    doc["wifiCurVersion"] = sysInfo->getWiFiFwVersion();
    doc["wifiLatestVersion"] = WIFI_FIRMWARE_LATEST_VERSION;
    const auto hldList = doc["holidayList"].to<JsonArray>();
    for (uint8_t hi = None; hi <= NewYear; hi++)
        hldList.add(holidayToString(static_cast<Holiday>(hi)));
    formatDateTime(buf, now());
    const bool bDST = sysInfo->isSysStatus(SYS_STATUS_DST);
    doc["currentOffset"] = bDST ? CDT_OFFSET_SECONDS : CST_OFFSET_SECONDS;
    doc["dst"] = bDST;
    doc["MAC"] = sysInfo->getMacAddress();
}

/**
 * Read the saved sys info file - no op, if there is no file
 * Note the time this is performed, the WiFi is not ready, nor other fields even populated yet
 */
void readSysInfo() {
    auto json = new String();
    json->reserve(512);  // approximation
    if (const size_t sysSize = readTextFile(sysFileName, json); sysSize > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *json);
        if (error) {
            Log.error(F("Error reading the system information JSON file %s [%d bytes]: %s - system information state NOT restored. Content read:\n%s"), sysFileName, sysSize, error.c_str(), json->c_str());
            delete json;
            return;
        }
        //const fields
        String bldVersion = doc[csBuildVersion];
        String brdName = doc[csBoardName];
        String bldTime = doc[csBuildTime];
        auto gitBranch = doc[csScmBranch].as<String>();
        if (bldVersion.equals(sysInfo->buildVersion) && doc[csWdReboots].is<JsonArray>()) {
            auto wdReboots = doc[csWdReboots].as<JsonArray>();
            for (JsonVariant i: wdReboots)
                sysInfo->wdReboots.push(i.as<time_t>());
        } else
            Log.warn(F("Build version change detected - previous watchdog reboot timestamps %s have been discarded"), doc[csWdReboots].as<String>().c_str());
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
        Log.info(F("System Information restored from %s [%d bytes]: boardName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%X (last %X), IP=%s, Gateway=%s"),
                   sysFileName, sysSize, brdName.c_str(), bldVersion.c_str(), bldTime.c_str(), gitBranch.c_str(), sysInfo->boardId.c_str(), sysInfo->secElemId.c_str(), sysInfo->macAddress.c_str(), sysInfo->status, lastStatus,
                   sysInfo->strIpAddress.c_str(), sysInfo->strGatewayIpAddress.c_str());
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
    const auto reboots = doc[csWdReboots].to<JsonArray>();
    for (auto & t : sysInfo->wdReboots)
        reboots.add(t);

    auto str = new String();    //larger temporary string, put it on the heap
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!writeTextFile(sysFileName, str))
        Log.error(F("Failed to create/write the system information file %s"), sysFileName);
    delete str;
}
