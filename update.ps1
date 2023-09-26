[CmdletBinding()]
param ([ValidateSet('auto', 'com3', 'com4')][string]$port='auto')


#######################################
## Main
#######################################
if ($port -eq 'auto') {
    pio run -t upload
} else {
    pio run -t upload --port $port
}