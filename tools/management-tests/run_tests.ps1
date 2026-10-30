# Host unit tests for the portable management modules.
#
# The tests compile only the parts of entrance/src/management that do not
# depend on ESP-IDF (time_utils, sha256, access_schedule, admin_auth,
# access_events, system_status, serial_menu) and run them on the PC.
#
# Requirements: a C11 compiler.  By default the script uses the "zig cc"
# compiler installed through pip (python -m pip install ziglang).  Set
# COCIS_TEST_CC to use another compiler, for example:
#   $env:COCIS_TEST_CC = "gcc"
#
# Usage (from the repository root):
#   powershell -ExecutionPolicy Bypass -File tools/management-tests/run_tests.ps1

$ErrorActionPreference = "Stop"

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = Resolve-Path (Join-Path $scriptDirectory "..\..")
$sourceDirectory = Join-Path $repositoryRoot "entrance\src"
$buildDirectory = Join-Path $scriptDirectory "build"

$coreSources = @(
    "management\time_utils.c",
    "management\sha256.c",
    "management\access_schedule.c",
    "management\admin_auth.c",
    "management\access_events.c",
    "management\system_status.c",
    "management\serial_menu.c"
)

$testSources = @(
    "test_main.c",
    "test_time.c",
    "test_schedule.c",
    "test_auth.c",
    "test_events.c",
    "test_status.c",
    "test_menu.c"
)

# Works out how to invoke the compiler.  "zig cc" is used through the Python
# module so no PATH changes are required.
$compiler = $env:COCIS_TEST_CC
$compilerArguments = @()

if ([string]::IsNullOrWhiteSpace($compiler)) {
    $python = Get-Command python -ErrorAction SilentlyContinue

    if ($python -ne $null) {
        & python -c "import ziglang" 2>$null

        if ($LASTEXITCODE -eq 0) {
            $compiler = "python"
            $compilerArguments = @("-m", "ziglang", "cc")
        }
    }

    if ([string]::IsNullOrWhiteSpace($compiler)) {
        $compiler = "gcc"
    }
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null

$binary = Join-Path $buildDirectory "management-tests.exe"
$arguments = @()
$arguments += $compilerArguments
$arguments += @("-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-g")
$arguments += @("-I", $sourceDirectory, "-I", $scriptDirectory)

foreach ($source in $coreSources) {
    $arguments += (Join-Path $sourceDirectory $source)
}

foreach ($source in $testSources) {
    $arguments += (Join-Path $scriptDirectory $source)
}

$arguments += @("-o", $binary)

Write-Host "Compiling with: $compiler $($compilerArguments -join ' ')"
Write-Host ""

& $compiler @arguments

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Build failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
& $binary
exit $LASTEXITCODE
