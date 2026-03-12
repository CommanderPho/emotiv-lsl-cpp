---
name: Single-Command Build & Run
overview: Add a CMakePresets.json for clean named configure/build/install steps, and a cross-platform PowerShell Core script (run.ps1) that does configure+build+install+run in one command.
todos:
  - id: cmake-presets
    content: Create CMakePresets.json with emotiv configure/build/install/workflow presets
    status: completed
  - id: run-script
    content: Create run.ps1 PowerShell Core script that invokes cmake --workflow --preset emotiv then runs the built binary
    status: completed
isProject: false
---

# Single-Command Build & Run

Two files solve this completely. Pick the level of automation you need:

---

## Option A — Build only (CMake workflow preset, no extra tools)

`CMakePresets.json` exposes named presets. CMake 3.25+ supports **workflow presets** that chain configure → build → install into one command:

```
cmake --workflow --preset emotiv
```

This produces a self-contained install at `build/install/` (with `lsl.dll`/`liblsl.so` copied next to the binary by the existing `LSL_install_liblsl` call in [CMakeLists.txt](CMakeLists.txt)).

---

## Option B — Build AND run (single command, cross-platform)

A `run.ps1` PowerShell Core script wraps the workflow preset and then executes the binary. PowerShell Core (`pwsh`) ships with Windows 10+ and is a one-line install on macOS/Linux.

```
pwsh ./run.ps1
```

The script will:

1. Run `cmake --workflow --preset emotiv` (configure + build + install)
2. Detect the platform and run `build/install/emotiv_lsl[.exe]`

---

## Files to create

### `CMakePresets.json` (root)

- `configurePresets`:
  - `emotiv` — sets `BUILD_TYPE=Release`, `LSLTEMPLATE_BUILD_GUI=OFF`, `LSLTEMPLATE_BUILD_CLI=OFF`, `CMAKE_INSTALL_PREFIX=build/install`
- `buildPresets`:
  - `emotiv` — references the configure preset, parallel build
- `installPresets`:
  - `emotiv` — references the configure preset, installs to `build/install`
- `workflowPresets`:
  - `emotiv` — chains configure → build → install

### `run.ps1` (root)

```powershell
cmake --workflow --preset emotiv
if ($IsWindows) { & ".\build\install\emotiv_lsl.exe" }
else            { & "./build/install/emotiv_lsl" }
```

---

## Summary of commands

- **Build self-contained install:** `cmake --workflow --preset emotiv`
- **Build + start collecting:** `pwsh ./run.ps1`
- Subsequent runs (already built): `pwsh ./run.ps1` re-runs the workflow (CMake skips up-to-date steps) then launches

