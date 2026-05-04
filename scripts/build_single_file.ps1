#Requires -Version 7
<#
.SYNOPSIS
  Build a single-file, self-contained emotiv_lsl.exe (Windows).

.DESCRIPTION
  Configures + builds the project with liblsl, hidapi, and xdfwriter
  statically linked, plus the MSVC C runtime statically linked
  (CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded), so the resulting binary has
  no .dll / VC++ Redistributable dependencies at runtime.

  Uses a dedicated build-static/ directory so it does not clobber the
  default dynamic build/ used by run.ps1. The final binary is copied to
  dist/emotiv_lsl-<version>-windows-<arch>.exe.

  After build, runs `dumpbin /dependents` (when available in the current
  shell) and warns if any of lsl.dll, hidapi.dll, or xdfwriter.dll appear
  in the dependency list.

.EXAMPLE
  pwsh ./scripts/build_single_file.ps1
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root      = Split-Path -Parent $PSScriptRoot
$buildDir  = Join-Path $root "build-static"
$distDir   = Join-Path $root "dist"
$cacheFile = Join-Path $buildDir "CMakeCache.txt"
$wantedGen = "Visual Studio 17 2022"

$env:CMAKE_GENERATOR = $wantedGen

if (Test-Path $cacheFile) {
    $cachedGen = (Select-String -Path $cacheFile -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)').Matches.Groups[1].Value
    if ($cachedGen -and $cachedGen -ne $wantedGen) {
        Write-Host "==> Stale cache detected (was: $cachedGen, want: $wantedGen). Clearing $buildDir." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $buildDir
    }
}

Write-Host "==> cmake configure (static link) -> $buildDir" -ForegroundColor Cyan
cmake -S $root -B $buildDir -G $wantedGen `
    -DCMAKE_BUILD_TYPE=Release `
    -DLSLTEMPLATE_BUILD_GUI=OFF `
    -DLSLTEMPLATE_BUILD_CLI=OFF `
    -DEMOTIVLSL_BUILD_EMOTIV=ON `
    -DLSL_BUILD_STATIC=ON `
    -DBUILD_SHARED_LIBS=OFF `
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
    -DCMAKE_INSTALL_PREFIX="$buildDir/install"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

Write-Host "==> cmake --build $buildDir --config Release" -ForegroundColor Cyan
cmake --build $buildDir --config Release -j
if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }

$exe = Join-Path $buildDir "src/emotiv/Release/emotiv_lsl.exe"
if (-not (Test-Path $exe)) {
    throw "Built binary not found at: $exe"
}

# Pull project version from CMakeLists.txt: project(LSLTemplate VERSION x.y.z ...)
$cmakeListsPath = Join-Path $root "CMakeLists.txt"
$versionMatch   = Select-String -Path $cmakeListsPath -Pattern 'VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
$version        = if ($versionMatch) { $versionMatch.Matches.Groups[1].Value } else { "0.0.0" }

# Architecture: PROCESSOR_ARCHITECTURE is AMD64 / ARM64 / x86 on Windows.
$arch = switch -Wildcard ($env:PROCESSOR_ARCHITECTURE) {
    "AMD64" { "x86_64" }
    "ARM64" { "arm64" }
    "x86"   { "i386" }
    default { ($env:PROCESSOR_ARCHITECTURE).ToLower() }
}

if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir | Out-Null }
$outName = "emotiv_lsl-$version-windows-$arch.exe"
$outPath = Join-Path $distDir $outName
Copy-Item -Path $exe -Destination $outPath -Force

# Verify no dynamic deps on bundled libs. dumpbin is part of the MSVC
# toolchain; only available inside a VS Developer prompt. If absent we
# silently skip the check.
$dumpbin = Get-Command "dumpbin.exe" -ErrorAction SilentlyContinue
if ($dumpbin) {
    Write-Host "==> Verifying no dynamic dependencies on lsl.dll / hidapi.dll / xdfwriter.dll" -ForegroundColor Cyan
    $deps = & $dumpbin.Source /dependents $outPath 2>$null
    $bad  = $deps | Select-String -Pattern '(?i)\b(lsl|hidapi|xdfwriter)\.dll\b'
    if ($bad) {
        Write-Host "FAIL: binary still depends on:" -ForegroundColor Red
        $bad | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        throw "Static link verification failed."
    }
    Write-Host "    OK (no bundled-library DLL dependencies)" -ForegroundColor Green
} else {
    Write-Host "==> dumpbin not on PATH; skipping dependency check (run from a VS Developer prompt for verification)" -ForegroundColor Yellow
}

$sizeMB = [Math]::Round((Get-Item $outPath).Length / 1MB, 2)
Write-Host ""
Write-Host "==> Single-file build complete" -ForegroundColor Green
Write-Host "    $outPath ($sizeMB MB)" -ForegroundColor Green
