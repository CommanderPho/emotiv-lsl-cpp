---
name: CFG-driven XDF naming
overview: When `--record` is omitted, resolve an XDF output path from the bundled `LSLTemplate.cfg` by extending `AppConfig` and `ConfigManager::load()` with a new `[Recording]` section (enabled flag, output subdirectory, filename template with placeholders). `--record <path>` continues to override. Executable-relative paths and `create_directories` ensure the file can be created.
todos:
  - id: extend-appconfig-recording
    content: Add recording fields to AppConfig; section-aware load/save in Config.cpp
    status: completed
  - id: resolve-recording-path
    content: Implement resolveRecordingOutputPath + expose exe dir helper; update LSLTemplate.cfg [Recording]
    status: completed
  - id: emotiv-main-wireup
    content: "emotiv main.cpp: findConfigFile + resolve when --record absent"
    status: completed
  - id: readme-emotiv-cfg
    content: Document cfg-driven XDF naming and override behavior in README
    status: completed
isProject: false
---

# CFG-driven default XDF filenames for emotiv_lsl

## Current behavior

- [`src/emotiv/main.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/main.cpp) only sets `record_file` when `--record <path>` is present; otherwise recording is off ([`emotiv_base.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp) gates on `!record_file.empty()`).
- The repo already ships [`LSLTemplate.cfg`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/LSLTemplate.cfg) with `[Stream]` / `[Device]`, and [`src/core/src/Config.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/Config.cpp) implements `lsltemplate::ConfigManager::load` / `findConfigFile`. `emotiv_lsl` already links `LSLTemplate::core` but does not use this config for recording.

## Desired behavior

| CLI | Result |
|-----|--------|
| `--record C:\foo\bar.xdf` | Use that path (unchanged override). |
| No `--record` | If `[Recording]` in the loaded cfg enables auto recording, build a unique path from template; otherwise no XDF (same as today). |

## Design

1. **Extend [`AppConfig`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/include/lsltemplate/Config.hpp)** with optional recording fields, for example:
   - `bool recording_auto_enabled = false`
   - `std::string recording_directory = "recordings"` (subdirectory under the executable directory when relative)
   - `std::string recording_filename_template = "{stream_name}_{date}_{time}.xdf"`

2. **Section-aware parsing in `ConfigManager::load`** ([`Config.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/Config.cpp)): today `current_section` is read but ignored, so keys are global. Refactor so:
   - `[Stream]` keys (`name`, `type`, `channels`, `sample_rate`, …) only apply when `current_section == "Stream"` (case-sensitive or normalized to match existing files).
   - `[Device]` keys (`device_param`, …) only when section is `Device`.
   - New `[Recording]` keys (`enabled`, `directory`, `filename_template`, optional `basename` to override `stream_name` for filenames only) only when section is `Recording`.

   This avoids collisions if someone adds a `name` under `[Recording]` later.

3. **Append `[Recording]` to [`LSLTemplate.cfg`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/LSLTemplate.cfg)** with commented defaults: `enabled=0` so existing behavior is preserved until the user opts in.

4. **Path resolution helper** (small function in `Config.cpp` + declaration in `Config.hpp`, or a dedicated `.cpp` under `src/emotiv` if you prefer to keep core generic — recommendation: keep **`resolveRecordingOutputPath(const AppConfig&, const std::filesystem::path& exe_dir)`** in `lsltemplate` next to config so the GUI/CLI could reuse it later):

   - If `!recording_auto_enabled`, return `nullopt`.
   - Build base dir = `exe_dir / recording_directory` (if `recording_directory` is absolute, use it as-is per `std::filesystem` rules).
   - Expand template: replace `{stream_name}` with `AppConfig.stream_name`, optional `{basename}` if added, `{date}` → local `YYYY-MM-DD`, `{time}` → `HH-MM-SS` (filesystem-safe). Sanitize or reject path components if needed (minimal: avoid `:` on Windows in the filename only by using `-` for time).
   - `create_directories` for the parent of the final `.xdf` path.
   - Return absolute path string (or `path`) for `record_file`.

   Reuse the same executable-directory logic already used in [`emotiv_lsl_log_config.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_lsl_log_config.cpp) — either export a shared `exe_parent_dir()` from a small shared helper or duplicate the minimal call via `lsltemplate::ConfigManager` / existing `getExecutablePath` in `Config.cpp` (already has `getExecutablePath()` in anonymous namespace; consider exposing a public `getExecutableDirectory()` on `ConfigManager` to avoid duplication).

5. **`main.cpp` (emotiv)** after argv parsing:
   - If `record_file` non-empty → keep as today.
   - Else → `findConfigFile("LSLTemplate.cfg")`, `load`, then `resolveRecordingOutputPath`. If result is set, assign to `record_file` and print the same “Recording natively to XDF file: …” line as manual `--record`.

6. **`ConfigManager::save`**: Update to write `[Recording]` with the new fields so the GUI “Save” does not strip recording options (optional but recommended for consistency).

7. **Docs**: Short paragraph in [`README.md`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/README.md) under the emotiv section: `--record` overrides; otherwise enable `[Recording]` in `LSLTemplate.cfg` next to the executable, placeholders, and example.

## Testing

- No `--record`, default cfg `enabled=0`: no recording (no regression).
- Set `enabled=1` in cfg, run without `--confirm`: file appears under `exe_dir/recordings/…`, logs show resolved path.
- `--record` explicit path: cfg ignored for path choice.
