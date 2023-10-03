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
if ($port -eq 'auto') {
    pio device monitor | tee -FilePath $PSScriptRoot/logs/$fname
} else {
    pio device monitor --port $port | tee -FilePath $PSScriptRoot/logs/$fname
}