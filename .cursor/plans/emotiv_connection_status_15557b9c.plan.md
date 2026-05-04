---
name: Emotiv connection status
overview: Clarify console status so "USB receiver open" is not confused with "headset streaming," and add a paced read loop plus counters so you can distinguish no dongle, dongle idle (waiting for headset), and active EEG/motion packets.
todos:
  - id: reword-connect
    content: Change post-open log to USB receiver + waiting semantics; optionally print serial once
    status: completed
  - id: hid-read-timeout
    content: Use hid_read_timeout in inner loop with small ms timeout for periodic status
    status: completed
  - id: stream-detection
    content: Track raw reports vs first has_eeg||has_motion; throttle waiting/diagnostic messages
    status: completed
  - id: retry-message
    content: Clarify find_open_emotiv_device failure message (dongle/receiver not detected)
    status: completed
  - id: manual-test
    content: Build Release; test no dongle / dongle only / full headset scenarios
    status: completed
isProject: false
---

# Distinct status: dongle vs headset vs streaming

## What goes wrong today

- In [`EmotivBase::main_loop()`](src/emotiv/emotiv_base.cpp), after `find_open_emotiv_device()` succeeds, the code prints **"Connected to Emotiv device: " + `device_name`** (line ~180). `device_name` is the product string from the subclass (e.g. Epoc X in [`emotiv_epoc_x.cpp`](src/emotiv/emotiv_epoc_x.cpp)), not live link state.
- The inner loop uses **`hid_read`** with no timeout ([`emotiv_base.cpp`](src/emotiv/emotiv_base.cpp) ~196–202). On `bytes_read == 0` it **`continue`s with no log**, so there is no feedback while the dongle is present but the headset is off or not yet sending EEG/motion frames.
- **Valid streaming signal**: Treat **`result.has_eeg || result.has_motion`** as "headset is sending usable stream data." Do **not** use `has_quality` alone for that: in [`EmotivEpocX::decode_data()`](src/emotiv/emotiv_epoc_x.cpp), quality is filled from fixed byte indices whenever decryption runs and can look "present" without a real EEG stream.

## Proposed behavior (console)

| Situation | User-visible behavior |
|-----------|------------------------|
| No Emotiv HID interface | Keep retry loop; **reword** message to something like **no USB receiver / dongle detected** (same code path as today: `find_open_emotiv_device()` returns null). |
| HID opened, no EEG/motion yet | One line on connect: **USB receiver open** (include `device_name` and serial if you want). Then **rate-limited** lines (e.g. every 3–5 s): **waiting for headset data** (no valid EEG/motion decoded yet). |
| First EEG or motion sample | One line: **streaming** (EEG and/or motion as applicable). |
| USB reports arrive but 0 EEG/motion after N packets | Optional single diagnostic (or same bucket as "waiting"): **reports received but no EEG/motion — check headset power/pairing** (avoids implying the dongle is "broken" when the radio link is down). |
| `hid_read` error | Keep existing **disconnect / reconnect** message. |

## Implementation (minimal surface area)

**File: [`src/emotiv/emotiv_base.cpp`](src/emotiv/emotiv_base.cpp)** (and only if needed, tiny constants in an anonymous namespace—no header change required unless you extract a small helper).

1. **Replace the connect log** so it does not say "Connected to Emotiv device" in the misleading sense. Example intent: *USB receiver connected — waiting for headset (EEG/motion)…*

2. **Switch the read path to `hid_read_timeout()`** (hidapi already linked) with a modest timeout (e.g. 250–500 ms). This yields:
   - Predictable wakeups for status printing without busy-spinning on `bytes_read == 0`.
   - Same handling for `bytes_read < 0` (reconnect) and `> 0` (process packet).

3. **Per HID session state** (reset each time a new `device` is opened after the inner loop `break`):
   - `bool saw_streaming = false` (set when first `has_eeg || has_motion`).
   - `uint64_t raw_hid_reports` (increment for each `bytes_read > 0`).
   - `steady_clock` last status print time for throttling.

4. **In the inner loop**, when `bytes_read == 0` **or** when data did not yield EEG/motion:
   - If `!saw_streaming` and throttle elapsed, print the **waiting** line (and optionally mention `raw_hid_reports` if > 0 to distinguish "silent dongle" vs "reports but no decode").

5. **Optional polish**: Log **serial_number** once on USB connect (already populated in `find_open_emotiv_device()`) to make support/debugging easier.

## Out of scope (unless you ask)

- **GUI / LSL metadata**: mirroring status in [`src/gui/`](src/gui/) or LSL stream description.
- **New CLI flags** (`--verbose` / JSON): easy to add later if you want machine-readable status.
- **Stricter packet validation** in `validate_data()` (e.g. magic bytes): only if empirical capture shows we need it to avoid counting garbage as "reports."

## Verification

- **No dongle**: paced "no receiver" retry message; process stays up (existing reconnect behavior).
- **Dongle only, headset off**: USB-connected line, then repeating **waiting** lines; no "streaming" until EEG/motion.
- **Headset on**: **waiting** stops being relevant after first stream line; outlets behave as today.
- **Unplug mid-run**: existing disconnect path unchanged.
