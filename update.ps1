[CmdletBinding()]
param ([string]$port='auto', [switch]$dbg, [string]$otaAddrHint,
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "FX01", "FX02")]
    [string]$board = "Dev",
    [switch]$log
)

#######################################
## Global
#######################################
$brdEnv = $dbg ? "rp2040-dbg" : "rp2040-rel"
$boardFqbn = "rp2040:rp2040:arduino_nano_connect"
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

function prepEnvironment() {
    # see config.h in the include folder for board ID values
    # 1 = Dev, 2 = FX01, 3 = FX02
    $boardId = switch ($board) {
        "Dev" { 1 }
        "FX01" { 2 }
        "FX02" { 3 }
    }
    $env:PLATFORMIO_BUILD_FLAGS = "-DBOARD_ID=$boardId"
    if ($log) {
        $env:PLATFORMIO_BUILD_FLAGS += " -DLOGGING_ENABLED=1"
    }
}

function buildClean() {
    prepEnvironment
    pio run -t clean -e $brdEnv
    pio run -e $brdEnv
}

function updateFirmwareSerial() {
    prepEnvironment
    if ($port -eq 'auto') {
        pio run -t upload -e $brdEnv
    } else {
        pio run -t upload -e $brdEnv --port $port
    }
}

function updateFirmwareOTA() {
    $ipAddress = (isArduinoCliPresent) ? (getOTAEnabledIPAddress) : $null
    if ($null -eq $ipAddress) {
        Write-Warning "No OTA enabled device found, proceeding with serial upload"
        updateFirmwareSerial
        return
    }

    Write-Information "${clrMsg}Updating firmware OTA to $ipAddress board${clrReset}" -InformationAction Continue
    buildClean
    arduino-cli upload --fqbn $boardFqbn --upload-field password=$otaPassword --protocol network --port "$ipAddress" -i .pio/build/$brdEnv/firmware.bin

}

#######################################
## Main
#######################################
Push-Location $PSScriptRoot

updateFirmwareOTA

Pop-Location