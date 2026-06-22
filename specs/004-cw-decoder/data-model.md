# Data Model: CW Decoder

**Phase 1 output** · 2026-04-21 · Feature branch `004-cw-decoder`

This document formalizes the entities introduced by SPEC-005 and the runtime state each carries. C++ types are named for clarity; final member visibility and ordering are implementation details.

---

## Entities from the specification

### 1. AudioInputDeviceBinding

Represents the binding between a radio and a system audio input device, stored as part of the per-radio rig configuration (not as standalone decoder state).

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `radioSide` | `enum { Left, Right }` | The radio this binding is for. In non-SO2R mode, only `Left` is used. |
| `deviceDescription` | `QString` | Human-readable device name (from `QAudioDevice::description()`). Empty string = "(none)" = decoder disabled for this radio. |
| `muteDecoderOnPtt` | `bool` | Per-radio setting. Default `true`. When false, both PTT mute paths (FR-019a, FR-019c) are disabled for this radio. |
| `decoderPttGraceMs` | `int` | Grace window in milliseconds added to the internal-send mute duration. Default `250`. Range 0-2000. |

**Persistence**: Stored in `QSettings` under `rig.left.*` and `rig.right.*` alongside the existing `backend / host / port` keys.

**Lifecycle**: Loaded at app startup; re-read on "Apply" in Rig Connection Settings. When `deviceDescription` changes, the running decoder for that radio is stopped cleanly and a new `DecoderSession` is started if the new device is non-empty.

---

### 2. DecoderSession

Represents a live multi-channel decoder running for a single radio. One instance exists per radio whose `AudioInputDeviceBinding.deviceDescription` is non-empty.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `owningRadio` | `enum { Left, Right }` | Immutable once constructed; used for click-fill routing. |
| `deviceDescription` | `QString` | Snapshot of the binding at session start. |
| `passbandLowHz` | `int` | Lower edge of the frequency passband. Default `400`. Range 200-2500. |
| `passbandHighHz` | `int` | Upper edge. Default `1000`. Must exceed `passbandLowHz` by ≥ `binCount × 50` to satisfy DSP resolvability. |
| `binCount` | `int` | Number of parallel Goertzel bins. Default `6`. Range 1-16. |
| `spotlightRowIndex` | `int` | `-1` = no spotlight; otherwise 0-based index into the bin array. |
| `squelchThreshold` | `float` | Normalized signal-strength gate applied uniformly across all bins. Default `0.05`. Range 0.0-1.0. |
| `wpmMin` | `int` | Lower bound for WPM estimator. Default `5`. Range 3-80. |
| `wpmMax` | `int` | Upper bound. Default `60`. Range `wpmMin + 1` - 100. |
| `bins` | `QVector<BinChannel>` | The per-bin state; sized to `binCount` on session start and on every bin-config change. |
| `muteState` | `MuteState` | TX-mute bookkeeping. |
| `audioCapture` | `AudioCapture*` | The QAudioSource wrapper feeding this session. |
| `worker` | `CwDecoderWorker*` | Pointer to the `QObject` running on the decoder thread. |
| `thread` | `QThread*` | Owns the worker's thread. |

**Persistence of tunable fields**: `passbandLowHz`, `passbandHighHz`, `binCount`, `spotlightRowIndex`, `squelchThreshold`, `wpmMin`, `wpmMax` are persisted under `audio/cwDecoder/left/*` or `audio/cwDecoder/right/*` in `QSettings`.

**Runtime-only fields** (NOT persisted): `bins`, `muteState`, `audioCapture`, `worker`, `thread`.

**Lifecycle**:
- **Construct**: When `AudioInputDeviceBinding.deviceDescription` transitions from empty → non-empty.
- **Reconfigure (bin change)**: When `passbandLowHz / passbandHighHz / binCount` change, the worker is stopped, the `bins` vector is rebuilt with new Goertzel coefficients, and the worker is restarted. Must complete within 1 second per FR-018.
- **Destruct**: When `AudioInputDeviceBinding.deviceDescription` transitions from non-empty → empty, or on application exit. Audio capture is stopped, the worker thread is joined, the ring buffer is freed.

**State transitions** (muteState):
```
Idle ──(rigBackend emits pttStateChanged(true))──▶ PttMuted
PttMuted ──(rigBackend emits pttStateChanged(false))──▶ Idle
Idle ──(MainWindow calls muteForInternalSend(N))──▶ InternalSendMuted(until=now+N)
InternalSendMuted ──(now >= until)──▶ Idle
PttMuted + InternalSendMuted → composite; decoder gates while EITHER is active
```

---

### 3. BinChannel

A single Goertzel-based decoder for one frequency bin within a `DecoderSession`. One BinChannel per element of `DecoderSession.bins`.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `centerFreqHz` | `double` | Precomputed from `passbandLowHz + (index + 0.5) × (passbandHighHz − passbandLowHz) / binCount`. |
| `goertzelCoeff` | `double` | `2 * cos(2π * centerFreqHz / sampleRate)`; precomputed once. |
| `goertzelSPrev` | `double` | Running Goertzel state. Reset between blocks. |
| `goertzelSPrev2` | `double` | Running Goertzel state. |
| `toneActive` | `bool` | Per-block flag: is a tone currently detected? |
| `toneActiveStartMs` | `qint64` | Monotonic timestamp of the most recent transition-to-active. |
| `toneInactiveStartMs` | `qint64` | Monotonic timestamp of the most recent transition-to-inactive. |
| `dotLengthWindow` | `QList<int>` | Rolling window of recent dot lengths (ms). Max size 16. |
| `currentWpm` | `int` | Derived from `1200 / median(dotLengthWindow)`. `0` = no lock. |
| `lockState` | `enum { NoLock, Locked }` | `NoLock` when the estimator cannot produce a stable WPM within `wpmMin / wpmMax`. |
| `morseBuffer` | `QString` | Accumulating dit/dah symbols for the current character (e.g., `".-"`). Reset on character boundary. |
| `textBuffer` | `QString` | Scrolling decoded characters for this bin. Rolled at ~10,000 chars. |
| `tokens` | `QList<DecodedToken>` | Active click-fill token spans within `textBuffer`. |

**Lifecycle**:
- **Construct** when the `DecoderSession.bins` vector is sized.
- **Reset** on any bin-config change (full reconstruction).
- **Destruct** with the owning session.

**Per-block update** (called by `CwDecoderWorker` every 10 ms):
1. Run Goertzel recursion across the 80-sample block using `goertzelCoeff`, `goertzelSPrev`, `goertzelSPrev2`.
2. Compute magnitude² = `s_prev² + s_prev2² - coeff * s_prev * s_prev2`.
3. Compare against session's `squelchThreshold × maxExpectedMagnitude²`; set `toneActive`.
4. On `toneActive` transition: close the previous element (dot if short, dash if long per current WPM estimate), append to `morseBuffer`, or close the character/word on long gap.
5. On character boundary: look up `morseBuffer` in the Morse table, append the decoded char to `textBuffer`, emit `charDecoded(binIdx, char)` signal.
6. On character emission: re-scan the tail of `textBuffer` for new `DecodedToken` matches.
7. If a new dot was measured: append to `dotLengthWindow`, recompute `currentWpm`, update `lockState`, emit `wpmUpdated(binIdx, currentWpm)` if changed.

**Gating**: If `DecoderSession.muteState` is non-Idle, step 4 onward is skipped - the Goertzel recursion still runs (so state doesn't go stale when mute drops) but no characters are emitted, no timing is measured, no tokens are created.

---

### 4. DecodedToken

A span within a `BinChannel.textBuffer` that matches a clickable pattern.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `binIndex` | `int` | Originating bin (for UI attribution; routing uses the session's `owningRadio`). |
| `kind` | `enum { Callsign, Rst }` | Which regex matched. |
| `text` | `QString` | The matched text (e.g., `"K1ABC"`, `"599"`). |
| `startOffset` | `int` | Byte offset in `textBuffer` where the match begins. |
| `length` | `int` | Match length in bytes. |

**Lifecycle**: Created by the token parser when a fresh character completes a pattern. Invalidated (removed from `BinChannel.tokens`) when the `textBuffer` is rolled past `startOffset` during buffer trim.

**Click routing**: The widget emits `callClicked(QString)` / `rstClicked(QString)` (without `binIndex`, because routing targets the **owning radio** per Clarify #3/FR-023) to `MainWindow`, which routes to `m_entryWidgets` (Left) or `m_entryWidgetsR` (Right) via the same handler used for keyboard entry.

---

### 5. AudioBlock (internal)

The unit of data on the capture → decoder ring buffer.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `samples` | `int16_t[80]` | 80 samples × 10 ms at 8 kHz. Fixed size. |
| `captureTimestampMs` | `qint64` | Monotonic clock stamp at capture time. Used for latency instrumentation during development; may be dropped in release build. |

**Lifecycle**: Allocated by the capture callback into a preallocated ring-buffer slot; consumed by the worker; slot reused.

---

### 6. MuteState (internal)

Bookkeeping for the two mute paths; owned by `DecoderSession`.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `rigPttActive` | `bool` | Set by the `pttStateChanged(true/false)` signal from the owning rig backend. |
| `internalSendMuteUntilMs` | `qint64` | Absolute monotonic timestamp. Set by `muteForInternalSend(durationMs)`. `0` = inactive. |
| `pttSignalReceivedEver` | `bool` | Tracks whether the rig backend has emitted `pttStateChanged` at least once. If false when needed, the fallback log message (FR-019b) fires once. |

**Derived state**: `isMuted() = rigPttActive || (now() < internalSendMuteUntilMs)`.

---

## Relationship diagram

```
Settings (QSettings)
  │
  ├── rig.left.*                         ┌── AudioInputDeviceBinding(Left)
  └── rig.right.*                        │     + muteDecoderOnPtt, decoderPttGraceMs
              │                          │
              │                          │
   RigControlDialog ◀────edits────▶ AudioInputDeviceBinding
              │
              │     (if deviceDescription non-empty)
              ▼
   MainWindow spawns ─────▶ DecoderSession(owningRadio=Left)
                                   │
                                   ├── AudioCapture ──▶ SPSC Ring Buffer ──▶
                                   ├── QThread
                                   │     └── CwDecoderWorker
                                   │            └── CwDecoder
                                   │                    └── QVector<BinChannel>
                                   │                            ├── Goertzel state
                                   │                            ├── dotLengthWindow
                                   │                            ├── morseBuffer
                                   │                            ├── textBuffer
                                   │                            └── QList<DecodedToken>
                                   ├── MuteState
                                   │     ◀─── pttStateChanged(bool) from owning rig backend
                                   │     ◀─── muteForInternalSend(int) from MainWindow
                                   └── CwDecoderWidget (UI)
                                         ├── N stacked scrolling rows
                                         ├── bin-config controls
                                         ├── squelch slider
                                         ├── spotlight selector
                                         ├── live WPM readout per row
                                         └── emits callClicked / rstClicked
                                              ─▶ MainWindow routes to owningRadio's entry
                                                   (same handler as keyboard entry)
```

---

## Validation rules

Applied at settings-apply time (in `RigControlDialog`) and defensively at `DecoderSession` construction:

- `passbandLowHz` ≥ 200 and ≤ 2400.
- `passbandHighHz` > `passbandLowHz` AND `passbandHighHz` ≤ 2500.
- `binCount` ≥ 1 and ≤ 16.
- `(passbandHighHz − passbandLowHz) / binCount` ≥ 50 Hz (minimum practical bin spacing for 10 ms blocks at 8 kHz).
- `wpmMin` ≥ 3 AND `wpmMax` > `wpmMin` AND `wpmMax` ≤ 100.
- `squelchThreshold` ∈ [0.0, 1.0].
- `decoderPttGraceMs` ∈ [0, 2000].

Violations: the dialog shows an inline error and refuses to save. Defensive failures at session construction are logged and the session falls back to defaults.

---

## Key entity → FR traceability

| Entity | Spec FRs |
|---|---|
| `AudioInputDeviceBinding` | FR-001, FR-002, FR-003, FR-004, FR-004a |
| `DecoderSession` | FR-005, FR-006, FR-010, FR-011, FR-012, FR-014, FR-018, FR-019a, FR-019b, FR-019c, FR-019d, FR-025, FR-026 |
| `BinChannel` | FR-007, FR-008, FR-013, FR-015, FR-016, FR-017, FR-019 |
| `DecodedToken` | FR-020, FR-021, FR-023, FR-024, FR-024a |
| (widget interaction) | FR-009, FR-022, FR-027, FR-028 |
