#######################################
## Global
#######################################
# see platformio.ini for the environment names
$relEnv = "rp2350-rel"
$dbgEnv = "rp2350-dbg"

# platformio ArduinoPico package paths
$coreFreeRTOSPath = Join-Path -Path $HOME/.platformio/packages -ChildPath "framework-arduinopico" "cores/rp2040/freertos"
$kernelFreeRTOSPath = Join-Path -Path $HOME/.platformio/packages -ChildPath "framework-arduinopico" "FreeRTOS-Kernel" "portable/MemMang"

#######################################
## Functions
#######################################

# Function to ensure heap4 is used for memory management in FreeRTOS. Heap scheme 4 is the only one supporting coalescing free blocks, reducing fragmentation
# Essentially ensures the ~\.platformio\packages\framework-arduinopico\cores\rp2040\freertos\heap_4.c file exists
# otherwise, copies it from the FreeRTOS kernel ~\.platformio\packages\framework-arduinopico\FreeRTOS-Kernel\portable\MemMang\heap_4.c
function ensureHeap4Strategy() {
    $heapFiles = Get-ChildItem -Path $coreFreeRTOSPath -Filter "heap_*.c"
    $heap4File = $heapFiles | Where-Object { $_.Name -eq "heap_4.c" }
    $srcHeap4File = Get-Item (Join-Path -Path $kernelFreeRTOSPath -ChildPath "heap_4.c")
    if ($null -ne $heap4File) {
        if ($heap4File.LastWriteTime -eq $srcHeap4File.LastWriteTime) {
            Write-Host "Heap4 strategy already in place and up-to-date for FreeRTOS" -ForegroundColor Yellow
            return
        } else {
            Write-Host "Updating existing Heap4 strategy for FreeRTOS" -ForegroundColor Green
            Copy-Item -Path $srcHeap4File.FullName -Destination $heap4File.FullName -Force
        }
    } else {
        # heapFiles does not contain heap_4.c, so we need to backup all the files and set heap 4 up
        Write-Host "Setting up Heap4 strategy for FreeRTOS" -ForegroundColor Green
        $heapFiles | ForEach-Object {
            Move-Item -Path $_.FullName -Destination ($_.FullName -replace "\.c$", ".bak")
        }
        Copy-Item -Path $srcHeap4File.FullName -Destination $coreFreeRTOSPath
    }
}

# Function to update the stack size for the CORE 0 and CORE 1 tasks in the freertos-main.cpp file. This is necessary to increase the stack size for the CORE tasks, 
# which can be necessary for more complex applications. The default stack size is 1024 bytes, but we can set it to 2048 bytes or more as needed.
function prepCoreStackSize([int]$size) {
    $freertosMainFile = Join-Path -Path $coreFreeRTOSPath -ChildPath "freertos-main.cpp"

    if (Test-Path $freertosMainFile) {
        $content = Get-Content $freertosMainFile
        $updated = $false
        
        for ($i = 0; $i -lt $content.Length; $i++) {
            if ($content[$i] -match 'xTaskCreate\([^,]+,\s*("[^"]*"),\s*(\d+)') {
                $task = $Matches[1]
                $currentSize = [int]$Matches[2]
                if (($task -match '^"CORE\d') -and ($currentSize -ne $size)) {
                    $content[$i] = $content[$i] -replace '(xTaskCreate\([^,]+,\s*"[^"]*",\s*)\d+', "`${1}$size"
                    $updated = $true
                }
            }
        }
        
        if ($updated) {
            Set-Content -Path $freertosMainFile -Value $content
            Write-Host "Updated CORE 0, 1 stack sizes to $size bytes" -ForegroundColor Green
        } else {
            Write-Host "CORE stack sizes are already set to $size bytes" -ForegroundColor Yellow
        }
    } else {
        Write-Host "Could not find freertos-main.cpp to update CORE stack sizes - using defaults" -ForegroundColor Red
    }
}

# Function to prepare the build environment with appropriate flags
function prepEnvironment([string]$board, [bool]$log, [bool]$ignoreBroadcast, [bool]$dbg) {

    ensureHeap4Strategy
    prepCoreStackSize(2048) # set core stack size to 2KB to increase stack sizes for CORE tasks, default is 1024 bytes

    $boardId = (Get-BoardByName $board).Id
    $env:PLATFORMIO_BUILD_FLAGS = "-DBOARD_ID=$boardId"
    if ($log) {
        $env:PLATFORMIO_BUILD_FLAGS += " -DLOGGING_ENABLED=1"
    }
    if ($ignoreBroadcast) {
        $env:PLATFORMIO_BUILD_FLAGS += " -DIGNORE_WEB_EFFECT_CHANGES=1"
    }
    if (!$log -and !$dbg) {
        $env:PLATFORMIO_BUILD_FLAGS += " -DPIO_FRAMEWORK_ARDUINO_NO_USB"
    }
}

function Get-FrameworkArduinoPicoShortCommit() {
    $frameworkPath = Join-Path -Path $HOME/.platformio/packages -ChildPath "framework-arduinopico"
    if (-not (Test-Path $frameworkPath)) {
        return $null
    }

    $commit = git -C $frameworkPath rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $commit) {
        return $commit.Trim()
    }

    return $null
}

function Get-BoardEnvName([bool]$dbg) {
    return $dbg ? $dbgEnv : $relEnv
}

function Clean([bool]$dbg) {
    # Clean the build
    pio run -t clean -e (Get-BoardEnvName $dbg)
}

# Function to build the application
function Build-Application([string]$board, [bool]$log, [bool]$ignoreBroadcast, [bool]$dbg) {
    $envName = Get-BoardEnvName $dbg
    
    Write-Host "`nPlatformIO building for board '$board' with environment '$envName'" -ForegroundColor Cyan

    prepEnvironment $board $log $ignoreBroadcast $dbg
    $frameworkCommit = Get-FrameworkArduinoPicoShortCommit

    Write-Host "Building application firmware..." -ForegroundColor Green
    Write-Host "  > framework-arduinopico at commit: $frameworkCommit `n" -ForegroundColor Green
    # Add your build commands here
    # Example:
    # & "path\to\build\tool" --env $envName
    pio run -e $envName
}

# Function to (build and) upload application firmware via USB connection
function Update-FirmwareSerial([string]$board, [bool]$log, [bool]$ignoreBroadcast, [bool]$dbg, [string]$port='auto') {
    $brdEnv = Get-BoardEnvName $dbg
    Write-Host "`nPlatformIO building & updating for board '$board' with environment '$brdEnv' on port $port" -ForegroundColor Cyan
    prepEnvironment $board $log $ignoreBroadcast $dbg
    $frameworkCommit = Get-FrameworkArduinoPicoShortCommit
    Write-Host "Building & updating application firmware..." -ForegroundColor Green
    Write-Host "  > framework-arduinopico at commit: $frameworkCommit `n" -ForegroundColor Green
    if ($port -eq 'auto') {
        pio run -t upload -e $brdEnv
    } else {
        pio run -t upload -e $brdEnv --port $port
    }
}

#######################################
## Main
#######################################
. $PSScriptRoot/boards.ps1

