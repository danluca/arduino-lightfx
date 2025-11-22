## Copyright (c) 2025 by Dan Luca. All rights reserved.
##
## Build script for PlatformIO project
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev","FX01", "FX02")]
    [string]$board = "Dev"
)

#######################################
## Global
#######################################
. $PSScriptRoot/boards.ps1
$brdUri = $boardMap[$board]

write-host "Board $board at $brdUri - File List`n" -ForegroundColor Yellow

iwr $brdUri/files.json | select -ExpandProperty Content
