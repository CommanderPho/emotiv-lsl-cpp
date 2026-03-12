#Requires -Version 7
<#
.SYNOPSIS
  Configure, build, install, and run emotiv_lsl in one step.

.DESCRIPTION
  Runs cmake --workflow --preset emotiv (configure + build), then installs the
  self-contained bundle to build/install/ (copies lsl.dll/liblsl.so next to
  the binary), then launches emotiv_lsl. All C++ dependencies (liblsl, hidapi,
  tiny-AES-c) are fetched automatically by CMake on first run.

  If a stale CMakeCache.txt exists from a different generator, it is removed
  automatically before configuring so the build stays consistent cross-platform.

.EXAMPLE
  pwsh ./run.ps1
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root         = $PSScriptRoot
$cacheFile    = Join-Path $root "build" "CMakeCache.txt"
$wantedGen    = if ($IsWindows) { "Visual Studio 17 2022" } else { "Ninja" }

# Tell CMake which generator to use (preset has none specified so this env var
# is the authoritative source on all platforms).
$env:CMAKE_GENERATOR = $wantedGen

# Remove stale cache when the stored generator doesn't match the wanted one.
# This happens after a previous build with a different generator.
# Object files in build/ are also removed so CMake starts clean.
if (Test-Path $cacheFile) {
    $cachedGen = (Select-String -Path $cacheFile -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)').Matches.Groups[1].Value
    if ($cachedGen -and $cachedGen -ne $wantedGen) {
        Write-Host "==> Stale cache detected (was: $cachedGen, want: $wantedGen). Clearing build/ for clean configure." -ForegroundColor Yellow
        Remove-Item -Recurse -Force (Join-Path $root "build")
    }
}

Write-Host "==> cmake --workflow --preset emotiv" -ForegroundColor Cyan
cmake --workflow --preset emotiv
if ($LASTEXITCODE -ne 0) { throw "CMake workflow failed (exit $LASTEXITCODE)" }

Write-Host "==> cmake --install build --config Release" -ForegroundColor Cyan
cmake --install (Join-Path $root "build") --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake install failed (exit $LASTEXITCODE)" }

$binary = if ($IsWindows) { Join-Path $root "build\install\emotiv_lsl.exe" }
          else            { Join-Path $root "build/install/emotiv_lsl" }

if (-not (Test-Path $binary)) {
    throw "Binary not found at: $binary"
}

Write-Host "==> Starting $binary" -ForegroundColor Cyan
& $binary @args
