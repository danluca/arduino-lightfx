// Copyright (c) by Dan Luca. All rights reserved.
//
#include <Arduino.h>

#include "config.h"
#include "log.h"
#include "sysinfo_internal.h"
#include "version.h"

namespace {

constexpr size_t kBufIdSize = 20;

} // namespace

SysInfo::SysInfo() : boardName(BOARD_NAME), deviceName(DEVICE_NAME), buildVersion(BUILD_VERSION), buildTime(BUILD_TIME), scmBranch(GIT_BRANCH),
                     ipAddress({IP_ADDR}), ipGateway({IP_GW}) {
    boardId.reserve(kBufIdSize);
    secElemId.reserve(kBufIdSize);
    macAddress.reserve(kBufIdSize);
    strIpAddress.reserve(kBufIdSize);
    strGatewayIpAddress.reserve(kBufIdSize);
    wifiFwVersion.reserve(kBufIdSize);
    ssid.reserve(kBufIdSize);
    cpuModel.reserve(kBufIdSize);
    cpuFrequency = 0;
    cpuVersion = 0;
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
#if defined(PICO_RP2350)
    cpuModel = "RP2350";
    cpuVersion = rp2350_chip_version();
    psramSize = rp2040.getPSRAMSize();
#elif defined(ARDUINO_ARCH_RP2040)
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
    const SysStatus oldStatus = status;
    status |= bitMask;
    if (status != oldStatus)
        SysInfoPersistence::instance().markDirty();
    return status;
}

SysStatus SysInfo::resetSysStatus(const SysStatus bitMask) {
    CoreMutex coreMutex(&mutex);
    const SysStatus oldStatus = status;
    status &= (~bitMask);
    if (status != oldStatus)
        SysInfoPersistence::instance().markDirty();
    return status;
}

bool SysInfo::isSysStatus(const SysStatus bitMask) const {
    CoreMutex coreMutex(&mutex);
    return (status & bitMask) == bitMask;
}

SysStatus SysInfo::getSysStatus() const {
    CoreMutex coreMutex(&mutex);
    return status;
}

void SysInfo::addWatchdogReboot(const WatchdogRebootInfo& info) {
    CoreMutex coreMutex(&mutex);
    wdReboots.push(info);
    SysInfoPersistence::instance().markDirty();
}

size_t SysInfo::watchdogRebootsCount() const {
    CoreMutex coreMutex(&mutex);
    return wdReboots.size();
}

bool SysInfo::hasWatchdogReboots() const {
    CoreMutex coreMutex(&mutex);
    return !wdReboots.empty();
}

time_t SysInfo::lastWatchdogReboot() const {
    CoreMutex coreMutex(&mutex);
    return wdReboots.empty() ? 0 : wdReboots.back().time;
}

std::vector<WatchdogRebootInfo> SysInfo::watchdogRebootsSnapshot() const {
    CoreMutex coreMutex(&mutex);
    std::vector<WatchdogRebootInfo> snapshot;
    snapshot.reserve(wdReboots.size());
    for (const auto &r : wdReboots)
        snapshot.push_back(r);
    return snapshot;
}

void SysInfo::transformWatchdogReboots(const std::function<time_t(time_t)> &transform) {
    CoreMutex coreMutex(&mutex);
    for (auto &r : wdReboots)
        r.time = transform(r.time);
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
    const String oldSsid = ssid;
    const String oldIp = strIpAddress;
    const String oldGw = strGatewayIpAddress;

    ssid = wifi.SSID();
    wifiFwVersion = ::WiFiClass::firmwareVersion();
    strIpAddress = wifi.localIP().toString();
    strGatewayIpAddress = wifi.gatewayIP().toString();

    if (ssid != oldSsid || strIpAddress != oldIp || strGatewayIpAddress != oldGw)
        SysInfoPersistence::instance().markDirty();

    const IPAddress dns1 = wifi.dnsIP(0);
    const IPAddress dns2 = wifi.dnsIP(1);
    log_info(F("WiFi DNS servers: %s, %s"), dns1.toString().c_str(), dns2.toString().c_str());
    (void)dns1;
    (void)dns2;

    uint8_t mac[WL_MAC_ADDR_LENGTH];
    wifi.macAddress(mac);
    char buf[kBufIdSize];
    int x = 0;
    for (const auto &b : mac)
        x += snprintf(buf + x, 4, "%02X:", b);
    buf[x - 1] = 0;
    macAddress = buf;
    SysInfoPersistence::instance().markDirty();
}

void SysInfo::setSecureElementId(const String &secId) {
    if (secElemId != secId) {
        secElemId = secId;
        SysInfoPersistence::instance().markDirty();
    }
}
