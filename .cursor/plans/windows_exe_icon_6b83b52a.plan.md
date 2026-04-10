---
name: Windows exe icon
overview: Embed the existing ICO into `emotiv_lsl.exe` by adding a small Windows resource script and wiring it into the `emotiv_lsl` target on WIN32 only. No changes to `run.ps1` or the root `CMakeLists.txt` are required for the icon to show in Explorer.
todos:
  - id: add-rc-file
    content: Add src/emotiv/emotiv_lsl.rc with ID 1 ICON pointing to ../../images/icons/emotiv_lsl_cpp_icon_design.ico
    status: completed
  - id: wire-cmake
    content: "In src/emotiv/CMakeLists.txt: enable_language(RC) on WIN32 before add_executable; target_sources(emotiv_lsl PRIVATE emotiv_lsl.rc) on WIN32"
    status: completed
isProject: false
---

# Windows application icon for `emotiv_lsl.exe`

## Context

- The executable is defined in [`src/emotiv/CMakeLists.txt`](src/emotiv/CMakeLists.txt) as target `emotiv_lsl` (sources: `main.cpp`, etc.). There is no `.rc` file yet.
- The icon file is at [`images/icons/emotiv_lsl_cpp_icon_design.ico`](images/icons/emotiv_lsl_cpp_icon_design.ico).
- [`run.ps1`](run.ps1) only drives CMake workflow + install + launch; the icon is applied at **link time** by the resource compiler, so **this script does not need edits**.

## Approach

Windows shows the “file type” icon from an **RT_ICON** resource in the PE file. The standard way with CMake + MSVC (your preset uses Visual Studio 17 2022) is:

1. Add a **resource script** (e.g. [`src/emotiv/emotiv_lsl.rc`](src/emotiv/emotiv_lsl.rc)) containing a single application icon with **resource ID `1`** (Explorer’s usual convention for the primary icon):

   ```rc
   1 ICON "../../images/icons/emotiv_lsl_cpp_icon_design.ico"
   ```

   Path is relative to the `.rc` file’s directory (`src/emotiv/`), so it resolves to the repo-root `images/icons/...` file without copying assets.

2. In [`src/emotiv/CMakeLists.txt`](src/emotiv/CMakeLists.txt), **before** `add_executable(emotiv_lsl ...)`:

   - `if(WIN32) enable_language(RC) endif()` — enables the RC toolchain for this directory (avoid adding `RC` to the top-level `project(... LANGUAGES ...)` in [`CMakeLists.txt`](CMakeLists.txt), which can be awkward on non-Windows generators).

3. Still in the same file, for **WIN32 only**, attach the resource to the exe:

   - `target_sources(emotiv_lsl PRIVATE emotiv_lsl.rc)` (place it inside the existing `if(WIN32)` block next to the `hidapi`/DLL copy commands, or immediately after `add_executable`, guarded by `WIN32`).

CMake will invoke `rc.exe` for MSVC and embed the icon into `emotiv_lsl.exe` in `build/Release/` and in `build/install/` after install.

## Files to touch

| File | Change |
|------|--------|
| [`src/emotiv/emotiv_lsl.rc`](src/emotiv/emotiv_lsl.rc) | **New** — one `1 ICON "..."` line as above |
| [`src/emotiv/CMakeLists.txt`](src/emotiv/CMakeLists.txt) | `enable_language(RC)` + `target_sources(... emotiv_lsl.rc)` under `WIN32` |

## Out of scope / optional follow-ups

- **Root [`CMakeLists.txt`](CMakeLists.txt)**: not required for embedding; only change if you later want CPack/install to ship the `.ico` separately (e.g. for shortcuts).
- **Taskbar icon at runtime**: that is separate (often Qt `QApplication::setWindowIcon` or Win32 APIs). This task only covers the **`.exe` file icon in Explorer**.

## Verification

After a Release build (`pwsh ./run.ps1` or equivalent), check `build/install/emotiv_lsl.exe` in Explorer or File Explorer properties — the icon should match `emotiv_lsl_cpp_icon_design.ico`. If the resource path is wrong, `rc.exe` fails at build time with a clear “cannot open file” error.
