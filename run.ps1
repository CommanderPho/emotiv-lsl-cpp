#Requires -Version 7
<#
.SYNOPSIS
  Configure, build, install, and run emotiv_lsl in one step.

.DESCRIPTION
  Runs cmake --workflow --preset emotiv (configure + build), then installs the
  self-contained bundle to build/install/ (copies lsl.dll/liblsl.so next to
  the binary), then launches emotiv_lsl. All C++ dependencies (liblsl, hidapi,
  tiny-AES-c) are fetched automatically by CMake on first run.

.EXAMPLE
  pwsh ./run.ps1
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot

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
