---
name: C++ phopylslhelper parity
overview: Mirror PhoPyLSLhelper’s `EasyTimeSyncParsingMixin` stream description fields in `emotiv-lsl-cpp` so XDF-loaded EEG streams expose `phopylslhelper/version`, `stream_start_datetime`, and `stream_start_lsl_local_offset_seconds` in the same format Python’s `readable_dt_str` / parsers expect—fixing PhoPyMNEHelper’s asserts and time alignment logic.
todos:
  - id: add-sync-state-api
    content: Add arbitrary_time_sync_points_ + capture/add methods to emotiv_base.h
    status: completed
  - id: implement-format-append
    content: Implement UTC readable_dt_str twin, precision float string, ctor capture, and phopylslhelper XML in add_lsl_outlet_info_common (emotiv_base.cpp)
    status: completed
  - id: verify-build-xdf
    content: Build emotiv-lsl-cpp and smoke-test XDF desc + PhoPyMNEHelper / pyxdf parse path
    status: completed
isProject: false
---

# C++ parity for PhoPyLSLhelper stream metadata

## Contract (from PhoPyLSLhelper)

- [`PhoPyLSLhelper/src/phopylslhelper/easy_time_sync.py`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/PhoPyLSLhelper/src/phopylslhelper/easy_time_sync.py): `EasyTimeSyncParsingMixin_add_lsl_outlet_info` appends a `phopylslhelper` child under `desc` with `version` = **`1.0.3`**, then for each stored sync point `label`: **`{label}_datetime`** and **`{label}_lsl_local_offset_seconds`** (stringified float).
- [`PhoPyLSLhelper/src/phopylslhelper/general_helpers.py`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/PhoPyLSLhelper/src/phopylslhelper/general_helpers.py): `readable_dt_str` formats UTC as **`strftime("%Y-%m-%d %I:%M:%S %p")`** on the UTC timezone (12-hour clock, zero-padded month/day/hour/minute/second, English **AM/PM**). **`from_readable_dt_str`** parses the same pattern. C++ must match this byte-for-byte style so existing loads don’t break.
- Capture semantics: [`capture_stream_start_timestamps`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/PhoPyLSLhelper/src/phopylslhelper/easy_time_sync.py) stores **`stream_start`** = `(datetime.now(timezone.utc), pylsl.local_clock())`. Emotiv Python never uses `capture_recording_start_timestamps`, but the mixin supports **`recording_start`** the same way. For **feature parity with the mixin**, implement the same hooks so optional `recording_start` can be added later without another format change.

## Consumer impact (PhoPyMNEHelper)

[`PhoPyMNEHelper/src/phopymnehelper/xdf_files.py`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/PhoPyMNEHelper/src/phopymnehelper/xdf_files.py) (e.g. around 662–672) **asserts** `stream_start_lsl_local_offset_seconds` and `stream_start_datetime` when a stream has timestamps. Missing `phopylslhelper` metadata in C++-origin XDFs is the failure mode this change fixes.

## Implementation (emotiv-lsl-cpp only)

All LSL outlets for Emotiv already funnel through [`EmotivBase::add_lsl_outlet_info_common`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp); [`EmotivEpocX`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_epoc_x.cpp) calls this after building channels/cap. **No changes needed in Epoc X–specific code** if the base method appends the XML.

### 1. [`emotiv_base.h`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.h)

- Add a small storage map, e.g. `std::map<std::string, std::pair<std::chrono::system_clock::time_point, double>> arbitrary_time_sync_points_` (label → UTC wall time + `lsl::local_clock()` seconds).
- Add **protected** methods mirroring the mixin:
  - `addArbitraryTimeSyncPoint(...)`
  - `captureCurrentArbitraryTimeSyncPoint(const std::string& label)` (UTC now + `lsl::local_clock()`)
  - `captureStreamStartTimestamps()` / `captureRecordingStartTimestamps()`
- Keep `add_lsl_outlet_info_common` public API as today; internally it will append `phopylslhelper` after existing manufacturer/version/description fields.

### 2. [`emotiv_base.cpp`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp)

- **Constructor**: call `captureStreamStartTimestamps()` so every `EmotivBase` instance gets `stream_start` at construction (same role as `init_EasyTimeSyncParsingMixin()` for Python’s `EmotivBase`; only `EmotivEpocX` exists today in C++, which is fine).
- Add **anonymous-namespace helpers**:
  - `readableDtStrUtc(time_point)` — format with **manual** 12h AM/PM (do **not** rely on locale-dependent `strftime("%p")`).
  - `lslOffsetString(double)` — use a string stream with sufficient precision (e.g. `std::setprecision(17)` + defaultfloat) so `float(...)` in Python round-trips like `str(lsl_offset_sec)`.
- **`add_lsl_outlet_info_common`**: after existing three `append_child_value` calls, `append_child("phopylslhelper")`, set `version` = `1.0.3`, then iterate `arbitrary_time_sync_points_` in sorted order (`std::map` is fine; order is not semantically required). For each entry append `{label}_datetime` and `{label}_lsl_local_offset_seconds`.

### 3. Verification

- **Build** the emotiv CLI/GUI target as you normally do.
- **Smoke test**: run the app, record a short XDF, load in Python with `pyxdf` or PhoPyMNEHelper and confirm `desc` contains `phopylslhelper` with expected keys and that `EasyTimeSyncParsingMixin.parse_and_add_lsl_outlet_info_from_desc(..., should_fail_on_missing=False)` populates `stream_start_datetime` and `stream_start_lsl_local_offset_seconds`.

## Out of scope (unless you want it in the same PR)

- Generic template outlet [`src/core/src/LSLOutlet.cpp`](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/LSLOutlet.cpp) is not used for Emotiv EEG; leaving it unchanged keeps the diff focused on the Emotiv path.
