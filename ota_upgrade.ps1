[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    $fwPath = ".pio/build/rp2040-rel/firmware.bin",
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "FX01", "FX02")]
    [string]$board = "Dev"
)

#######################################
## Global
#######################################
$brdUri = "http://192.168.0.10/fw"  # Default URI for Dev board
switch ($board) {
    "FX01" { $brdUri = "http://192.168.0.11/fw"; break; }
    "FX02" { $brdUri = "http://192.168.0.12/fw"; break; }
}

$clrReset = "`e[0m"
$clrMsg = "`e[38;5;112m"

#######################################
## Functions
#######################################
function uploadFile($filePath) {
    if (-not (Test-Path $filePath)) {
        Write-Warning "File not found: $filePath"
        return
    }
    $f = Get-Item $filePath
    $hash = (sha256 $f.FullName) -split ' ' | Select-Object -First 1
    $headers = @{
        "X-Token" = "KlFpc1dAdFd0eDRXdkVSZg";
        "X-Check" = $hash;
    }
    $resp = Invoke-WebRequest -Uri $brdUri -Method Post -InFile $f.FullName -Headers $headers -ContentType "application/octet-stream" -SkipHttpErrorCheck
    if ($resp.StatusCode -eq 200) {
        Write-Information "${clrMsg}File uploaded successfully: $filePath. Response:`n$($resp.Content) ${clrReset}" -InformationAction Continue
    } else {
        Write-Warning "Failed to upload file: $filePath. Status code: $($resp.StatusCode). Response:`n$($resp.Content)"
    }
}

#######################################
## Main
#######################################
Push-Location $PSScriptRoot

if (-not (Test-Path $fwPath) -and ($fwPath -match '\.pio[\\/]build[\\/]')) {
    Write-Information "${clrMsg}Building firmware file...${clrReset}" -InformationAction Continue
    ./build.ps1
}

$ms = Measure-Command {
    uploadFile $fwPath
}
Write-Information "${clrMsg}Upload completed in $($ms.TotalSeconds) seconds.${clrReset}" -InformationAction Continue

Pop-Location