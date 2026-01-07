## Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
##
## Build script for PlatformIO project
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "FX01", "FX02")]
    [string]$board = "Dev"
)

#######################################
## Global
#######################################
. $PSScriptRoot/util.ps1
$brdUri = (Get-BoardByName $board).IpAddress.TrimEnd('/')

write-host "Board $board at $brdUri - Status`n" -ForegroundColor Yellow

iwr $brdUri/status.json | select -ExpandProperty Content
