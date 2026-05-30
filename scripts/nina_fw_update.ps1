#######################################
## Global
#######################################
[CmdletBinding()]
param(
    [Parameter(ParameterSetName="GetVersion")]
    [switch]$GetVersion,
    [Parameter(ParameterSetName="List")]
    [switch]$List,
    [Parameter(ParameterSetName="Flash")]
    [switch]$Flash,
    [Parameter(ParameterSetName="Flash")]
    [string]$Version,
    [Parameter(ParameterSetName="Flash")]
    [string]$File,
    [Parameter(ParameterSetName="Dir")]
    [switch]$Dir,
    [Parameter(ParameterSetName="GetVersion")]
    [Parameter(ParameterSetName="Flash")]
    [string]$Port = "/dev/ttyACM0",
    [Parameter(ParameterSetName="GetVersion")]
    [Parameter(ParameterSetName="Flash")]
    [string]$FQBN = "arduino:mbed_nano:nanorp2040connect"
)

#######################################
## Functions
#######################################

function Run-FirmwareCommand {
    param(
        [string]$Command,
        [string[]]$fArgs
    )
    $fullCommand = "./arduino-fwuploader firmware $Command " + ($fArgs -join " ")
    Write-Host "Running: $fullCommand"
    Invoke-Expression $fullCommand
}

#######################################
## Main
#######################################
if ($IsWindows) {
    Write-Warning "This script is intended to be run on Linux or macOS where arduino-fwuploader is available. Exiting."
    exit 1
}

if (-not (Test-Path "~/Code/Tools/FWUploader/arduino-fwuploader")) {
    Write-Warning "Error: arduino-fwuploader not found in directory ~/Code/Tools/FWUploader"
    Write-Warning "       Please download the script from https://github.com/arduino/arduino-fwuploader/releases/latest"
    Write-Warning "       and place it in that directory. Exiting."
    exit 1
}

pushd ~/Code/Tools/FWUploader

if ($GetVersion) {
    $verArgs = @("--fqbn", $FQBN, "-a", $Port)
    Run-FirmwareCommand "get-version" $verArgs
}

if ($List) {
    Run-FirmwareCommand "list" @("-b", $FQBN)
}

if ($Flash) {
    $flashArgs = @("--fqbn", $FQBN, "-a", $Port)
    if ($Version) {
        $flashArgs += @("--module", $("NINA@" + $Version.Trim()))
    } elseif ($File) {
        $flashArgs += @("-i", $File)
    } else {
        Write-Host "Error: For flash, provide either -Version or -File"
        exit 1
    }
    Run-FirmwareCommand "flash" $flashArgs
}

if ($Dir) {
    Write-Host "Available firmware files in current directory:"
    Get-ChildItem -Path "." -Filter "*.bin" | Select-Object Name
}

if (-not ($GetVersion -or $List -or $Flash -or $Dir)) {
    Write-Host "No action specified. Use -GetVersion, -List, -Flash, or -Dir"
}

popd