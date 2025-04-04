[CmdletBinding()]
param ($fwPath)

#######################################
## Global
#######################################
$brdUri = "http://192.168.0.10/fw"

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

uploadFile $fwPath

Pop-Location