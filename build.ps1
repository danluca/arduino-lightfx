[CmdletBinding()]
param ([switch]$dbg, [switch]$clean)

# see platformio.ini for the environment names
$relEnv = "rp2040-rel"
$dbgEnv = "rp2040-dbg"

# Function to build the application
function Build-Application($envName) {
    Write-Host "PlatformIO building for environment: $envName"

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
