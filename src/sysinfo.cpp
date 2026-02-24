// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SchedulerExt.h>
#include <FastLED.h>
#include "hardware/watchdog.h"
#include "filesystem.h"
#include "util.h"
#include "sysinfo.h"
#include "config.h"
#include "timeutil.h"
#include "version.h"
#include "constants.hpp"
#include "log.h"
#if LOGGING_ENABLED == 1
#include <stringutils.h>
#endif

#define BUF_ID_SIZE  20

#if LOGGING_ENABLED == 1
// static constexpr char threadInfoFmt[] = "[%u] %s:: time=%s [%u%%] priority(c.b)=%u.%u state=%s id=%u core=%#X stackSize=%u free=%u\n";
static constexpr auto heapStackInfoFmt = "HEAP/STACK INFO\n  Stack     :: ptr=%#X;\n  Heap      :: size=%zu used=%zu free=%zu lowest=%zu block max/min/free=%zu/%zu/%zu\n";
static constexpr auto heapPSRAMInfoFmt = "  PSRAM Heap:: PSRAM=%zu size=%d (free=%d used=%d)\n";
static constexpr auto sysInfoFmt = "SYSTEM INFO\n  CPU ROM %d [%.1f MHz] CORE %d\n  FreeRTOS version %s\n  Arduino PICO version %s [SDK %s]\n  Board UID 0x%s name '%s'\n  MAC Address %s\n  Device name %s build version %s at %s\n  Flash size %u";
static constexpr auto fmtTaskInfo = "%-10s\t%s\t%u%c\t%-6u  %-4u\t0x%02x  %-12llu  %.2f %%\n";
static constexpr auto fmtTotalCPULoad = "\nTotal CPU Load:    %.2f %% / %.2f s\n";
#endif
static constexpr auto unknown = "N/A";
static constexpr auto idleTaskMarker = "idle";
constexpr CRGB CLR_ALL_OK = CRGB::Indigo;
constexpr CRGB CLR_SETUP_IN_PROGRESS = CRGB::Green;
constexpr CRGB CLR_UPGRADE_PROGRESS = CRGB::Blue;
constexpr CRGB CLR_SETUP_ERROR = CRGB::Red;

unsigned long prevStatTime = 0;
unsigned long prevIdleTime = 0;
SysInfo *sysInfo;
static TaskStatus_t *prevTaskStatusArray = nullptr;
static TaskStatus_t *curTaskStatusArray = nullptr;

// constexpr TaskDef stLedTasks {nullptr, state_led_run, 384, "LED", 3, CORE_0};

const char *taskStatusToString(const eTaskState state) {
    switch (state) {
        case eReady: return csReady;
        case eBlocked: return csBlocked;
        case eSuspended: return csSuspended;
        case eDeleted: return csDeleted;
        case eRunning: return csRunning;
        case eInvalid: return csInvalid;
        default: return unknown;
    }
}

TaskStatus_t* findTaskStatus(TaskStatus_t* taskStatusArray, const UBaseType_t arraySize, const UBaseType_t taskNumber) {
    for (UBaseType_t i = 0; i < arraySize; i++) {
        if (taskStatusArray[i].xTaskNumber == taskNumber) {
            return &taskStatusArray[i];
        }
    }
    return nullptr;
}

static int compareTasksByNumber(const void* a, const void* b) {
    const auto* taskA = static_cast<const TaskStatus_t*>(a);
    const auto* taskB = static_cast<const TaskStatus_t*>(b);
    return static_cast<int>(taskA->xTaskNumber) - static_cast<int>(taskB->xTaskNumber);
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
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    static uint64_t prevTaskStatsTime = 0ul;
    static unsigned long prevSysTime = 0ul;
    /* Take a snapshot of the number of tasks in case it changes while this function is executing. */
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    /* Allocate a TaskStatus_t structure for each task. An array could be allocated statically at compile time. */
    if(curTaskStatusArray = new TaskStatus_t[uxArraySize]; curTaskStatusArray != nullptr ) {
        String strTaskInfo;
        strTaskInfo.reserve(1024);  //ensure enough space to avoid reallocations for each thread - 64 bytes per task * 15 tasks = 960

        // Generate raw status information about each task. Refs:
        // https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/01-uxTaskGetSystemState
        configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0;
        uxArraySize = uxTaskGetSystemState( curTaskStatusArray, uxArraySize, &ulTotalRunTime );
        qsort(curTaskStatusArray, uxArraySize, sizeof(TaskStatus_t), compareTasksByNumber);
        uint64_t uxTotalRunTime = 0ul;  // Summing up times spent by ALL tasks (as reported by each task) should account for NUM_CORES - this value should be NUM_CORES*ulTotalRunTime
        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            uxTotalRunTime += (curTaskStatusArray[x].ulRunTimeCounter);
        }
        uint64_t uxDeltaTime = uxTotalRunTime - prevTaskStatsTime;    //this accounts for number of cores

        StringUtils::append(strTaskInfo, F("TASK STATS [sys total run time %llu, delta cycles %llu, current time %lu ms, %s\n"), ulTotalRunTime, uxDeltaTime, millis(), TimeFormat::asStringMs(nowMillis()).c_str());
        StringUtils::append(strTaskInfo, F("total CPU cycles 32/64bit %lu / %llu, total task cycles cur/prev %llu / %llu, CPU frequency %d Hz]\n"),
            rp2040.getCycleCount(), rp2040.getCycleCount64(), uxTotalRunTime, prevTaskStatsTime, sysInfo->getCPUFrequency());
        strTaskInfo.concat(F("Name      \tSt \tPr \tStk     Num \tCore  RunTime       RunPct\n"));
        uxDeltaTime /= 100; //prepares for percentage calculation
        double fTotalCPULoadPercentage = 0.0;
        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            const TaskStatus_t *prevTaskStatus = prevTaskStatusArray != nullptr ? findTaskStatus(prevTaskStatusArray, uxArraySize, curTaskStatusArray[x].xTaskNumber) : nullptr;
            const uint64_t taskDeltaTime = prevTaskStatus != nullptr ? (curTaskStatusArray[x].ulRunTimeCounter - prevTaskStatus->ulRunTimeCounter) : curTaskStatusArray[x].ulRunTimeCounter;
            const double fStatsAsPercentage = uxDeltaTime > 0 ? static_cast<double>(taskDeltaTime) / static_cast<double>(uxDeltaTime) : 0.0;
            //only add non-IDLE task percentages to total CPU load
            String taskName(curTaskStatusArray[x].pcTaskName);
            taskName.toLowerCase();
            if (taskName.indexOf(idleTaskMarker) < 0)
                fTotalCPULoadPercentage += fStatsAsPercentage;
            const char prElevated = curTaskStatusArray[x].uxCurrentPriority > curTaskStatusArray[x].uxBasePriority ? '+' : curTaskStatusArray[x].uxCurrentPriority < curTaskStatusArray[x].uxBasePriority ? '-' : ' ';
            const uint coreAffinity = curTaskStatusArray[x].uxCoreAffinityMask >= CORE_ALL ? CORE_ALL : curTaskStatusArray[x].uxCoreAffinityMask;
            char buf[80];
            snprintf(buf, 80, fmtTaskInfo, curTaskStatusArray[x].pcTaskName, taskStatusToString(curTaskStatusArray[x].eCurrentState),
                (uint)curTaskStatusArray[ x ].uxCurrentPriority, prElevated, (uint)curTaskStatusArray[ x ].usStackHighWaterMark,
                (uint)curTaskStatusArray[ x ].xTaskNumber, coreAffinity, taskDeltaTime, fStatsAsPercentage);
            strTaskInfo.concat(buf);
        }
        /* The array is no longer needed, free the memory it consumes. */
        delete[] prevTaskStatusArray;
        prevTaskStatusArray = curTaskStatusArray;
        prevTaskStatsTime = uxTotalRunTime;
        //add the total CPU load
        unsigned long curSysTime = millis();
        float fTimeWindow = (curSysTime - prevSysTime) / 1000.0f;
        prevSysTime = curSysTime;
        char buf[80];
        snprintf(buf, 80, fmtTotalCPULoad, fTotalCPULoadPercentage, fTimeWindow);
        strTaskInfo.concat(buf);
        log_info(strTaskInfo.c_str());
    }
    // Simple heap stats
    logHeapStats();
    struct mallinfo mf = mallinfo();
    log_info(F("Malloc memory stats: allocated=%u, used=%u, free=%u"), mf.arena, mf.uordblks, mf.fordblks);

    log_info(F("Minimum log buffer free space %zu bytes"), Log.getMinBufferSpace());
    // log_info(F("Current watchdog remaining value %u us"), watchdog_get_time_remaining_ms());

#endif
}

/**
 * Log Heap memory allocation stats
 */
void logHeapStats() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;    // Simple heap stats
    String strHeapInfo;
    strHeapInfo.reserve(256);  //ensure enough space to avoid reallocations
    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);
    StringUtils::append(strHeapInfo, heapStackInfoFmt, rp2040.getStackPointer(), configTOTAL_HEAP_SIZE, (configTOTAL_HEAP_SIZE-heapStats.xAvailableHeapSpaceInBytes), heapStats.xAvailableHeapSpaceInBytes,
        heapStats.xMinimumEverFreeBytesRemaining, heapStats.xSizeOfLargestFreeBlockInBytes, heapStats.xSizeOfSmallestFreeBlockInBytes, heapStats.xNumberOfFreeBlocks);
#ifdef PICO_RP2350
    StringUtils::append(strHeapInfo, heapPSRAMInfoFmt, rp2040.getPSRAMSize(), rp2040.getTotalPSRAMHeap(), rp2040.getFreePSRAMHeap(), rp2040.getUsedPSRAMHeap());
#endif
    log_info(strHeapInfo.c_str());
#endif
}

/**
 * See resetReason_t enum definition in RP2040Support.h
 * @param reason resetReason_t numeric enum value of reset reason
 * @return string representation of the reset reason numeric value
 */
const char *resetReasonToString(const RP2040::resetReason_t reason) {
    switch (reason) {
        case RP2040::UNKNOWN_RESET: return unknown;
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

const char *resetMarkerToString(const uint32_t marker) {
    switch (marker) {
        case kResetMarkerNone: return "none";
        case kResetMarkerPanic: return "panic";
        case kResetMarkerAssert: return "assert";
        case kResetMarkerHardFault: return "hardfault";
        case kResetMarkerMalloc: return "malloc_failed";
        case kResetMarkerStackOverflow: return "stack_overflow";
        case kResetMarkerFxStall: return "fx_stall";
        case kResetMarkerOta: return "ota";
        case kResetMarkerReboot: return "reboot";
        case kResetMarkerUnknown: return "unknown";
        default: return "other";
    }
}

const char *fxStageToString(const uint32_t stage) {
    switch (stage) {
        case kFxStageNone: return "none";
        case kFxStageEnter: return "enter";
        case kFxStageAfterQueue: return "after_queue";
        case kFxStageAfterOtaCheck: return "after_ota_check";
        case kFxStageFirmwareUpgrade: return "fw_upgrade";
        case kFxStageBeforeLoop: return "before_loop";
        case kFxStageAfterLoop: return "after_loop";
        case kFxStageAfterPing: return "after_ping";
        default: return "unknown";
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
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    log_info(sysInfoFmt, rp2040_rom_version(), RP2040::f_cpu()/1000000.0, RP2040::cpuid(), tskKERNEL_VERSION_NUMBER, ARDUINO_PICO_VERSION_STR, PICO_SDK_VERSION_STRING,
               sysInfo->getBoardId().c_str(), BOARD_NAME, sysInfo->getMacAddress().c_str(), DEVICE_NAME, sysInfo->getBuildVersion().c_str(), sysInfo->getBuildTime().c_str(),
               sysInfo->get_flash_capacity());
    log_info(F("System reset reason %s"), resetReasonToString(rp2040.getResetReason()));
    const uint32_t resetMarker = watchdog_hw->scratch[kResetMarkerScratchIndex];
    log_info(F("System reset marker %s (0x%08lX)"), resetMarkerToString(resetMarker), resetMarker);
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerNone;
    const uint32_t fxHeartbeat = watchdog_hw->scratch[kFxHeartbeatScratchIndex];
    log_info(F("FX heartbeat marker 0x%08lX"), fxHeartbeat);
    watchdog_hw->scratch[kFxHeartbeatScratchIndex] = 0u;
    const uint32_t fxStage = watchdog_hw->scratch[kFxStageScratchIndex];
    log_info(F("FX stage marker %s (0x%08lX)"), fxStageToString(fxStage), fxStage);
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageNone;
#if DIAG_CORE_HEARTBEATS
    const uint32_t core0Heartbeat = watchdog_hw->scratch[kCore0HeartbeatScratchIndex];
    log_info(F("CORE0 heartbeat marker 0x%08lX"), core0Heartbeat);
    watchdog_hw->scratch[kCore0HeartbeatScratchIndex] = 0u;
    const uint32_t core1Heartbeat = watchdog_hw->scratch[kCore1HeartbeatScratchIndex];
    log_info(F("CORE1 heartbeat marker 0x%08lX"), core1Heartbeat);
    watchdog_hw->scratch[kCore1HeartbeatScratchIndex] = 0u;
#endif

    //interesting memory pointers from pico-sdk/src/rp2_common/pico_crt0/rp2040/memmap_default.ld
    extern char __exidx_start;
    extern char __exidx_end;
    extern char __etext;
    extern char __data_start__;
    extern char __preinit_array_start;
    extern char __preinit_array_end;
    extern char __init_array_start;
    extern char __init_array_end;
    extern char __fini_array_start;
    extern char __fini_array_end;
    extern char __data_end__;
    extern char __bss_start__;
    extern char __bss_end__;
    extern char __end__;
    extern char __HeapLimit;
    extern char __StackLimit;
    extern char __StackTop;
    extern uint32_t __scratch_x_start__;
    extern uint32_t __scratch_y_start__;
    extern uint32_t* core1_separate_stack_address;
    log_info(F("Memory map pointers:"));
    log_info(F("  .text end:            __etext       = %#X"), (uint32_t)&__etext);
    log_info(F("  .data start/end:      __data_start__/__data_end__ = %#X/%#X"), (uint32_t)&__data_start__, (uint32_t)&__data_end__);
    log_info(F("  .bss start/end:       __bss_start__/__bss_end__   = %#X/%#X"), (uint32_t)&__bss_start__, (uint32_t)&__bss_end__);
    log_info(F("  .exidx start/end:     __exidx_start__/__exidx_end__ = %#X/%#X"), (uint32_t)&__exidx_start, (uint32_t)&__exidx_end);
    log_info(F("  .preinit_array start/end: __preinit_array_start__/__preinit_array_end__ = %#X/%#X"), (uint32_t)&__preinit_array_start, (uint32_t)&__preinit_array_end);
    log_info(F("  .init_array start/end:    __init_array_start__/__init_array_end__     = %#X/%#X"), (uint32_t)&__init_array_start, (uint32_t)&__init_array_end);
    log_info(F("  .fini_array start/end:    __fini_array_start__/__fini_array_end__     = %#X/%#X"), (uint32_t)&__fini_array_start, (uint32_t)&__fini_array_end);
    log_info(F("  Program end markers:  __end__       = %#X"), (uint32_t)&__end__);
    log_info(F("  Heap limits:          __HeapLimit   = %#X"), (uint32_t)&__HeapLimit);
    log_info(F("  Stack limits:         __StackLimit  = %#X; __StackTop = %#X"), (uint32_t)&__StackLimit, (uint32_t)&__StackTop);
    log_info(F("  Scratch RAM start:    __scratch_x_start__ = %#X; __scratch_y_start__ = %#X"), __scratch_x_start__, __scratch_y_start__);
    log_info(F("  Core 1 separate stack address = %#X"), (uint32_t)*core1_separate_stack_address);
#endif
}

/**
 * Logs the current system state, including system status and formatted uptime.
 */
void logSystemState() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    char buf[20];
    const unsigned long uptime = millis();
    snprintf(buf, 16, "%3luD %2luH %2lum", uptime/86400000l, (uptime/3600000l%24), (uptime/60000%60));
    log_info(F("System state: %#hX; uptime %s"), sysInfo->getSysStatus(), buf);
#endif
}

// SysInfo
SysInfo::SysInfo() : boardName(BOARD_NAME), deviceName(DEVICE_NAME), buildVersion(BUILD_VERSION), buildTime(BUILD_TIME), scmBranch(GIT_BRANCH),
                     ipAddress({IP_ADDR}), ipGateway({IP_GW}) {
    boardId.reserve(BUF_ID_SIZE);       // flash unique ID in hex, \0 terminator
    secElemId.reserve(BUF_ID_SIZE);     // 18 from ECCX08Class::serialNumber() implementation
    macAddress.reserve(BUF_ID_SIZE);    // 6 (WL_MAC_ADDR_LENGTH) groups of 2 hex digits and ':' separator, includes \0 terminator
    strIpAddress.reserve(BUF_ID_SIZE);     // 4 groups of 3 digits, 3 '.' separators, \0 terminator
    wifiFwVersion.reserve(BUF_ID_SIZE); // typical semantic version e.g., v1.5.0, 3 groups of 2 digits, '.' separator, \0 terminator
    ssid.reserve(BUF_ID_SIZE);          // initial space, most networks are short names
    cpuFrequency = 0;
    cpuVersion = 0;
    cpuModel.reserve(BUF_ID_SIZE);
    psramSize = 0;
    status = SysStatus::None;
    cleanBoot = true;
}

/**
 * Capture Flash UID as board unique identifier in HEX string format
 */
void SysInfo::fillBoardId() {
    boardId = rp2040.getChipID();
    cpuFrequency = RP2040::f_cpu();
#ifdef PICO_RP2350
    cpuModel = "RP2350";
    cpuVersion = rp2350_chip_version();
    psramSize = rp2040.getPSRAMSize();
#elifdef ARDUINO_ARCH_RP2040
    cpuModel = "RP2040";
    cpuVersion = rp2040_rom_version();
#endif

}

/**
 * <p>Per AT25SF128A Flash specifications, command ox9F returns 0x1F8901, where last byte 0x01 should represent density (size)
 * Trial/error shows the command returns 0xFF1F89011F</p>
 * <p>We won't use the flash chip SPI commands - very chip-specific - but instead leverage the constant PICO_FLASH_SIZE_BYTES already tailored to the board we use, Nano RP2040</p>
 * @return Board flash size in bytes - currently fixed at PICO_FLASH_SIZE_BYTES
 */
uint SysInfo::get_flash_capacity() const {
//    uint8_t txbuf[STORAGE_CMD_TOTAL_BYTES] = {0x9f};
//    uint8_t rxbuf[STORAGE_CMD_TOTAL_BYTES] = {0};
//    flash_do_cmd(txbuf, rxbuf, STORAGE_CMD_TOTAL_BYTES);

//    return 1 << rxbuf[3];
    return PICO_FLASH_SIZE_BYTES;
}

SysStatus SysInfo::setSysStatus(const SysStatus bitMask) {
    CoreMutex coreMutex(&mutex);
    status |= bitMask;
    return status;
}

SysStatus SysInfo::resetSysStatus(const SysStatus bitMask) {
    CoreMutex coreMutex(&mutex);
    status &= (~bitMask);
    return status;
}

bool SysInfo::isSysStatus(const SysStatus bitMask) const {
    return (status & bitMask) == bitMask;
}

SysStatus SysInfo::getSysStatus() const {
    return status;
}

/**
 * Extracts identification information from a connected Wi-Fi.
 * NOTE: For reasons unknown yet, reading the network information from the Wi-Fi subsystem (through SPI connection) freezes the whole system
 * if the Analog/Digital Converter API calls are present somewhere else in the code. These Wi-Fi network information calls have SPI responses with 3 parameters,
 * unsure if this is a factor in failure - all other SPI calls with Wi-Fi module seem to be working fine, and they have fewer parameters.
 * For this reason, the workaround is to record the IP address and Gateway Address from the configuration provided (we're using static IP assignment) rather
 * than retrieving from the Wi-Fi module.
 * @param wifi the Wi-Fi (global) object
 */
void SysInfo::setWiFiInfo(::WiFiClass &wifi) {
    ssid = wifi.SSID();
    wifiFwVersion = ::WiFiClass::firmwareVersion();
    strIpAddress = wifi.localIP().toString();
    strGatewayIpAddress = wifi.gatewayIP().toString();
    // strIpAddress = ipAddress.toString();
    // strGatewayIpAddress = ipGateway.toString();

    const IPAddress dns1 = wifi.dnsIP(0);
    const IPAddress dns2 = wifi.dnsIP(1);
    log_info(F("WiFi DNS servers: %s, %s"), dns1.toString().c_str(), dns2.toString().c_str());

    //MAC address - Formats the MAC address into the character buffer provided; space for 20 chars is needed (includes nul terminator)
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    wifi.macAddress(mac);
    char buf[BUF_ID_SIZE];
    int x = 0;
    for (const auto &b : mac)
        x += snprintf(buf+x, 4, "%02X:", b);
    //the last character - at index x-1 is a ':', make it null to trim the last colon character
    buf[x-1] = 0;
    macAddress = buf;
}

void SysInfo::setSecureElementId(const String &secId) {
    secElemId = secId;
}

void SysInfo::sysConfig(JsonDocument &doc) {
    doc["arduinoPicoVersion"] = ARDUINO_PICO_VERSION_STR;
    doc["freeRTOSVersion"] = tskKERNEL_VERSION_NUMBER;
    doc[csBoardName] = sysInfo->boardName;
    doc[csDeviceName] = sysInfo->deviceName;
    doc[csBoardId] = sysInfo->boardId;
    doc["psramSize"] = sysInfo->psramSize;
    doc["cpuModel"] = sysInfo->cpuModel;
    doc["cpuVersion"] = sysInfo->cpuVersion;
    doc["cpuFrequency"] = sysInfo->cpuFrequency;
    doc["cpuCore"] = RP2040::cpuid();
    doc[csSecElemId] = sysInfo->secElemId;
    doc[csBuildVersion] = sysInfo->buildVersion;
    doc[csScmBranch] = sysInfo->scmBranch;
    doc[csBuildTime] = sysInfo->buildTime;
    doc[csWifiFwVersion] = sysInfo->wifiFwVersion;
    doc["wifiLatestVersion"] = WIFI_FIRMWARE_LATEST_VERSION;
    doc[csMacAddress] = sysInfo->macAddress;
    doc[csIpAddress] = sysInfo->strIpAddress;
    doc[csGatewayAddress] = sysInfo->strGatewayIpAddress;
    doc[csStatus] = static_cast<uint16_t>(sysInfo->status);
    doc[csHeapSize] = sysInfo->heapSize;
    doc[csFreeHeap] = sysInfo->freeHeap;
    doc[csStackSize] = sysInfo->stackSize;
    doc[csFreeStack] = sysInfo->freeStack;
    const auto reboots = doc[csWdReboots].to<JsonArray>();
    for (auto & t : sysInfo->wdReboots)
        (void)reboots.add(t);
}

/**
 * Generate Heap memory utilization statistics in JSON form
 * @param doc JSON object to populate
 */
void SysInfo::heapStats(JsonObject &doc) {
    // Simple heap stats
    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);

    doc["stackPointer"] = rp2040.getStackPointer();
    doc["freeStack"] = sysInfo->freeStack = rp2040.getFreeStack();
    doc["totalHeap"] = sysInfo->heapSize = configTOTAL_HEAP_SIZE;
    doc["freeHeap"] = sysInfo->freeHeap = heapStats.xAvailableHeapSpaceInBytes;
    doc["usedHeap"] = (sysInfo->heapSize - sysInfo->freeHeap);
    doc["minHeap"] = heapStats.xMinimumEverFreeBytesRemaining;
    doc["maxHeapBlock"] = heapStats.xSizeOfLargestFreeBlockInBytes;
    doc["minHeapBlock"] = heapStats.xSizeOfSmallestFreeBlockInBytes;
    doc["freeBlocks"] = heapStats.xNumberOfFreeBlocks;
    doc["psramSize"] = sysInfo->psramSize;
#ifdef PICO_RP2350
    doc["psramHeapTotal"] = rp2040.getTotalPSRAMHeap();
    doc["psramHeapFree"] = rp2040.getFreePSRAMHeap();
    doc["psramHeapUsed"] = rp2040.getUsedPSRAMHeap();
#endif

#if LOGGING_ENABLED == 1
    doc["logMinBufferSpace"] = Log.getMinBufferSpace();
#endif
    //doc["watchdogRemaining"] = watchdog_get_time_remaining_ms();
}

/**
 * Generate task runtime statistics in JSON form
 * @param doc JSON array to populate
 */
void SysInfo::taskStats(JsonObject &doc) {
    static uint64_t prevTaskStatsTime = 0ul;
    /* Take a snapshot of the number of tasks in case it changes while this function is executing. */
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    /* Allocate a TaskStatus_t structure for each task. An array could be allocated statically at compile time.
     * Note the use of new operator that is overridden to engage pvPortMalloc */
    if(curTaskStatusArray = new TaskStatus_t[uxArraySize]; curTaskStatusArray != nullptr ) {
        // General counts
        doc["count"] = uxArraySize;
        const auto jsArray = doc["items"].to<JsonArray>();
        // Refs: https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/01-uxTaskGetSystemState
        configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0;
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState( curTaskStatusArray, uxArraySize, &ulTotalRunTime );
        doc["sysTotalRunTime"] = ulTotalRunTime;
        uint64_t uxTotalRunTime = 0ul;
        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            uxTotalRunTime += (curTaskStatusArray[x].ulRunTimeCounter);
        }
        const uint64_t uxDeltaTime = (uxTotalRunTime - prevTaskStatsTime)/100;    //this accounts for number of cores
        doc["tasksTotalRunTime"] = uxTotalRunTime;
        double fTotalCPULoadPercentage = 0.0;
        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            const TaskStatus_t *prevTaskStatus = findTaskStatus(prevTaskStatusArray, uxArraySize, curTaskStatusArray[x].xTaskNumber);
            JsonObject task = jsArray.add<JsonObject>();
            const uint64_t taskDeltaTime = prevTaskStatus != nullptr ? (curTaskStatusArray[x].ulRunTimeCounter - prevTaskStatus->ulRunTimeCounter) : curTaskStatusArray[x].ulRunTimeCounter;
            const double fStatsAsPercentage = uxDeltaTime > 0 ? static_cast<double>(taskDeltaTime) / static_cast<double>(uxDeltaTime) : 0.0;
            String taskName = curTaskStatusArray[x].pcTaskName;
            taskName.toLowerCase();
            if (taskName.indexOf(idleTaskMarker) < 0)
                fTotalCPULoadPercentage += fStatsAsPercentage;  //only add the non-idle tasks
            const uint coreAffinity = curTaskStatusArray[x].uxCoreAffinityMask >= CORE_ALL ? CORE_ALL : curTaskStatusArray[x].uxCoreAffinityMask;

            task["name"] = curTaskStatusArray[x].pcTaskName;
            task["state"] = taskStatusToString(curTaskStatusArray[x].eCurrentState);
            task["curPriority"] = curTaskStatusArray[ x ].uxCurrentPriority;
            task["basePriority"] = curTaskStatusArray[ x ].uxBasePriority;
            task["stackHighWaterMark"] = curTaskStatusArray[ x ].usStackHighWaterMark;
            task["taskNumber"] = curTaskStatusArray[ x ].xTaskNumber;
            task["coreAffinity"] = coreAffinity;
            task["runTime"] = taskDeltaTime;
            task["runTimeLife"] = curTaskStatusArray[ x ].ulRunTimeCounter;
            task["runTimePct"] = fStatsAsPercentage;
        }
        doc["totalCPULoadPct"] = fTotalCPULoadPercentage;
        /* The array is no longer needed, free the memory it consumes. */
        delete[] prevTaskStatusArray;
        prevTaskStatusArray = curTaskStatusArray;
        prevTaskStatsTime = uxTotalRunTime;
    }
}

/**
 * Read the saved sys info file - no op, if there is no file
 * Note the time this is performed, the WiFi is not ready, nor other fields even populated yet
 */
void readSysInfo() {
    const auto json = new String();
    json->reserve(512);  // approximation
    if (const size_t sysSize = SyncFsImpl.readFile(sysFileName, json); sysSize > 0) {
        log_info(F("System information [%s]:\n%s"), sysFileName, json->c_str());
        JsonDocument doc;
        if (const DeserializationError error = deserializeJson(doc, *json)) {
            log_error(F("Error reading the system information JSON file %s [%zu bytes]: %s - system information state NOT restored. Content read:\n%s"), sysFileName, sysSize, error.c_str(), json->c_str());
            delete json;
            doc.clear();
            return;
        }
        //const fields
        const String bldVersion = doc[csBuildVersion];
        const String brdName = doc[csBoardName];
        const String devName = doc[csDeviceName] | DEVICE_NAME;
        const String bldTime = doc[csBuildTime];
        const auto gitBranch = doc[csScmBranch].as<String>();
        if (bldVersion.equals(sysInfo->buildVersion) && doc[csWdReboots].is<JsonArray>()) {
            const auto wdReboots = doc[csWdReboots].as<JsonArray>();
            for (JsonVariant i: wdReboots)
                sysInfo->wdReboots.push(i.as<time_t>());
        } else
            log_warn(F("Build version change detected - previous watchdog reboot timestamps %s have been discarded"), doc[csWdReboots].as<String>().c_str());
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
        //do not override the current status (in progress of populating) with the last run status
        const auto lastStatus = doc[csStatus].as<uint16_t>();
        log_info(F("System Information restored from %s [%d bytes]: boardName=%s, deviceName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%#hhX (last %#hhX), IP=%s, Gateway=%s"),
                   sysFileName, sysSize, brdName.c_str(), devName.c_str(), bldVersion.c_str(), bldTime.c_str(), gitBranch.c_str(), sysInfo->boardId.c_str(), sysInfo->secElemId.c_str(), sysInfo->macAddress.c_str(), sysInfo->status,
                   lastStatus, sysInfo->strIpAddress.c_str(), sysInfo->strGatewayIpAddress.c_str());
        doc.clear();
    } else
        log_info(F("System information file %s not found - system information will be re-built"), sysFileName);
    delete json;
}

/**
 * Saves the sys info to the file
 */
void saveSysInfo() {
    JsonDocument doc;
    SysInfo::sysConfig(doc);
    auto str = new String();    //larger temporary string, put it on the heap
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!SyncFsImpl.writeFile(sysFileName, str))
        log_error(F("Failed to create/write the system information file %s"), sysFileName);
    delete str;
    doc.clear();
    //read the fx file and merge it with the sys one into a new sysconfig file - by this time the FX task has created the fx config
    //for merging JsonDocuments see https://arduinojson.org/v7/how-to/merge-json-objects/
    str = new String();
    str->reserve(6144);  // approximation
    SyncFsImpl.readFile(fxCfgFileName, str);
    if (const DeserializationError error = deserializeJson(doc, *str)) {
        log_error(F("Failed to deserialize the FX configuration file %s: %s"), fxCfgFileName, error.c_str());
        delete str;
        doc.clear();
    } else {
        delete str;
        SysInfo::sysConfig(doc);
        str = new String();
        str->reserve(measureJson(doc));
        serializeJson(doc, *str);
        if (!SyncFsImpl.writeFile(sysCfgFileName, str))
            log_error(F("Failed to create/write the system configuration file %s"), sysCfgFileName);
        delete str;
        doc.clear();
    }
}

/**
 * Set-up the on-board status LED
 */
void SysInfo::setupStateLED() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    updateBoardLED(CRGB::Black);    //black, turned off
}
/**
 * Controls the on-board status LED
 * @param colorCode
 */
void SysInfo::updateBoardLED(const uint32_t colorCode) {
    updateBoardLED(CRGB(colorCode));
}

/**
 * Controls the onboard LED using individual values for R, G, B
 * On RPI based boards (Pico 2W, Plasma, etc), the LED(s) are simple GPIO-controlled LED.
 * On Plasma 2350 W, the RGB LED is on GPIO 16, 17, 18
 * On RPi Pico 2 W, the status LED is on GPIO 15
 * @param rgb RGB value
 */
void SysInfo::updateBoardLED(const CRGB rgb) {
    analogWrite(PIN_LED_R, 255 - rgb.red);
    analogWrite(PIN_LED_G, 255 - rgb.green);
    analogWrite(PIN_LED_B, 255 - rgb.blue);
}

/**
 * Adjusts the LED state (color, illumination style) in response to the overall system's state
 */
void SysInfo::updateStatusLED() const {
    const bool isOk = isSysStatus(SysStatus::Wifi | SysStatus::Ntp | SysStatus::Filesystem | SysStatus::Diag);
    const CRGB colorCode = isOk ? CLR_ALL_OK : !isSysStatus(SysStatus::Setup0 | SysStatus::Setup1) ? CLR_SETUP_IN_PROGRESS : CLR_SETUP_ERROR;
    updateBoardLED(colorCode);
}

/**
 * Flash status LED for as long as both cores are in setup mode
 */
void state_led_begin() {
    while (!sysInfo->isSysStatus(SysStatus::Setup0 | SysStatus::Setup1)) {
        SysInfo::updateBoardLED(CRGB::Black);
        taskDelay(640);
        SysInfo::updateBoardLED(CLR_SETUP_IN_PROGRESS);
        taskDelay(640);
    }
}

/**
 * Update the color of the status LED consistent with the system's state
 */
void state_led_update() {
    sysInfo->updateStatusLED();
}

void SysInfo::begin() {
}
