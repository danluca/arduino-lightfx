## Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
##
## USB Firmware update script for RP2350-based boards; allows default Arduino OTA if available
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "Tree")]
    [string]$board = "Dev",
    [string]$port='auto', 
    [string]$otaAddrHint,
    [switch]$dbg, 
    [switch]$log,
    [switch]$ignoreBroadcast
)

#######################################
## Global
#######################################
. $PSScriptRoot/scripts/util.ps1
$boardFqbn = "rp2350:rp2350:pimoroni_plasma2350"
$otaPassword = "password"

$clrReset = "`e[0m"
$clrMsg = "`e[38;5;112m"

#######################################
## Functions
#######################################
function isArduinoCliPresent() {
    $cli = Get-Command -Name arduino-cli -ErrorAction SilentlyContinue
    return $null -ne $cli
}

function getOTAEnabledIPAddress() {
    $boards = arduino-cli board list
    $ipAddress = $boards | Where-Object {
        if ($_.FQBN -match '\bnetwork\b') {
            return $_ -split '\s+' | Select-String -Pattern '\d+\.\d+\.\d+\.\d+' -AllMatches | ForEach-Object { $_.Matches.Value }
        }
    } | Where-Object { $otaAddrHint ? ($_ -match $otaAddrHint) : $true } | Select-Object -First 1
    if ($ipAddress) {
        Write-Information "${clrMsg}Found OTA enabled device at $ipAddress${clrReset}" -InformationAction Continue
    }
    return $ipAddress
}

function updateFirmwareOTA() {
    $ipAddress = (isArduinoCliPresent) ? (getOTAEnabledIPAddress) : $null
    if ($null -eq $ipAddress) {
        Write-Warning "No OTA enabled device found, proceeding with serial upload"
        Update-FirmwareSerial $board $log $ignoreBroadcast $dbg $port
        return
    }

    Write-Information "${clrMsg}Updating firmware OTA to $ipAddress board${clrReset}" -InformationAction Continue
    Clean -dbg $dbg
    Build-Application $board $log $ignoreBroadcast $dbg
    arduino-cli upload --fqbn $boardFqbn --upload-field password=$otaPassword --protocol network --port "$ipAddress" -i .pio/build/$brdEnv/firmware.bin

}

#######################################
## Main
#######################################
Push-Location $PSScriptRoot

start-transcript -path logs/build-$board.log
updateFirmwareOTA
stop-transcript

Pop-Location