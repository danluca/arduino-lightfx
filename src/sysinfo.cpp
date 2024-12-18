// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "sysinfo.h"
#include <flash.h>
#include <ArduinoJson.h>
#include "util.h"
#include "version.h"
#ifndef DISABLE_LOGGING
#include "diag.h"
#include <rtx_lib.h>
#endif

#define BUF_ID_SIZE  20

static const char unknown[] PROGMEM = "N/A";
static const char threadInfoFmt[] PROGMEM = "Thread[%u]:: name='%s' priority=%i state=%u id=%X stack size=%u free=%u\n";
static const char threadInfoVerboseFmt[] PROGMEM = "Thread[%u]:: name='%s' priority=%i state=%u id=%X entry=%X\n  Stack[0x%U - 0x%U] size=%u free=%u\n";
static const char heapInfoFmt[] PROGMEM = "Heap:: size=%u free=%u\n";
static const char heapInfoVerboseFmt[] PROGMEM = "Heap:: start=%X end=%X size=%u used=%u free=%u\n  maxUsed=%u totalAllocated=%u overhead=%u\n  allocated ok=%u err=%u\n";
//static const char stackInfoFmt[] PROGMEM = "Stack:: size=%u free=%u";
static const char genStackInfoFmt[] PROGMEM = "Gen Stack:: size=%u free=%u statsCount=%u\n";
static const char isrStackInfoFmt[] PROGMEM = "ISR Stack:: start=%X end=%X size=%u\n";
static const char sysInfoFmt[] PROGMEM = "SYSTEM INFO\n  CPU ID %X\n  Mbed OS version %u\n  Compiler ID %X version %u\n  Board UID 0x%s name '%s' vendor %X model %X\n  MAC Address %s\n  Device name %s\n  Flash size %u";
static const char cpuStatsFmt[] PROGMEM = "Time (µs): Uptime: %u Idle: %u Sleep: %u DeepSleep: %u Idle: %d%% Usage: %d%%\n";
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
us_timestamp_t prevIdleTime = 0;
SysInfo *sysInfo;

/**
 * This method and following few aren't really in the basic domain of providing logging capabilities to the system.
 * <p>Nonetheless, the stats they provide took deeper advantage of the logging infrastructure elements for an enhanced
 * look & feel - multi-line, spaced, information about threads, memory, system.</p>
 * @param threadId
 */
void logThreadInfo(osThreadId threadId) {
#ifndef DISABLE_LOGGING
    // Refs: ~\.platformio\packages\framework-arduino-mbed\libraries\mbed-memory-status\mbed_memory_status.cpp#print_thread_info
    // Refs: rtx_lib.h - #define os_thread_t osRtxThread_t
    //       rtx_os.h  - typedef struct osRtxThread_s { } osRtxThread_t

    if (!threadId) return;

    os_thread_t * tcb = (os_thread_t *) threadId;

    uint32_t stackSize = osThreadGetStackSize(threadId);
    uint32_t stackFree = osThreadGetStackSpace(threadId);
    const char* thrdName = osThreadGetName(threadId) ? osThreadGetName(threadId) : unknown;

    if (LOG_LEVEL_TRACE > Log.getLevel())
        Log.infoln(threadInfoFmt, 0, thrdName, (int)threadId, stackSize, stackFree);
    else
        Log.traceln(threadInfoVerboseFmt, 0, thrdName, (int)threadId, tcb->thread_addr, tcb->stack_mem, (uint8_t *)tcb->stack_mem + stackSize, stackSize, stackFree);
#endif
}

void logAllThreadInfo() {
    // Refs: ~\.platformio\packages\framework-arduino-mbed\libraries\mbed-memory-status\mbed_memory_status.cpp#print_all_thread_info
    // Refs: mbed_stats.c - mbed_stats_stack_get_each()

    uint32_t threadCount = osThreadGetCount();
    sysInfo->threadCount = threadCount;

#ifndef DISABLE_LOGGING
    osThreadId_t threads[threadCount]; // g++ will throw a -Wvla on this, but it is likely ok.
    mbed_stats_stack_t stacks[threadCount];

    memset(threads, 0, threadCount * sizeof(osThreadId_t));
    threadCount = osThreadEnumerate(threads, threadCount);
    mbed_stats_stack_get_each(stacks, threadCount);

    Log.info(F("THREADS INFO\n"));
    for (uint32_t i = 0; i < threadCount; i++) {
        osThreadId_t threadId = threads[i];
        if (!threadId) continue;

        os_thread_t * tcb = (os_thread_t *) threadId;
//        uint32_t stackSize = osThreadGetStackSize(threadId);
//        uint32_t stackFree = osThreadGetStackSpace(threadId);
        uint32_t stackSize = stacks[i].reserved_size;
        uint32_t stackFree = stacks[i].reserved_size-stacks[i].max_size;
        const char* thrdName = osThreadGetName(threadId) ? osThreadGetName(threadId) : unknown;

        if (LOG_LEVEL_TRACE > Log.getLevel())
            Log.info(threadInfoFmt, i, thrdName, tcb->priority, tcb->state, (int)threadId, stackSize, stackFree);
        else
            Log.trace(threadInfoVerboseFmt, i, thrdName, tcb->priority, tcb->state, (int)threadId, tcb->thread_addr, tcb->stack_mem, (uint8_t *)tcb->stack_mem+stackSize, stackSize, stackFree);
    }
    Log.endContinuation();
#endif
}

void logHeapAndStackInfo() {
    // Refs: ~\.platformio\packages\framework-arduino-mbed\libraries\mbed-memory-status\mbed_memory_status.cpp#print_heap_and_isr_stack_info
    extern unsigned char * mbed_heap_start;
    extern uint32_t        mbed_heap_size;
    extern uint32_t        mbed_stack_isr_size;
    extern unsigned char * mbed_stack_isr_start;
//    extern char __StackLimit, __bss_end__;

    mbed_stats_heap_t      heap_stats;
    mbed_stats_heap_get(&heap_stats);
    mbed_stats_stack_t allStack;
    mbed_stats_stack_get(&allStack);

    //another way to get free heap
//    uint32_t totalHeap = &__StackLimit - &__bss_end__;      //this matches mbed_heap_size
//    uint32_t freeHeap = totalHeap - mallinfo().uordblks;    //this matches mbed_heap_size-heap_stats.current_size-heap_stats.overhead_size

    // update the system info
    sysInfo->heapSize = mbed_heap_size;
    sysInfo->freeHeap = mbed_heap_size-heap_stats.current_size-heap_stats.overhead_size;
    sysInfo->stackSize = allStack.reserved_size;
    sysInfo->freeStack = allStack.reserved_size-allStack.max_size;

#ifndef DISABLE_LOGGING
    Log.info(F("HEAP & STACK INFO\n"));
    if (LOG_LEVEL_TRACE > Log.getLevel()) {
        Log.info(heapInfoFmt, mbed_heap_size, sysInfo->freeHeap);
        Log.info(genStackInfoFmt, allStack.reserved_size, allStack.reserved_size-allStack.max_size, allStack.stack_cnt);
    } else {
        Log.trace(heapInfoVerboseFmt, mbed_heap_start, heap_stats.reserved_size, mbed_heap_size, heap_stats.current_size,
                    mbed_heap_size-heap_stats.current_size-heap_stats.overhead_size, heap_stats.max_size, heap_stats.total_size, heap_stats.overhead_size,
                    heap_stats.alloc_cnt, heap_stats.alloc_fail_cnt);
        Log.trace(genStackInfoFmt, allStack.reserved_size, allStack.reserved_size-allStack.max_size, allStack.stack_cnt);
        Log.trace(isrStackInfoFmt, mbed_stack_isr_start, mbed_stack_isr_start+mbed_stack_isr_size, mbed_stack_isr_size);
    }
    Log.endContinuation();
#endif
}

void logSystemInfo() {
#ifndef DISABLE_LOGGING
    // this only works if MBED_SYS_STATS_ENABLED is on
    // Refs: https://os.mbed.com/docs/mbed-os/v6.16/apis/mbed-statistics.html
    //       https://github.com/ARMmbed/mbed-os-example-sys-info/blob/mbed-os-6.7.0/main.cpp

    // System info
    mbed_stats_sys_t statsSys;
    mbed_stats_sys_get(&statsSys);

    /* CPUID Register information
    [31:24]Implementer      0x41 = ARM
    [23:20]Variant          Major revision 0x0  =  Revision 0
    [19:16]Architecture     0xC  = Baseline Architecture
                            0xF  = Constant (Mainline Architecture)
    [15:4]PartNO            0xC20 = Cortex-M0
                            0xC60 = Cortex-M0+
                            0xC23 = Cortex-M3
                            0xC24 = Cortex-M4
                            0xC27 = Cortex-M7
                            0xD20 = Cortex-M23
                            0xD21 = Cortex-M33
    [3:0]Revision           Minor revision: 0x1 = Patch 1

     Compiler IDs
        ARM     = 1
        GCC_ARM = 2
        IAR     = 3

     Compiler versions:
       ARM: PVVbbbb (P = Major; VV = Minor; bbbb = build number)
       GCC: VVRRPP  (VV = Version; RR = Revision; PP = Patch)
       IAR: VRRRPPP (V = Version; RRR = Revision; PPP = Patch)
    */

    Log.infoln(sysInfoFmt, statsSys.cpu_id, statsSys.os_version, statsSys.compiler_id, statsSys.compiler_version, sysInfo->getBoardId().c_str(),
               BOARD_NAME, BOARD_VENDORID, BOARD_PRODUCTID, sysInfo->getMacAddress().c_str(), DEVICE_NAME, sysInfo->get_flash_capacity());
#endif
}

/**
 * This shows zero always on RP2040, the Nano RP2040 board is missing low power timers that seem to be required for these stats
 * See https://github.com/ARMmbed/mbed-os-example-cpu-stats/blob/mbed-os-6.7.0/main.cpp
 */
void logCPUStats() {
#ifndef DISABLE_LOGGING
    mbed_stats_cpu_t stats;
    mbed_stats_cpu_get(&stats);
    unsigned long curStatTime = millis();
    unsigned long sampleTime = curStatTime - prevStatTime;

    // Calculate the percentage of CPU usage
    uint64_t diff_usec = (stats.idle_time - prevIdleTime);
    uint8_t idle = (diff_usec * 100) / (sampleTime*1000);
    uint8_t usage = 100 - ((diff_usec * 100) / (sampleTime*1000));
    prevIdleTime = stats.idle_time;

    Log.info(F("CPU STATS\n"));
    Log.info(cpuStatsFmt, stats.uptime, stats.idle_time, stats.sleep_time, stats.deep_sleep_time, idle, usage);

    prevStatTime = curStatTime;

    Log.endContinuation();
#endif
}

// SysInfo
SysInfo::SysInfo() : boardName(DEVICE_NAME), buildVersion(BUILD_VERSION), buildTime(BUILD_TIME), scmBranch(GIT_BRANCH) {
    boardId.reserve(BUF_ID_SIZE);       // flash unique ID in hex, \0 terminator
    secElemId.reserve(BUF_ID_SIZE);     // 18 from ECCX08Class::serialNumber() implementation
    macAddress.reserve(BUF_ID_SIZE);    // 6 (WL_MAC_ADDR_LENGTH) groups of 2 hex digits and ':' separator, includes \0 terminator
    ipAddress.reserve(BUF_ID_SIZE);     // 4 groups of 3 digits, 3 '.' separators, \0 terminator
    wifiFwVersion.reserve(BUF_ID_SIZE); // typical semantic version e.g. v1.5.0, 3 groups of 2 digits, '.' separator, \0 terminator
    ssid.reserve(BUF_ID_SIZE);          // initial space, most networks are short names
    status = 0;
    cleanBoot = true;
}

/**
 * Capture Flash UID as board unique identifier in HEX string format
 */
void SysInfo::fillBoardId() {
    // Flash UID - could also use getUniqueSerialNumber (output in hex) with a buffer twice the size of FLASH_UNIQUE_ID_SIZE_BYTES
    uint8_t fuid[FLASH_UNIQUE_ID_SIZE_BYTES];
    flash_get_unique_id(fuid);
    int i = 0;
    char buff[BUF_ID_SIZE];
    for (auto &b : fuid)
        i += sprintf(buff+i, "%02X", b);    //boardId string must already have reserved space for Flash UID, see constructor
    buff[i] = 0;    //null terminator
    boardId = buff;
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

void SysInfo::setWiFiInfo(WiFiClass &wifi) {
    ssid = wifi.SSID();
    wifiFwVersion = WiFiClass::firmwareVersion();
    ipAddress = wifi.localIP().toString();
    gatewayIpAddress = wifi.gatewayIP().toString();

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
        sysInfo->ipAddress = doc[csIpAddress].as<String>();
        sysInfo->gatewayIpAddress = doc[csGatewayAddress].as<String>();
        sysInfo->heapSize = doc[csHeapSize];
        sysInfo->freeHeap = doc[csFreeHeap];
        sysInfo->stackSize = doc[csStackSize];
        sysInfo->freeStack = doc[csFreeStack];
        //do not override current status (in progress of populating) with last run status
        uint8_t lastStatus = doc[csStatus];
#ifndef DISABLE_LOGGING
        Log.infoln(F("System Information restored from %s [%d bytes]: boardName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%X (last %X), IP=%s, Gateway=%s"),
                   sysFileName, sysSize, brdName.c_str(), bldVersion.c_str(), bldTime.c_str(), gitBranch.c_str(), sysInfo->boardId.c_str(), sysInfo->secElemId.c_str(), sysInfo->macAddress.c_str(), sysInfo->status, lastStatus,
                   sysInfo->ipAddress.c_str(), sysInfo->gatewayIpAddress.c_str());
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
    doc[csIpAddress] = sysInfo->ipAddress;
    doc[csGatewayAddress] = sysInfo->gatewayIpAddress;
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
