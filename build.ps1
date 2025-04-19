[CmdletBinding()]
param ([switch]$dbg, [switch]$clean,
    [Parameter(Mandatory=$false)]
    [ValidateSet("Dev", "FX01", "FX02")]
    [string]$board = "Dev",
    [switch]$log
)

# see platformio.ini for the environment names
$relEnv = "rp2040-rel"
$dbgEnv = "rp2040-dbg"

function prepEnvironment() {
    # see config.h in the include folder for board ID values
    # 1 = Dev, 2 = FX01, 3 = FX02
    $boardId = switch ($board) {
        "Dev" { 1 }
        "FX01" { 2 }
        "FX02" { 3 }
    }
    $env:PLATFORMIO_BUILD_FLAGS = "-DBOARD_ID=$boardId"
    if ($log) {
        $env:PLATFORMIO_BUILD_FLAGS += " -DLOGGING_ENABLED=1"
    }
}

# Function to build the application
function Build-Application($envName) {
    Write-Host "PlatformIO building for environment: $envName"
    prepEnvironment
    # Add your build commands here
    # Example:
    # & "path\to\build\tool" --env $envName
    pio run -e $envName
}

if ($clean) {
    # Clean the build
    pio run -t clean -e ($dbg ? $dbgEnv : $relEnv)
}
# Call the build function with the appropriate environment name based on debug flag
Build-Application ($dbg ? $dbgEnv : $relEnv)
