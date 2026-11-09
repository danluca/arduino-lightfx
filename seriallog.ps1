## Copyright (c) by Dan Luca. All rights reserved.
##
## Serial log capture script for Arduino or RPi Pico boards connected via USB
##
[CmdletBinding()]
param ([string]$port='auto')

#######################################
## Global
#######################################
$clrReset = "`e[0m"
$clrMsg = "`e[38;5;112m"

#######################################
## Functions
#######################################

function Get-LogName {
    $dt = Get-Date -Format 'yyyyMMdd-HHmm'
    return "log-$dt.log"
}

function Get-SerialDevices {
    $deviceOutput = & pio device list --json-output
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to list serial devices with PlatformIO (exit code $LASTEXITCODE)."
    }

    $devices = ($deviceOutput -join [Environment]::NewLine) | ConvertFrom-Json
    return @($devices)
}

#######################################
## Main
#######################################
$logDirectory = Join-Path $PSScriptRoot 'logs'
$logPath = Join-Path $logDirectory (Get-LogName)
if (!(Test-Path $logDirectory -PathType Container)) {
    New-Item $logDirectory -ItemType Directory | Out-Null
}

if ($port -eq 'auto') {
    $devices = Get-SerialDevices
    $identifiedDevices = @($devices | Where-Object {
        $_.port -and
        (($_.description -and $_.description -ne 'n/a') -or
         ($_.hwid -and $_.hwid -ne 'n/a'))
    })
    if ($identifiedDevices.Count -eq 0) {
        throw 'No identified serial devices found. Relaunch with -port <port> to specify a port.'
    }
    if ($identifiedDevices.Count -gt 1) {
        Write-Information "${clrMsg}Multiple serial devices found. Relaunch with -port <port> and choose one:$clrReset" -InformationAction Continue
        foreach ($device in $identifiedDevices) {
            Write-Information ("${clrMsg}  {0} - {1} [{2}]$clrReset" -f $device.port, $device.description, $device.hwid) -InformationAction Continue
        }
        exit 1
    }

    $port = $identifiedDevices[0].port
    Write-Information "${clrMsg}Selected $port - $($identifiedDevices[0].description) [$($identifiedDevices[0].hwid)]$clrReset" -InformationAction Continue
} else {
    $devices = Get-SerialDevices
    $mappedDevice = $devices | Where-Object { $_.port -eq $port } | Select-Object -First 1
    if ($null -eq $mappedDevice) {
        Write-Warning "Port $port was not found in PlatformIO's device list; attempting it anyway."
    } else {
        Write-Information ("${clrMsg}Port {0} is mapped to {1} [{2}]$clrReset" -f $port, $mappedDevice.description, $mappedDevice.hwid) -InformationAction Continue
    }
}

& stty -F $port 115200 raw -echo
if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure serial port $port at 115200 baud."
}

Write-Information "${clrMsg}Capturing $port to $logPath (press Ctrl+C to stop)`n`n$clrReset" -InformationAction Continue
& cat $port | Tee-Object -FilePath $logPath