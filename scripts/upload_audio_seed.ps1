[CmdletBinding(DefaultParameterSetName='Upload')]
param(
    [Parameter(Mandatory=$true, ParameterSetName='Generate')]
    [string]$File,
    [Parameter(Mandatory=$true)]
    [string]$Variant,
    [Parameter(Mandatory=$true, ParameterSetName='Generate')]
    [int]$OffsetMinutes,
    [Parameter(Mandatory=$true, ParameterSetName='Generate')]
    [int]$MaxMinutes,
    [Parameter(Mandatory=$true, ParameterSetName='Upload')]
    [ValidateSet("Dev", "Tree")]
    [string]$Board='Dev',
    [switch]$force
)

#######################################
## Functions
#######################################

function Test-PythonVenvActive {
    if (-not ($env:VIRTUAL_ENV)) {
        return $false
    }
    return $true
}

function ActivateVenv {
    $candidates = @('.venv', 'venv', 'penv', 'lfxpy')
    foreach ($d in $candidates) {
        $psPath = Join-Path $PSScriptRoot $d ($IsWindows ? 'Scripts\Activate.ps1' : 'bin/Activate.ps1')
        if (Test-Path $psPath) {
            Write-Host "Attempting to activate venv at $d (posh style)"
            . $psPath
            return $true
        }
    }
    return $false
}

#######################################
## Main
#######################################

. $PSScriptRoot/boards.ps1

$outFile = "fsi4_seed$Variant.txt"

$Destination = (Get-BoardByName $Board).IpAddress

if ((Test-Path $outFile) -and -not $force) {
    Write-Host "Found existing $outFile — skipping generation"
} else {
    Write-Host "Generating $outFile..."

    if (-not (Test-PythonVenvActive)) {
        Write-Host "Python virtual environment not detected. Trying to activate a local venv if present..."
        if (ActivateVenv) { Start-Sleep -Seconds 1 }
    }

    if (-not (Test-PythonVenvActive)) {
        Write-Error "Python virtual environment is not active. Please activate your venv (e.g. '.\.venv\Scripts\Activate.ps1' on Windows or 'source venv/bin/activate' on Unix) and retry."
        exit 2
    } else {
        Write-Host "Python virtual environment ${env:VIRTUAL_ENV} is active."
    }

    $makeScript = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'make_audio_seed.ps1'
    if (Test-Path $makeScript) {
        & $makeScript -File $File -Variant $Variant -OffsetMinutes $OffsetMinutes -MaxMinutes $MaxMinutes -force
        if ($LASTEXITCODE -ne 0) {
            Write-Error "make_audio_seed.ps1 failed"
            exit 3
        }
    } else {
        Write-Error "make_audio_seed.ps1 not found"
        exit 4
    }
}

try {
    $hash = Get-FileHash -Algorithm SHA256 -Path $outFile
    $sha256 = $hash.Hash.ToLower()
} catch {
    Write-Error "Failed to compute SHA256 for $outFile"
    exit 5
}

Write-Host "Uploading $outFile (sha256=$sha256) to $Destination"

$headers = @{
    'X-Token' = "KlFpc1dAdFd0eDRXdkVSZg"
    'X-Path'  = "fx/fxi4_seed$Variant.txt"
    'X-Check' = $sha256
}

try {
    Invoke-RestMethod -Uri ($Destination.TrimEnd('/') + '/upload') -Method Post -Headers $headers -InFile $outFile -ContentType 'application/octet-stream' -ErrorAction Stop
} catch {
    Write-Error "Upload failed: $_"
    exit 6
}

try {
    $files = Invoke-RestMethod -Uri ($Destination.TrimEnd('/') + '/files.json') -Method Get -ErrorAction Stop
    $files.files
} catch {
    Write-Warning "Could not fetch files.json: $_"
}
