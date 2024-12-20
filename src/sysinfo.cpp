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

static constexpr char unknown[] PROGMEM = "N/A";
static constexpr char threadInfoFmt[] PROGMEM = "[%u] %s:: time=%s [%u%%] priority(c.b)=%u.%u state=%s id=%u core=%#X stackSize=%u free=%u\n";
static constexpr char heapStackInfoFmt[] PROGMEM = "HEAP/STACK INFO\n  Total Stack:: size=%lu free: tasks=%lu, system=%d;\n  Total Heap:: size=%d free=%d used=%d\n";
static constexpr char sysInfoFmt[] PROGMEM = "SYSTEM INFO\n  CPU ROM %d [%.1f MHz] CORE %d\n  FreeRTOS version %s\n  Arduino PICO version %s [SDK %s]\n  Board UID 0x%s name '%s'\n  MAC Address %s\n  Device name %s\n  Flash size %u";
static constexpr char csBuildVersion[] PROGMEM = "buildVersion";
static constexpr char csBoardName[] PROGMEM = "boardName";
static constexpr char csBuildTime[] PROGMEM = "buildTime";
static constexpr char csScmBranch[] PROGMEM = "scmBranch";
static constexpr char csWdReboots[] PROGMEM = "wdReboots";
static constexpr char csBoardId[] PROGMEM = "boardId";
static constexpr char csSecElemId[] PROGMEM = "secElemId";
static constexpr char csMacAddress[] PROGMEM = "macAddress";
static constexpr char csWifiFwVersion[] PROGMEM = "wifiFwVersion";
static constexpr char csIpAddress[] PROGMEM = "ipAddress";
static constexpr char csGatewayAddress[] PROGMEM = "gatewayIpAddress";
static constexpr char csHeapSize[] PROGMEM = "heapSize";
static constexpr char csFreeHeap[] PROGMEM = "freeHeap";
static constexpr char csStackSize[] PROGMEM = "stackSize";
static constexpr char csFreeStack[] PROGMEM = "freeStack";
static constexpr char csStatus[] PROGMEM = "status";
static constexpr char csReady[] PROGMEM = "Ready";
static constexpr char csBlocked[] PROGMEM = "Blocked";
static constexpr char csSuspended[] PROGMEM = "Suspended";
static constexpr char csDeleted[] PROGMEM = "Deleted";
static constexpr char csRunning[] PROGMEM = "Running";
static constexpr char csInvalid[] PROGMEM = "Invalid";
static constexpr char csUnknown[] PROGMEM = "Unknown";
static constexpr char csPowerOn[] PROGMEM = "power on";
static constexpr char csPinReset[] PROGMEM = "pin reset";
static constexpr char csSoftReset[] PROGMEM = "soft reset";
static constexpr char csWatchdog[] PROGMEM = "watchdog";
static constexpr char csDebug[] PROGMEM = "debug";
static constexpr char csGlitch[] PROGMEM = "glitch";
static constexpr char csBrownout[] PROGMEM = "brownout";

//#define STORAGE_CMD_TOTAL_BYTES 32

unsigned long prevStatTime = 0;
unsigned long prevIdleTime = 0;
SysInfo *sysInfo;

const char *taskStatusToString(const eTaskState state) {
    switch (state) {
        case eReady: return csReady;
        case eBlocked: return csBlocked;
        case eSuspended: return csSuspended;
        case eDeleted: return csDeleted;
        case eRunning: return csRunning;
        case eInvalid: return csInvalid;
        default: return csUnknown;
    }
}

/**
 * Logs detailed information about FreeRTOS task statistics and heap usage.
 * This method retrieves and processes data on tasks and heap memory allocation,
 * providing valuable insights for debugging and performance optimization.
 *
 * The task information includes details such as the name of each task, state, priority,
 * runtime percentages, stack usage, core affinity, and runtime counters across cores.
 *
 * Heap statistics report total stack size, free stack space, heap size, and usage.
 *
 * Key actions:
 * 1. Captures the number of active FreeRTOS tasks and allocates memory for task status data.
 * 2. Obtains task runtime metrics, including runtime counters broken down by cores.
 * 3. Computes percentage runtime of individual tasks relative to the total runtime.
 * 4. Logs per-task details formatted as a readable string.
 * 5. Frees dynamically allocated memory for task status data.
 * 6. Reports task-related and heap details as formatted logs.
 *
 * Note: This method performs critical memory allocations such as `pvPortMalloc` for task data
 * and ensures proper cleanup using `vPortFree`. Care should be taken when modifying to avoid
 * memory leaks or division by zero errors.
 *
 * This function references:
 * - FreeRTOS API: `uxTaskGetSystemState`, `vTaskList`, `uxTaskGetNumberOfTasks`.
 * - Scheduler tasks: Uses `Scheduler.getTask` to get additional task-related metadata.
 * - Utility formatting: Employs utility functions like `StringUtils::append` for creating compact logs.
 *
 * Hardware Dependencies:
 * - RP2040 chip-specific functions like `rp2040.getFreeStack`, `rp2040.getTotalHeap`, etc.
 *
 * This method is resource-intensive and should only be run in contexts where such
 * overhead does not impact the system performance significantly.
 */
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
        Log.info("Total run time reported by system: %lu", ulTotalRunTime);

        /* Avoid divide by zero errors. */
        if( ulTotalRunTime > 100 ) {
            // need to figure out task run time per core - raw results show that the total runtime above does not equal with all tasks runtime counter added up
            uint64_t coresTotalRunTime[2]{};    //using 64 bit longs, summing up all the runtime counters for tasks overflows the 32 bit numbers
            for( volatile UBaseType_t x = 0; x < uxArraySize; x++ ) {
                const TaskStatus_t ts = pxTaskStatusArray[x];
                coresTotalRunTime[0] += (ts.uxCoreAffinityMask & CORE_0 ? ts.ulRunTimeCounter : 0);
                coresTotalRunTime[1] += (ts.uxCoreAffinityMask & CORE_1 ? ts.ulRunTimeCounter : 0);
            }
            Log.info("Total run time reported by individual tasks per core: CORE 0=%llu; CORE 1=%llu", coresTotalRunTime[0], coresTotalRunTime[1]);
            ulTotalRunTime /= 100UL;    // For percentage calculations
            coresTotalRunTime[0] /= 100UL;
            coresTotalRunTime[1] /= 100UL;
            String strTaskInfo;
            strTaskInfo.reserve(2304);  //ensure enough space to avoid reallocations for each thread
            StringUtils::append(strTaskInfo, F("THREADS/TASKS INFO - max priority %hd\n"), configMAX_PRIORITIES-1);
            /* For each populated position in the pxTaskStatusArray array, format the raw data as human-readable ASCII data. */
            for( volatile UBaseType_t x = 0; x < uxArraySize; x++ ) {
                const TaskStatus_t ts = pxTaskStatusArray[x];
                // What percentage of the total run time has the task used? ulTotalRunTimeDiv100 has already been divided by 100.
                // these are huge numbers (ticks?) - we'll divide both by 256 to have more reasonable values
                uint64_t ref = ((ts.uxCoreAffinityMask & CORE_0 ? coresTotalRunTime[0] : 0) + (ts.uxCoreAffinityMask & CORE_1 ? coresTotalRunTime[1] : 0)) >> 8;
                double fStatsAsPercentage = (ts.ulRunTimeCounter >> 8) / (double) ref;
                char strStat[7];
                size_t szStat = snprintf(strStat, 7, "%.2f", fStatsAsPercentage);
                strStat[szStat] = 0;    //ensure null terminator
                //is this a task we created? if so, we have extra information - like stack size
                const TaskWrapper *tw = Scheduler.getTask(ts.xTaskNumber);
                const uint32_t stackSize = tw ? tw->getStackSize() : 0;
                StringUtils::append(strTaskInfo, threadInfoFmt, x, ts.pcTaskName, strStat, ts.ulRunTimeCounter, ts.uxCurrentPriority, ts.uxBasePriority,
                    taskStatusToString(ts.eCurrentState), ts.xTaskNumber, ts.uxCoreAffinityMask, stackSize, ts.usStackHighWaterMark);
                // the pxEndOfStack and pxStackBase fields of TaskStatus are always of value 0xA5A5A5A5 even though the flag configRECORD_STACK_HIGH_ADDRESS has been enabled
                // StringUtils::append(strTaskInfo,F("  stack end %#X begin %#X free %d min %u\n"), (*ts.pxEndOfStack), (*ts.pxStackBase), ((int32_t)(*ts.pxEndOfStack)-(int32_t)(*ts.pxStackBase)), ts.usStackHighWaterMark);
                ulTotalStack += stackSize;
                ulFreeStack += ts.usStackHighWaterMark;
            }
            Log.info(strTaskInfo.c_str());
        }
        /* The array is no longer needed, free the memory it consumes. */
        vPortFree( pxTaskStatusArray );
    }
    String strHeapInfo;
    strHeapInfo.reserve(256);  //ensure enough space to avoid reallocations
    StringUtils::append(strHeapInfo, heapStackInfoFmt, ulTotalStack, ulFreeStack, rp2040.getFreeStack(), rp2040.getTotalHeap(), rp2040.getFreeHeap(), rp2040.getUsedHeap());
    StringUtils::append(strHeapInfo, F("  Stack pointer: start %#X, free %d"), rp2040.getStackPointer(), rp2040.getFreeStack());
    Log.info(strHeapInfo.c_str());
    // HeapStats_t heap_stats;
    // vPortGetHeapStats(&heap_stats);
    // Log.info("\nHeap Stats\nfree %zu bytes; minFree %zu bytes; blocks free %zu [%zu -> %zu]; allocs %zu, frees %zu; ",
    //     heap_stats.xAvailableHeapSpaceInBytes, heap_stats.xMinimumEverFreeBytesRemaining, heap_stats.xNumberOfFreeBlocks, heap_stats.xSizeOfLargestFreeBlockInBytes,
    //     heap_stats.xSizeOfSmallestFreeBlockInBytes, heap_stats.xNumberOfSuccessfulAllocations, heap_stats.xNumberOfSuccessfulFrees);

    auto *stats = new String();
    stats->reserve(1024);       // doc states ~40bytes per task, we have 12 tasks
    stats->concat(F("\nName\tSt\tPr\tStk\tNum\tCore\n"));
    vTaskList(stats->begin());
    Log.info(stats->c_str());

    // Log.info(F("Current watchdog remaining value %u us"), watchdog_get_time_remaining_ms());
}

/**
 * See resetReason_t enum definition in RP2040Support.h
 * @param reason resetReason_t numeric enum value of reset reason
 * @return string representation of the reset reason numeric value
 */
const char *resetReasonToString(const RP2040::resetReason_t reason) {
    switch (reason) {
        case RP2040::UNKNOWN_RESET: return csUnknown;
        case RP2040::PWRON_RESET: return csPowerOn;
        case RP2040::RUN_PIN_RESET: return csPinReset;
        case RP2040::SOFT_RESET: return csSoftReset;
        case RP2040::WDT_RESET: return csWatchdog;
        case RP2040::DEBUG_RESET: return csDebug;
        case RP2040::GLITCH_RESET: return csGlitch;
        case RP2040::BROWNOUT_RESET: return csBrownout;
        default: return unknown;
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
    snprintf(buf, 16, "%3luD %2luH %2lum", uptime/86400000l, (uptime/3600000l%24), (uptime/60000%60));
    Log.info(F("System state: %#hX; uptime %s"), sysInfo->getSysStatus(), buf);
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
        x += snprintf(buf+x, 4, "%02X:", b);
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
            Log.error(F("Error reading the system information JSON file %s [%zu bytes]: %s - system information state NOT restored. Content read:\n%s"), sysFileName, sysSize, error.c_str(), json->c_str());
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
        Log.info(F("System Information restored from %s [%d bytes]: boardName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%#hhX (last %#hhX), IP=%s, Gateway=%s"),
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
