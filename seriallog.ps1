## Copyright (c) 2025 by Dan Luca. All rights reserved.
##
## Serial log capture script for RP2350-based boards
[CmdletBinding()]
param ([string]$port='auto')

#######################################
## Functions
#######################################

function logName {
    $dt = Get-Date -Format 'yyyyMMdd-HHmm'
    return "log-$dt.log"
}

#######################################
## Main
#######################################
$fname = logName
if (!(Test-Path $PSScriptRoot/logs -PathType Container)) {
    New-Item $PSScriptRoot/logs -ItemType Directory
}
if ($port -eq 'auto') {
    pio device monitor | Tee-Object $PSScriptRoot/logs/$fname
} else {
    pio device monitor --port $port | Tee-Object $PSScriptRoot/logs/$fname
}