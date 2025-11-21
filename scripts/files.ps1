## Copyright (c) 2025 by Dan Luca. All rights reserved.
##
## Build script for PlatformIO project
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev","Tree")]
    [string]$board = "Dev"
)

#######################################
## Global
#######################################
$brdUri = "http://192.168.0.10"  # Default URI for Dev board
switch ($board) {
    "FX01" { $brdUri = "http://192.168.0.11"; break; }
    "FX02" { $brdUri = "http://192.168.0.12"; break; }
}

write-host "Board $board at $brdUri - File List`n" -ForegroundColor Yellow

iwr $brdUri/files.json | select -ExpandProperty Content
