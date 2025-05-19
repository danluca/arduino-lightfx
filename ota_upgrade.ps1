[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "FX01", "FX02")]
    [string]$board = "Dev",
    [Parameter(Mandatory=$false)]
    $fwPath
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
    $f = Get-Item $filePath
    $hash = (sha256 $f.FullName) -split ' ' | Select-Object -First 1
    $headers = @{
        "X-Token" = "KlFpc1dAdFd0eDRXdkVSZg";
        "X-Check" = $hash;
    }
    $resp = Invoke-WebRequest -Uri $brdUri -Method Post -InFile $f.FullName -Headers $headers -ContentType "application/octet-stream" -SkipHttpErrorCheck
    $ok = $resp.StatusCode -eq 200
    if ($ok) {
        Write-Information "${clrMsg}Board $board updated successfully.${clrReset}" -InformationAction Continue
    } else {
        Write-Error "Board $board FAILED to upload file: $filePath. Status code: $($resp.StatusCode). Response:`n$($resp.Content)"
    }
    return $ok
}

trap {
    Pop-Location
    $errorMsg = $_.Exception.Message
    Write-Error "Error: $errorMsg"
    break
}

#######################################
## Main
#######################################
Push-Location $PSScriptRoot

if ($fwPath -and -not (Test-Path $fwPath -PathType Leaf)) {
    throw "File not found: $fwPath"
}

# if we're going to build the firmware, we'll do it in release mode for the OTA upgrade purposes.
# if the user wants to build in DEBUG mode, they need to plugin USB and use the update.ps1 script directly.

if (-not $fwPath) {
    Write-Information "${clrMsg}Building for board $board in release mode...${clrReset}" -InformationAction Continue
    ./build.ps1 -board $board | Tee-Object -FilePath logs/build-$board.log
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed. Please check the build.log for details."
    }
    $fwPath = Get-ChildItem -Path ".pio/build/rp2040-rel/firmware.bin" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $fwPath) {
        throw "No firmware file found after build. Please check the build process."
    } else {
        Write-Information "${clrMsg}Upgrading Board: $board${clrReset}" -InformationAction Continue
        Write-Information "${clrMsg}  file[$($fwPath.Length | Format-Byte)]: $fwPath${clrReset}" -InformationAction Continue
    }
}
if (-not $fwPath -match '\.pio[\\/]build[\\/]') {
    Write-Warning "Board $board firmware sourced from non-default build location: $(get-item $fwPath)"
}
$success = $false
$ms = Measure-Command { $success = uploadFile $fwPath }
Write-Information "${clrMsg}Board $board upload $($success ? 'completed':'failed') in $("{0:N3}" -f $ms.TotalSeconds) seconds.${clrReset}" -InformationAction Continue

Pop-Location