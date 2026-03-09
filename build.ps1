## Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
##
## Build script for PlatformIO project
[CmdletBinding()]
param (
    [switch]$dbg, 
    [switch]$clean,
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev","Tree")]
    [string]$board = "Dev",
    [switch]$log,
    [switch]$ignoreBroadcast
)

. $PSScriptRoot/scripts/util.ps1

if ($clean) {
    Clean -dbg $dbg
}

# Call the build function with the appropriate environment name based on debug flag
if (test-path -path $PSScriptRoot/logs/build-$board.log -type Leaf) {
    remove-item -force -path $PSScriptRoot/logs/build-$board.log
}
start-transcript -path $PSScriptRoot/logs/build-$board.log -force
Build-Application $board $log $ignoreBroadcast $dbg
stop-transcript
