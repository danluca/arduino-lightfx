[CmdletBinding()]
param ([ValidateSet('auto', 'com3', 'com4')][string]$port='auto', [switch]$debug)


#######################################
## Main
#######################################
$profile = $debug ? "rp2040-dbg" : "rp2040-rel"
if ($port -eq 'auto') {
    pio run -t upload -e $profile
} else {
    pio run -t upload -e $profile --port $port
}