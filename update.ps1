[CmdletBinding()]
param ([ValidateSet('auto', 'com3', 'com4')][string]$port='auto', [switch]$dbg)


#######################################
## Main
#######################################
$brdEnv = $dbg ? "rp2040-dbg" : "rp2040-rel"
if ($port -eq 'auto') {
    pio run -t upload -e $brdEnv
} else {
    pio run -t upload -e $brdEnv --port $port
}