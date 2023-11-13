[CmdletBinding()]
param ([ValidateSet('auto', 'com3', 'com4')][string]$port='auto')

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
    pio device monitor | tee $PSScriptRoot/logs/$fname
} else {
    pio device monitor --port $port | tee $PSScriptRoot/logs/$fname
}