## Copyright (c) 2025 by Dan Luca. All rights reserved.
##
## Build script for PlatformIO project
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "Tree")]
    [string]$board = "Dev"
)

#######################################
## Global
#######################################
. $PSScriptRoot/boards.ps1
$brdUri = $boardMap[$board].TrimEnd('/')

write-host "Board $board at $brdUri - Status`n" -ForegroundColor Yellow

iwr $brdUri/status.json | select -ExpandProperty Content
