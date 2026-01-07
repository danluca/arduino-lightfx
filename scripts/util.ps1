#######################################
## Global
#######################################
# see platformio.ini for the environment names
$relEnv = "rp2040-rel"
$dbgEnv = "rp2040-dbg"

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

# Function to prepare the build environment with appropriate flags
function prepEnvironment([string]$board, [bool]$log, [bool]$ignoreBroadcast, [bool]$dbg) {

    ensureHeap4Strategy

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

    Write-Host "PlatformIO building for board '$board' with environment '$envName'" -ForegroundColor Cyan
    
    prepEnvironment $board $log $ignoreBroadcast $dbg

    # Add your build commands here
    # Example:
    # & "path\to\build\tool" --env $envName
    pio run -e $envName
}    

# Function to (build and) upload application firmware via USB connection
function Update-FirmwareSerial([string]$board, [bool]$log, [bool]$ignoreBroadcast, [bool]$dbg, [string]$port='auto') {
    prepEnvironment $board $log $ignoreBroadcast $dbg
    $brdEnv = Get-BoardEnvName $dbg
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

