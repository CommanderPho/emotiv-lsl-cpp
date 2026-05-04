---
name: XDF path logging
overview: "The terminal snippet you shared is **not** recording XDF: no `--record` path was passed. When recording is enabled, the XDF file is opened when the USB dongle connects; LSL sample data is written after the headset brings streams online. The plan adds clear path logs at “meaningful start” and on successful file close, and optionally fixes recorder lifetime so the file actually closes on USB disconnect (not only at process exit)."
todos:
  - id: store-path-recording
    content: Add output path member to `recording`; log path in destructor after successful close
    status: completed
  - id: start-write-log
    content: In `emotiv_base.cpp`, log absolute XDF path once when `saw_streaming` becomes true and recorder exists
    status: completed
  - id: recorder-reset
    content: After `requestStop()` on disconnect, `recorder.reset()` so file closes and close log runs each session
    status: completed
  - id: verify-build
    content: Build emotiv preset and sanity-check with/without `--record`
    status: completed
isProject: false
---

# XDF recording behavior and path logging

## What your terminal output shows

[`src/emotiv/main.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/main.cpp) prints `Recording natively to XDF file: <path>` only when `record_file` is non-empty (set by `--record <path>`). Your log shows:

- `Starting Emotiv LSL C++ Server...`
- **No** `Recording natively to XDF file: ...`

So that run was **LSL streaming only**, not XDF. [`run.ps1`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/run.ps1) forwards arguments with `& $binary @args`; use e.g. `pwsh ./run.ps1 -- --record C:\path\session.xdf` (the `--` separates script args from exe args).

## What happens when `--record` **is** used

```mermaid
sequenceDiagram
  participant Main
  participant EmotivBase
  participant Recording
  participant LSL as LSL_outlets

  Main->>EmotivBase: record_file set
  EmotivBase->>EmotivBase: USB dongle found
  EmotivBase->>Recording: make_unique (watchfor EEG/Motion/Quality)
  Note over Recording: XDFWriter opens/truncates file
  EmotivBase->>LSL: Outlets created when headset data valid
  Recording->>Recording: resolve_stream finds outlets; writes samples
```

- **File created on disk**: As soon as [`recording`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/recording.cpp) is constructed (right after “USB receiver connected … waiting for headset”), because `XDFWriter` opens the path in the `recording` constructor ([`recording.cpp` lines 84–99](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/recording.cpp)).
- **Sample data written**: After the headset produces EEG/motion, outlets are created in [`emotiv_base.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp) (your “Set up EEG outlet!” lines). The recorder’s watch threads then resolve those streams and log lines like `Started data collection for stream ...` ([`recording.cpp` ~207](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/recording.cpp)).

So: **yes, it should record to XDF once streams exist**—but only if `--record` was passed; your pasted run did not.

## Gaps vs what you want

1. **“Print output path when it starts writing”**  
   Today: startup only prints the path in `main` if `--record` is set; there is **no** second message tied to “headset streaming / first real LSL recording.”  
   **Proposal**: In [`emotiv_base.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp), when `saw_streaming` first becomes true **and** `recorder` is non-null, print a single line such as `XDF recording writing to: <absolute path>` (resolve with `std::filesystem::absolute` / `weakly_canonical` on `record_file`). That matches “found the headset” better than dongle-only attach.

2. **“Print again when it successfully stops / closes”**  
   Today: [`recording::~recording()`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/recording.cpp) prints only `Closing the file.` with **no path**. The constructor’s `filename` is not stored on `recording`, so the destructor cannot repeat the path without a small change.  
   **Proposal**: Add a `std::string` member (e.g. `output_path_`) initialized in the `recording` constructor from `filename`, and in the destructor replace/extend the message to include that path after joins succeed (e.g. `Closed XDF: <path>`).

3. **Lifecycle (important for “on disconnect”)**  
   After a read error, [`emotiv_base.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp) calls `recorder->requestStop()` but **never** `recorder.reset()`. The `recording` destructor (which joins threads and destroys `XDFWriter`, flushing/closing the file) therefore runs only when the process exits—not on each USB disconnect. So you may **never** see the destructor’s close message until exit, and the file handle lifetime is tied to process lifetime.  
   **Proposal**: After `requestStop()`, call `recorder.reset()` so `~recording()` runs and closes the file; on the next successful dongle connection, `!recorder` allows creating a **new** `recording` (same or new path—same `--record` path would truncate again on reconnect). This aligns “successful close” with disconnect and makes the new close log meaningful.

## Files to touch

| File | Change |
|------|--------|
| [`src/emotiv/emotiv_base.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/emotiv_base.cpp) | Log absolute XDF path when streaming starts (once per recorder); after disconnect, `recorder.reset()` after `requestStop()` (or fold stop into reset if you add a synchronous `recording` API—simplest is reset driving destructor). |
| [`src/core/include/recording.h`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/include/recording.h) | Store `output_path_` (or similar). |
| [`src/core/src/recording.cpp`](C:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/core/src/recording.cpp) | Initialize path member; destructor message includes path; optionally log once when the file is first opened (if you also want a message at dongle attach—can be redundant with `main`). |

## Testing

- Run without `--record`: no new XDF messages beyond current behavior.
- Run with `--record test.xdf`: after headset streams, see “writing to” with absolute path; unplug dongle or trigger disconnect, see “Closed XDF: …” with same path; confirm file is readable and not locked.
- Ctrl+C: ensure global/static destruction or signal path still closes recorder if applicable (verify whether `EmotivBase` / `recorder` destruction on exit already runs).
