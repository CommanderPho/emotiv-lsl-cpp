---
name: Rename default path template prefix
overview: Rename the default filename template prefix from `LabRecorder_` to `EmotivLSLcpp_` in [LabRecorder.cfg](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/LabRecorder.cfg).
todos:
  - id: edit-cfg
    content: Update PathTemplate (line 28) and example comment (line 20) in LabRecorder.cfg from LabRecorder_ to EmotivLSLcpp_
    status: completed
isProject: false
---

## Scope

Two edits in [LabRecorder.cfg](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/LabRecorder.cfg):

- Line 20 (comment example):
  - Before: `;; for example syntax is: path/to/StudyRoot/LabRecorder_%datetime_eeg.xdf`
  - After: `;; for example syntax is: path/to/StudyRoot/EmotivLSLcpp_%datetime_eeg.xdf`
- Line 28 (active default `PathTemplate`):
  - Before: `PathTemplate=LabRecorder_%hostname_%datetime_eeg.xdf`
  - After: `PathTemplate=EmotivLSLcpp_%hostname_%datetime_eeg.xdf`

## Out of scope (intentionally not changed)

- [src/emotiv/lab_recorder_cfg_smoke.cpp](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/src/emotiv/lab_recorder_cfg_smoke.cpp) line 33 — the literal `"LabRecorder_%hostname_%datetime_eeg.xdf"` is a test fixture asserting Qt's `%datetime` prefix-replace behavior; the leading token doesn't affect what's being verified, so leaving it preserves the test's intent and avoids churn. Happy to update for consistency on request.
- [.cursor/plans/labrecorder_cfg_parity_6d28a116.plan.md](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/.cursor/plans/labrecorder_cfg_parity_6d28a116.plan.md) — historical plan doc, not a runtime artifact.

## Verification

- Open [LabRecorder.cfg](c:/Users/pho/repos/EmotivEpoc/ACTIVE_DEV/emotiv-lsl-cpp/LabRecorder.cfg) and confirm both lines updated.
- Optional: run a recording session and confirm the produced `.xdf` filename starts with `EmotivLSLcpp_<hostname>_<datetime>_eeg.xdf` under the configured `StudyRoot`.