# Emotiv EPOC X — USB HID Conformance Report (`emotiv-lsl-cpp`)

| Field | Value |
|---|---|
| **Date** | 2026-11-25 |
| **Target** | `emotiv-lsl-cpp` (C++) |
| **Transport under audit** | Modality 1 — 2.4 GHz USB HID receiver (`0x1234:0xED02` / `0x21A1:0x0001`) |
| **Ground truth** | `OfficialEmotivSoftwareReverseEngineer/WinVMRevEng/` (`NOTES/02`, `04`, `05`; `validate_epocx_spec.py` — **7/7 self-tests PASS**) |
| **Audit procedure** | `WinVMRevEng/PROMPTS/2026-11-25_Revise_Implementations_To_Official_Spec_PROMPT.md` §3.1 / §3.3–§3.4 / §4 |
| **Overall USB verdict** | **NON-CONFORMANT** — mirrors Python `emotiv-lsl`: **V2 EEG only**; **drops USB Motion**; **broken CQ**; **no Feature/Output Report `0x00`**; **no V4** |

---

## 1. Scope

USB HID interoperability only (enumeration, Feature/Output Report `0x00`, 32-byte input demux, V2/V4 crypto, Mode-7 EEG/CQ/Battery, Motion). BLE GATT is out of scope (README already states USB HID only).

Primary files audited:

| File | Role |
|---|---|
| `src/emotiv/emotiv_base.cpp` | HID enumerate / open / read loop |
| `src/emotiv/emotiv_epoc_x.cpp` / `.h` | Key schedule, decrypt, Mode-7 EEG, Motion |
| `src/emotiv/config.h` | Hardcoded `SRATE=128`, `MOTION_SRATE=16` |
| `src/emotiv/test_epoc_x_decode.cpp` | Unit decode tests (V2-oriented) |

Parity note: C++ intentionally tracks Python `emotiv-lsl` codec behavior, so the USB gap set is essentially identical.

---

## 2. Checklist Summary (USB §3.1 + shared codec §3.3–§3.4)

| # | Spec requirement | Status | Notes |
|---:|---|---|---|
| U1 | VID/PID + `(usage_page & 0xFF00)==0xFF00` + `input_report_length >= 32` | **PARTIAL** | Enumerates `hid_enumerate(0x1234, 0)` then mfg `"Emotiv"` + `usage==2` / `iface==1`. No `0x21A1`, no usage-page mask, no report-length check. |
| U2 | Dual serial: `"UD…"` + Feature Report `0x00` headset hex | **FAIL** | Serial from enumerate only (`find_open_emotiv_device`). No `hid_get_feature_report`. |
| U3 | Parse Feature Report → `headset_fw`, rates/bits | **FAIL** | `config.h` hardcodes rates; `eeg_resolution_bits_=0x10` default. |
| U4 | Output Report `00 66 BB 20 12 <CFG>` + ACK | **FAIL** | Not implemented. |
| U5 | Decrypt all 32B USB reports; demux `0x10` EEG / `0x20` Motion | **FAIL** | `decode_eeg_mode7` returns empty unless type `0x10`; `decode_data` never routes 32B→Motion. |
| C1 | V2 USB key (reversed `"UD…"` → Mode-7 perm → AES-128 + `0x55`) | **PASS** (V2 USB) | `serial_bytes_hid` + `aes_key_epocx_mode` match Python / Cortex. |
| C2 | V4 AES-256 + XOR `0x14` | **FAIL** | No V4 path; always XOR `0x55` + AES-128 (`decode_data` / `decrypt_eeg_packet`). |
| C3 | Mode-7 EEG map + ÷7.8 / ÷1.95 | **PASS** | `kEpocXRawEegIdMode7` / `kAdcDiv*`. |
| C4 | Battery gate + per-electrode CQ demux | **FAIL** | Broadcasts `words[7]&0x7f` to 14 channels. |
| C5 | `"top"` / `"back"` headband | **PARTIAL** | Hardcoded `"top"` in `decode_motion_20`. |
| C6 | `marker_hw` + counter modulus `eeg_rate` | **FAIL** | Marker unused; no counter continuity. |

**Score: 2 PASS / 1 PARTIAL / 8 FAIL** (U1, C5 PARTIAL).

---

## 3. Gap Matrix (file + line)

### Defect 1 — USB 32B Motion drop (**CRITICAL**)

| Spec | Decrypt 32B with `is_mems=false`; `P[1]&0x7F==0x20` → Motion `P[0:20]`. |
|---|---|
| **Code** | `emotiv_epoc_x.cpp` `decode_eeg_mode7` L96–99: non-`0x10` → empty `EmotivData`. `decode_data` L303–337: Motion only if `n==20`; 32B path always `decode_eeg_mode7`. |
| **Impact** | USB Motion frames (encrypted 32B, type `0x20`) never decoded. README already notes motion outlet defaults off “until a real 20-byte HID report is confirmed” — the real USB path is **32B encrypted**, not 20B. |

### Defect 2 — BLE 64/84B

| Status | **N/A** — USB-only transport. |

### Defect 3 — Missing V4 (**CRITICAL**)

| Spec | `fw>=0x073F` → SHA256 V4 key, XOR `0x14`, AES-256-ECB (32B EEG; 20B Motion = 16B cipher + 4B XOR tail). |
|---|---|
| **Code** | Header comment / README: V4 unsupported. `decrypt_eeg_packet` L76–88 and `decode_data` L329–335 always `^=0x55` + AES-128. No `headset_fw` input to key derivation. |
| **Impact** | V4 USB headsets produce garbage; V4 also requires Feature Report headset serial (not `"UD…"`). |

### Defect 4 — Missing Feature Report `0x00`

| Spec | Read Report ID `0x00` (≥18 B); parse `dongle_fw`, `headset_fw`, `serial_hex`, LE rates. |
|---|---|
| **Code** | `emotiv_base.cpp` `find_open_emotiv_device` L87–133 opens path and copies USB descriptor serial; never `hid_get_feature_report`. |
| **Impact** | No firmware branching, no V4 serial seed, no autodetection of 256 Hz / MEMS rate. |

### Defect 5 — CQ broadcast & Battery (**CRITICAL**)

| Spec | Battery only on `counter==eeg_rate-1`; CQ demux full `words[7]` by slot. |
|---|---|
| **Code** | `decode_eeg_mode7` L112–115: `cq = words[7]&0x7f` assigned to all 14 quality channels. LSL note in `get_lsl_outlet_electrode_quality_stream_info` L297–299 documents the incorrect broadcast. |
| **Impact** | CQ stream meaningless; no battery outlet/latch. |

### Defect 6 — Windows HID filter

| Spec | Match `0x1234:0xED02` **or** `0x21A1:0x0001` with `(usage_page&0xFF00)==0xFF00` and `input_report_length>=32`. |
|---|---|
| **Code** | `emotiv_base.cpp` L89: `hid_enumerate(0x1234, 0x0000)`; L95–96 / L117–118: mfg + `usage==2` / `iface==1`. Fallback full enumerate still requires mfg `"Emotiv"`. No `0x21A1`, no usage-page mask. |
| **Impact** | Same as Python: works for many classic dongles; misses alternate VID and can misfire on composite interfaces. |

### Defect 7 — BLE Active Scan / MTU

| Status | **N/A**. |

### Defect 8 — Hardcoded `"top"` orientation

| Spec | Configurable `"top"` vs `"back"`. |
|---|---|
| **Code** | `decode_motion_20` L130–139 always applies `"top"` swaps/negations. |

### Defect 9 — Missing USB mode configuration

| Spec | 33-byte `00 66 BB 20 12 <CFG>`, ACK `0x88 0x12`, re-query Feature Report. |
|---|---|
| **Code** | Absent. `config.h`: `SRATE=128`, `MOTION_SRATE=16` (16 Hz MEMS is **not** a valid hardware rate; live USB ground truth uses **32 Hz**). |

### Defect 10 — Missing trigger & counter wrap

| Spec | Expose `marker_hw`; EEG counter mod `eeg_rate`. |
|---|---|
| **Code** | Type nibble filtered; bit 7 of `plain[1]` unused. No packet-loss interpolation / modulus handling in decode path. |

---

## 4. Confirmed matches (keep)

Aligned with Cortex / `validate_epocx_spec.py` for **V2 USB EEG**:

1. **`kEpocXModePerm`** + `serial_bytes_hid` → V2 USB AES-128 key schedule.
2. **`kEpocXRawEegIdMode7`** channel remap.
3. **ADC ÷7.8 / ÷1.95**.
4. **`"top"` Motion** BE int16 + axis remap + physical scales (when 20B plaintext is provided offline/tests).

`src/emotiv/test_epoc_x_decode.cpp` exercises V2 decode helpers but does **not** assert USB Feature `0x00`, 32B Motion demux after decrypt, V4, or CQ/Battery gate against WinVMRevEng vectors.

---

## 5. Priority fix order (USB)

1. Post-decrypt 32B demux `0x10` / `0x20` (Defect 1) — unlocks USB Motion without waiting for mythical 20B HID reports.
2. `hid_get_feature_report(0x00)` parse + store `headset_fw` / rates / headset serial (Defect 4).
3. V4 key + decrypt (Defect 3).
4. CQ demux + battery latch (Defect 5).
5. Output Report mode switch (Defect 9); fix `MOTION_SRATE` metadata to 32/64/128.
6. HID filter for `0x21A1` + usage-page mask (Defect 6).
7. `headband_position`, `marker_hw` (Defects 8, 10).

Keep public LSL stream names / `.epocxwire` replay compatibility; extend header schema only for newly exposed Battery / CQ / V4 fields as needed.

---

## 6. Oracle cross-check

```text
python OfficialEmotivSoftwareReverseEngineer/WinVMRevEng/validate_epocx_spec.py --self-test
→ ALL VALIDATIONS PASSED (7/7)
```

Recommended regression targets after fixes (from `05_CleanRoom_Reimplementation_Spec.md` §4.2):

| Vector | Expect |
|---|---|
| Feature `00202006ff0720e50202e92b018000102010` | `fw=0x0720`, serial `E50202E9`, 128 Hz / 16-bit / MEMS 32 |
| V2 USB key `"UD20240731007676"` | `36373737373637373637363736373736` |
| USB EEG cipher `1723661a…` | `AF3 = 4210.7692307692305` |
| USB Motion cipher `d33414d7…` | `cnt_mems=26`, `Q0≈0.33037257181502816`, `ACCX≈-0.8848736418019778` |

---

## 7. Verdict

**`emotiv-lsl-cpp` is not USB-spec-conformant for EPOC X.** It is a **faithful port of the Python V2 EEG path**, including the same protocol gaps:

- Correct V2 USB key + Mode-7 EEG µV scaling.
- **Fails** USB 32B Motion demux, Feature/Output Report `0x00`, V4, CQ/Battery, and rate autodetection.

Until Defects 1–5/9 are fixed, treat the C++ LSL publisher as **EEG-only / V2-only / CQ-invalid** on the classic USB dongle.
