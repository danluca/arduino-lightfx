[CmdletBinding()]
param(
    [Parameter(Position=0, Mandatory=$true)]
    [string]$File,
    [Parameter(Position=1)]
    [string]$Variant=1,
    [Parameter(Position=2)]
    [int]$OffsetMinutes=0,
    [Parameter(Position=3)]
    [int]$MaxMinutes=3,
    [switch]$force
)

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

$outFile = "fsi4_seed$Variant.txt"

if ((Test-Path $outFile) -and -not $force) {
    Write-Host "Found existing $outFile — skipping generation"
    exit 0
}

Write-Host "Generating $outFile..."

if (-not (Test-PythonVenvActive)) {
    Write-Host "Python virtual environment not detected. Trying to activate a local venv if present..."
    if (ActivateVenv) { Start-Sleep -Seconds 1 }
}

if (-not (Test-PythonVenvActive)) {
    Write-Error "Python virtual environment is not active. Please activate your venv (e.g. '.\\.venv\\Scripts\\Activate.ps1' on Windows or 'source venv/bin/activate' on Unix) and retry."
    exit 2
} else {
    Write-Host "Python virtual environment ${env:VIRTUAL_ENV} is active."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$createScript = Join-Path $scriptDir 'create_audio_seed.py'

if (-not (Test-Path $createScript)) {
    Write-Error "create_audio_seed.py not found in $scriptDir"
    exit 3
}

& python $createScript $File $outFile 4 30 --stretch_p 1 99 --contrast 1.8 --transient 0.4 --max_minutes $MaxMinutes --offset_minutes $OffsetMinutes
if ($LASTEXITCODE -ne 0) { Write-Error "create_audio_seed.py failed"; exit 4 }

try {
    $size = (Get-Item -Path $outFile -ErrorAction Stop).Length
} catch {
    Write-Error "Expected output file $outFile not found"
    exit 5
}

if ($size -gt 22528) {
    Write-Host "Warning: Audio seed file size $size is larger than 22k limit, applying sifting..."
    $everyScript = Join-Path $scriptDir 'every_nth_line.py'
    if (-not (Test-Path $everyScript)) {
        Write-Error "every_nth_line.py not found in $scriptDir"
        exit 6
    }

    & python $everyScript $outFile -o "$outFile.short" -n 3
    if ($LASTEXITCODE -ne 0) { Write-Error "every_nth_line.py failed"; exit 7 }

    Move-Item -Force "$outFile.short" $outFile

    try { $size = (Get-Item -Path $outFile -ErrorAction Stop).Length } catch { $size = 0 }
    if ($size -gt 22528) {
        Write-Error "ERROR: File size $size (after sifting) is still larger than 22k limit. Please adjust audio spectral analysis params and try again."
        exit 1
    }
}

Get-ChildItem -Force -Path $outFile | Format-List Name,Length,Mode,LastWriteTime


