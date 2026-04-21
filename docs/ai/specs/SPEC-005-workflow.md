# SpecKit Workflow: SPEC-005 — CW Decoder

**Template Version**: 1.0.0
**Created**: 2026-04-21
**Purpose**: End-to-end SpecKit workflow for the CW Decoder feature.

---

## Workflow Overview

| Phase | Command | Status | Notes |
|-------|---------|--------|-------|
| Specify | `/speckit.specify` | ✅ Done | `specs/004-cw-decoder/spec.md` + `checklists/requirements.md` — 8 user stories, 30 FRs, 10 SCs, 0 `[NEEDS CLARIFICATION]` markers |
| Clarify | `/speckit.clarify` | ✅ Done | 4 Q/A in Session 2026-04-21: (1) multi-channel skimmer, (2) per-radio PTT mute default ON, (3) click-fill identical to keyboard entry, (4) internal-send signalling for F-key/CW-console mutes |
| Plan | `/speckit.plan` | ✅ Done | `specs/004-cw-decoder/plan.md` + `research.md` (11 decisions) + `data-model.md` (6 entities) + `contracts/signals.md` + `quickstart.md`. Found a concrete prerequisite gap: `RigInterface` needs a new `pttStateChanged(bool)` signal (R4). Constitution Check: all 5 principles pass; `Qt6::Multimedia` module addition approved at gate. |
| Checklist | `/speckit.checklist` | ✅ Done | 4 domains, 187 items: `ux.md` (48) · `performance.md` (41) · `error-handling.md` (49) · `cross-platform.md` (49). All items test requirements quality (Gap/Ambiguity/Conflict/Clarity/Completeness/Coverage/Consistency/Measurability markers) — "unit tests for English" |
| Tasks | `/speckit.tasks` | ✅ Done | `specs/004-cw-decoder/tasks.md` — 70 tasks across 11 phases (includes 5 analyze-phase coverage-gap closures). P1 MVP = US1+US2+US3+US5+US8 (36 tasks incl. Clear). P2 refinements = US4+US6+US7 (12 tasks). Foundational (Phase 2) adds `pttStateChanged` signal to RigInterface + all three backends. TDD tests included per plan. |
| Analyze | `/speckit.analyze` | ✅ Done | 0 CRITICAL, 4 HIGH, 3 MEDIUM, 2 LOW findings. All 4 HIGH and both MEDIUM spec-ambiguity findings remediated: added T033a (FR-012 Clear), T062a (SC-005 keystroke latency), T062b (SC-009 CPU budget), T062c (SC-008 cross-platform CI), T062d (SC-007 zero-resource); tightened Spotlight assumption to "highlighted background only" and quantified PTT self-decode leak tolerance to ≤ 500 ms / ~5 chars per TX event. |
| Implement | `/speckit.implement` | 🔄 Partial | Phase 1–7 implementation complete as far as compilable code goes. `make` passes clean with 0 warnings; all 6 existing tests pass. New audio subsystem (`src/audio/`, `include/audio/`) is ~1100 LOC + widget ~500 LOC. Remaining: T032 (audit CW-send sites to call notifyInternalCwSend), T040 (render tokens as clickable anchors instead of auto-emit), T054 RST emit enable, T011 macOS plist, all TDD test files (T018–T021, T034, T038, T044, T048, T053, T056), Phase 11 polish (CHANGELOG, Window menu entries, CPU profile, CI update). |

---

## Gate Checklist

| Gate | After | Pass Criteria |
|------|-------|---------------|
| G1 | Specify | All user stories clear, no `[NEEDS CLARIFICATION]` markers remain |
| G2 | Clarify | SO2R routing, tone acquisition, and click-to-fill token semantics fully resolved |
| G3 | Plan | Architecture approved, constitution gates pass, Qt6::Multimedia dependency approved |
| G4 | Checklist | All `[Gap]` markers addressed (including cross-platform audio differences) |
| G5 | Tasks | Task coverage verified, DSP/UI/integration phases ordered |
| G6 | Analyze | No `CRITICAL` issues, `WARNING` items reviewed |
| G7 | Implement | `make` succeeds, `make test` passes, decoder verified against known CW audio on Linux |

---

## Prerequisites

### Constitution Validation

| Principle | Requirement | Verification |
|-----------|-------------|--------------|
| I. Contest Accuracy | Decoder output is advisory — the operator's typed CALL/exchange remain authoritative; no QSO is logged from decoder output alone | Code review + UX review |
| II. Qt6-Native Architecture | Widget MUST use Qt6 idioms (QDockWidget, signals/slots, `QAudioSource`, `QMediaDevices`). Adds `Qt6::Multimedia` — a new Qt6 module, not a third-party dep — to the approved stack | Code review + CMake review |
| III. Keyboard-First | Decoder runs off-thread; click-to-fill is an enhancement. QSO entry keyboard latency MUST NOT regress. Clicking a token routes to the active radio's entry panel in SO2R (no focus steal) | Manual test |
| IV. JSON-Driven Contests | Decoder behavior is contest-agnostic — no contest JSON changes | Code review |
| V. Simplicity | No full FFT waterfall; Goertzel tone detect only. No CW *sending* (existing CW console owns that). No RTTY / digital / SSB. No network sharing of decoded text | Code review |

**New dependency note:** `Qt6::Multimedia` is added to `CMakeLists.txt`. The constitution lists Core, Widgets, Network, SerialPort, Xml as the approved Qt6 modules — this spec proposes extending that list by one module. Adopt via PR review; no third-party library introduced.

**Constitution Check:** ⏳ (mark ✅ / ❌ before proceeding to G1)

---

## Specification Context

### Basic Information

| Field | Value |
|-------|-------|
| **Spec ID** | SPEC-005 |
| **Name** | CW Decoder |
| **Branch** | `004-cw-decoder` (created by `/speckit.specify`) |
| **Dependencies** | None — truly independent feature |
| **Enables** | Nothing else in the roadmap depends on it |
| **Priority** | P2 |

### Success Criteria Summary

- [ ] CW Decoder dock widget appears in the Window menu; docks, floats, and closes cleanly
- [ ] Rig Connection Settings dialog has a per-radio "Audio Input Device" dropdown (alongside backend/host/port/CAT), listing system audio input devices; empty selection is valid and disables the decoder for that radio
- [ ] In SO2R mode, any combination (neither, L only, R only, both radios with audio) works correctly; decoder instances are spawned only for radios with a configured audio device
- [ ] Decoded text streams into a scrolling view in real time (≤ 200 ms perceptible latency at 25 WPM)
- [ ] WPM is continuously auto-detected from the received signal's dot length and displayed live; operator sets only a bounding hint range (default 5–60 WPM); no manual WPM setting
- [ ] Tone frequency is auto-acquired in a configurable passband; operator can pin/unpin it
- [ ] Squelch / threshold slider suppresses noise-driven false decodes
- [ ] Clicking a callsign-shaped token fills the CALL field in the **owning** radio's QSO entry panel (the radio whose audio decoded the token — not necessarily the currently-active radio)
- [ ] Clicking an RST-shaped token fills the RSTr field in the owning radio's QSO entry panel
- [ ] No perceptible QSO entry latency regression with the decoder running
- [ ] Decoder degrades gracefully when no audio device is available or selected
- [ ] Widget state (device, threshold, passband, dock position) persists across sessions
- [ ] Builds and runs on Linux, macOS, Windows (`make` + CI green on all three)

---

## Phase 1: Specify

**When to run:** At the start. Focus on WHAT and WHY, not implementation details.
**Output:** `specs/004-cw-decoder/spec.md` (sequential numbering; SpecKit may pick the next available integer)

### Specify Prompt

```
/speckit.specify

## Feature: CW Decoder (SPEC-005)

### Problem Statement

Contest operators copying CW at speed often miss characters under pileups, QSB,
or fatigue. External decoders (fldigi, MRP40, CW Skimmer) work but require
window-switching, separate audio routing, and manual transcription of callsigns
back into the log. ContestLogX should provide an integrated CW decoder dock
that captures audio from the same system audio device the operator already
routes their radio's receive audio to, decodes incoming Morse in real time,
and lets the operator click decoded callsigns and RST values directly into the
QSO entry panel. The decoder is a copy assist — the operator remains the
authority on what gets logged.

### Users

- Contest operators copying CW, especially during high-rate runs or under
  difficult conditions (QRM/QSB/weak signals)
- Operators new to CW contesting who want a backup when their head-copy fails
- SO2R operators who need decoder support on one or both radios simultaneously

### User Stories

1. As an operator, I want to configure an **audio input device per radio** in
   the existing Rig Connection Settings dialog (alongside backend/host/port/CAT),
   so the radio's audio source is declared at rig setup time — not in a
   separate widget. Any radio without an audio device configured simply has
   no decoder active; this is legal and common (e.g., SO2R where only one
   radio has an audio feed wired to the PC).

2. As an operator, I want decoded CW text to stream into a scrolling view in
   real time, so I can read what I missed when my ears can't keep up.

3. As an operator, I want the decoder to continuously auto-detect and adapt to
   the sender's WPM (within a hint range I configure), so I never have to set
   a speed manually — the decoder listens, measures the dot length, and adjusts
   its receive WPM in real time as different stations and speeds come and go.

4. As an operator, I want the decoder to auto-acquire the tone frequency within
   a configurable passband, and let me pin the current tone so the decoder
   stays locked to one signal even if a stronger carrier appears nearby.

5. As an operator, I want to click a callsign-shaped token in the decoded text
   and have it fill the CALL field of the active radio's QSO entry panel, so I
   can skip retyping.

6. As an operator, I want to click a signal-report-shaped token (e.g., 599,
   5NN, 57) and have it fill the RSTr field of the active radio's QSO entry
   panel, so the exchange captures quickly.

7. As an operator, I want a squelch/threshold control so background noise
   doesn't produce spurious characters in the decode buffer.

8. As an SO2R operator, with an audio device configured for Radio L and
   optionally Radio R (either, both, or neither are legal), I want each
   radio's decoded output to route unambiguously to that radio's QSO entry
   panel (click-to-fill CALL/RSTr always fills the owning radio's fields,
   not the currently-active radio's fields), so I can copy both radios
   independently without focus confusion.

### Constraints

- Audio capture MUST use Qt6::Multimedia's `QAudioSource` and enumerate devices
  via `QMediaDevices` — no platform-specific audio APIs, no third-party DSP library
- The audio input device is a **per-radio setting**, stored in the existing
  rig configuration (Rig Connection Settings dialog), alongside backend, host,
  port, and CAT serial device. An empty / unselected audio device is valid
  and MUST be treated as "no decoder for this radio"
- ContestLogX does NOT access flrig's audio — the user provides audio via
  a system input device (USB audio endpoint on the radio, soundcard line-in,
  virtual audio cable, etc.), the same approach fldigi and other decoders use.
  Note: radios like the Elecraft K4 or Icom IC-7300 expose audio as a USB
  audio endpoint distinct from the CAT serial endpoint; both are enumerated
  separately by the OS and configured independently in the rig settings
- SO2R: each radio (L and R) has its own independent audio device setting.
  Any combination is legal — neither, L only, R only, or both. The decoder
  instance per radio is spawned when that radio has an audio device set
- Click-to-fill MUST route to the **owning radio's** QSO entry panel (the
  radio whose audio produced the decoded token), NOT the currently-active
  radio. This is deterministic because audio is bound to a radio at
  configuration time
- The DSP pipeline MUST use the Goertzel algorithm for single-frequency tone
  detection — a full FFT is not needed and is out of scope
- Speed detection MUST be continuously adaptive — the decoder derives the
  current WPM from the measured dot length of the received signal (PARIS
  standard: WPM = 1200 / dot_ms) using a rolling-median estimator, and the
  updated WPM feeds back into the dot/dash classifier so speed changes
  mid-QSO are tracked within a few characters
- Adaptation MUST be bounded by an operator-configurable WPM hint range
  (default 5–60 WPM) so noise bursts cannot drive the estimator out of
  a plausible contest-speed window
- No manual WPM setting — the operator sets only the bounding range; the
  live WPM is derived from the signal and displayed read-only
- Clicking a decoded token MUST route to the *active* radio's QSO entry panel
  in SO2R mode — never to a non-active radio, never steal focus away from
  what the operator is typing
- The CW Decoder is a COPY ASSIST — no QSO is auto-logged from decoder
  output; the operator's keyboard input is authoritative
- Widget MUST be a QDockWidget (dockable, floatable, closable) consistent
  with all other panels
- Widget state (device selection, threshold, passband, dock position) MUST
  persist via QSettings and QMainWindow saveState/restoreState
- Cross-platform: Linux (PulseAudio/PipeWire), macOS (CoreAudio), Windows
  (WASAPI) — all via Qt6::Multimedia abstraction
- QSO entry keyboard latency MUST NOT regress when decoder is running

### Out of Scope

- Sending CW via the decoder (the existing CW console + flrig owns keying)
- SSB voice-to-text decoding
- RTTY, FT8, PSK, or any digital-mode decoding (WSJT-X UDP already covers FT8)
- Full FFT / waterfall / panadapter display
- Network sharing of decoded text across multi-op stations (possible future)
- Simultaneous multi-signal separation within a single passband (operator
  tunes so one signal is in the passband)
- Direct audio capture from flrig (no such API exists; audio is a system-level
  routing concern)
- Auto-logging a QSO based on decoder output alone
```

### Specify Results

| Metric | Value |
|--------|-------|
| Functional Requirements | 30 (FR-001 … FR-030) |
| User Stories | 8 (US1–US3+US5+US8 = P1; US4, US6, US7 = P2) |
| Acceptance Criteria | ≥ 3 scenarios per story |
| Success Criteria | 10 (SC-001 … SC-010) |
| `[NEEDS CLARIFICATION]` markers | 0 |
| Assumptions documented | 10 |

### Files Generated

- [x] `specs/004-cw-decoder/spec.md`
- [x] `specs/004-cw-decoder/checklists/requirements.md`

---

## Phase 2: Clarify

**When to run:** After spec — several operator-facing behaviors are ambiguous and
benefit from explicit decisions before planning. Expect two sessions minimum.

### Clarify Session 1: Widget Layout & Token Routing

```
/speckit.clarify Focus on widget layout and click-to-fill semantics
(SO2R routing is already resolved: click-to-fill always targets the
owning radio — the radio whose audio produced the token):

- Widget layout: TWO separate CwDecoderWidget docks (Left / Right, mirroring
  the SO2R QSO entry pattern where entryWidgets L/R already exist as
  separate docks), or ONE dock containing two panels side-by-side, or ONE
  dock with a tab per radio? Recommendation needed for consistency with
  the existing SO2R UX.
- In single-radio (non-SO2R) mode, how is the widget presented — a single
  unlabeled decoder, or still labeled "Radio L"? (Recommend: unlabeled
  when SO2R is off; labeled when SO2R is on.)
- If the target QSO entry field is already populated, does clicking
  overwrite, append, or do nothing? (Recommend: overwrite, matching how
  DX cluster row-click behaves today.)
- Does clicking steal focus to the QSO entry field, or leave focus wherever
  it is? (Recommend: do not steal focus — respect Principle III.)
- Should clicking a CALL token also trigger the existing partial-check (SCP)
  lookup the same way keyboard entry does?
- Radio with no audio device configured: is the decoder dock hidden
  entirely, or shown with a "No audio device configured — set one in Rig
  Connection Settings" message? (Recommend: hidden when the radio has no
  audio device, so the operator doesn't see dead UI.)
```

### Clarify Session 2: Tone Acquisition & Decoder Behavior

```
/speckit.clarify Focus on tone acquisition, speed tracking, and token detection:

- Tone acquisition default: auto-detect the strongest tone in a configurable
  passband (e.g., 400–1000 Hz) — what passband default is best? How does the
  decoder behave if multiple strong tones exist (stick with first acquired,
  or always follow strongest)?
- When the operator "pins" the current tone, is that pin persistent across
  app restarts, or per-session only?
- Callsign token detection: what regex/heuristic decides a decoded string
  is a callsign? How do we handle portable/slash notation (e.g., `PJ2/N9OH`,
  `YB1AR/2`) — do those count as callsign tokens?
- RST token detection: accept `599`, `5NN` (CW short form with N=9), `57`,
  `-15` (digital-mode SNR-style), anything else?
- Speed (WPM) tracking is continuously adaptive (established in the spec).
  Open questions: what rolling-window size for the dot-length median (too
  small = jittery, too large = slow to adapt)? What convergence time after
  a speed change is acceptable (e.g., ≤ 5 characters to track a ±10 WPM
  change)? Does the decoder ever "freeze" on command (e.g., a lock
  checkbox) for operators who want stable tracking on a rag-chewer, or is
  lock-free adaptation always on?
- Squelch/threshold: is this a signal-level gate (SNR), a dot-duration gate
  (reject too-short pulses), or both?
- Decode buffer: how much history is retained in the scrolling view? Does
  the clear button wipe everything, or just the on-screen portion?
```

### Clarify Results

| Session | Focus Area | Questions | Key Outcomes |
|---------|------------|-----------|--------------|
| 1 | SO2R & Token Routing | 5 | (fill after running) |
| 2 | Tone / Speed / Tokens | 7 | (fill after running) |

---

## Phase 3: Plan

**When to run:** After spec is finalized and clarify questions resolved.
**Output:** `specs/004-cw-decoder/plan.md`

### Plan Prompt

```
/speckit.plan

## Tech Stack

- Language: C++17
- Framework: Qt6 (Core, Widgets, Network, Xml) + NEW: Qt6::Multimedia
- UI: QDockWidget + custom QWidget (scrolling text view + controls toolbar)
- Audio: QAudioSource (Qt6::Multimedia) for capture; QMediaDevices for device
  enumeration; QAudioFormat for 8 kHz / 16 kHz mono PCM S16LE configuration
- DSP: Goertzel algorithm (single-frequency tone detector), implemented inline;
  adaptive dot-length estimator for WPM tracking; Morse lookup table for
  character decode
- Threading: DSP runs on a dedicated QThread (mirror of HamlibWorker pattern
  in src/rig/hamlibClient.cpp) — audio samples posted from the QAudioSource
  callback into a ring buffer; worker pulls blocks, runs Goertzel +
  decoder, emits `charDecoded(QChar)` and `wpmUpdated(int)` signals
- Build: CMake — add `find_package(Qt6 REQUIRED COMPONENTS Multimedia)` and
  link `Qt6::Multimedia` on the main target
- Testing: Unit tests for Morse-table lookup, dot-length estimator, and
  Goertzel coefficient math (pure C++ — no audio hardware needed); manual
  verification for end-to-end audio path

## Architecture Notes (updated per Clarify 2026-04-21)

- `CwDecoderWidget` (src/ui/cwDecoderWidget.cpp / include/cwDecoderWidget.h)
  — QDockWidget bound to a specific radio (Left or Right); hosts a
  stacked list of N bin rows (one scrolling text view per bin), plus
  shared controls: squelch slider, bin-config (passband + bin count),
  bounding-WPM-range setting, spotlight-row selector, clear button.
  **No audio device selector** — device comes from owning radio's rig
  config. Constructor takes a radio identifier; widget knows its owning
  radio at all times for deterministic click-to-fill routing.
- `AudioCapture` (src/audio/audioCapture.cpp / include/audio/audioCapture.h)
  — QAudioSource wrapper; configures format (8 kHz or 16 kHz mono S16LE);
  emits raw audio blocks to the decoder worker's ring buffer; constructed
  with a specific QAudioDevice pulled from rig config
- `CwDecoder` (src/audio/cwDecoder.cpp / include/audio/cwDecoder.h) — pure
  DSP + timing engine **with a BinChannel array**. Each BinChannel owns:
  its own Goertzel coefficients (for that bin's center frequency), dot/dash
  classifier state, rolling-median dot-length estimator (live WPM per
  PARIS: `WPM = 1200 / median_dot_ms`), Morse-table state machine, and
  decoded-character buffer. For each incoming audio block, the decoder
  iterates every BinChannel and runs its Goertzel + state machine. WPM
  adaptation and character emission are per-bin and independent. Emits
  `charDecoded(binIdx, QChar)` and `wpmUpdated(binIdx, int)` signals.
- `CwDecoderWorker` (src/audio/cwDecoderWorker.cpp) — QObject on a
  QThread; owns the CwDecoder; consumes ring buffer; emits per-bin
  decoded chars + WPM back to the widget via Qt::QueuedConnection.
- `TokenParser` — per-bin: scans each bin's decoded text buffer as
  characters arrive, identifies CALL and RST candidates, turns them into
  clickable anchors in that bin's text row.
- `MainWindow` wiring:
  - Up to two decoder widgets (one per radio that has audio configured).
  - Click signals `callClicked(QString)` / `rstClicked(QString)` from each
    widget route to that widget's owning radio's entry panel fields
    (m_entryWidgets for Left, m_entryWidgetsR for Right) via the **same
    code path that keyboard entry uses** — so SCP/call-history/dupe-check/
    name-QTH auto-fill all fire identically (per Clarify #3). No parallel
    fill code path.
  - **PTT mute (path 1 — rig-backend)**: rig backend's PTT signal is
    connected to the decoder's muteForPtt slot; while PTT is true and
    "Mute on PTT" is enabled, decoder gates all bins (no char emission,
    no token detection). Falls back to always-active + one-time debug log
    if backend doesn't report PTT.
  - **PTT mute (path 2 — internal send)**: MainWindow calls
    `decoder->muteForInternalSend(durationMs)` whenever it initiates a CW
    send (F-key memory, CW console, any cwio_text path). Duration =
    (text_chars × 1200 / send_wpm) + grace window (default 250 ms).
    Independent of backend accuracy.
- `Settings` extension:
  - `rig.left.audioInputDevice` / `rig.right.audioInputDevice` (QString
    device name, empty = disabled)
  - `rig.left.muteDecoderOnPtt` / `rig.right.muteDecoderOnPtt` (bool,
    default true)
  - `rig.left.decoderPttGraceMs` / `rig.right.decoderPttGraceMs` (int,
    default 250)
  - `audio/cwDecoder/left/*` + `audio/cwDecoder/right/*` runtime settings:
    `passbandLowHz`, `passbandHighHz`, `binCount`, `spotlightRowIndex`,
    `wpmMin`, `wpmMax`, `squelch`
- `Rig Connection Settings dialog`: each radio tab gains:
  - "Audio Input Device" combo (populated from `QMediaDevices::audioInputs()`
    plus an explicit "(none)" entry)
  - "Mute decoder on PTT" checkbox (default checked)
  - "PTT grace window (ms)" spin box (default 250)

## Project Structure

src/audio/                   — NEW: DSP + audio capture (cwDecoder,
                               audioCapture, cwDecoderWorker)
include/audio/               — NEW: audio headers
src/ui/cwDecoderWidget.cpp   — per-radio widget (radio-bound via constructor)
include/cwDecoderWidget.h    — widget header (flat include/ per project convention)
src/ui/rigControlDialog.cpp  — add Audio Input Device combo per radio tab
include/rigControlDialog.h   — add m_audioDeviceCombo members for L/R
src/utils/settings.cpp       — add rig.{left,right}.audioInputDevice keys +
                               audio/cwDecoder/{left,right}/* runtime keys
src/ui/mainWindow.cpp        — spawn decoder widget(s) based on rig audio
                               config; wire click signals to each widget's
                               owning entry panel (Left → m_entryWidgets,
                               Right → m_entryWidgetsR)
CMakeLists.txt               — add Qt6::Multimedia to find_package and link
tests/test_cwDecoder.cpp     — Morse table + dot-length estimator unit tests
tests/test_goertzel.cpp      — Goertzel correctness on synthetic sine input

## Constraints

- No third-party libraries — DSP implemented from scratch on Qt6::Multimedia
- Decoder worker runs on a QThread; DO NOT do DSP on the main thread
- Signal from capture → ring buffer → worker MUST be lock-free or use a
  QMutex small enough not to block the audio callback
- Use QAudioFormat with 8 kHz or 16 kHz mono S16LE; reject any other format
  by falling back to default device + logging an error in the status area
- Cross-platform: verify device enumeration works on at least Linux
  (PipeWire/PulseAudio) and that no macOS/Windows-only API leaks in
- HiDPI-safe: scrolling text uses QTextEdit/QPlainTextEdit (Qt handles
  scaling); custom toolbar uses standard widgets, no manual pixel math
```

### Plan Results

| Artifact | Status | Notes |
|----------|--------|-------|
| `plan.md` | ⏳ | |
| `research.md` | ⏳ | Goertzel coefficient derivation, WPM timing standards |
| `data-model.md` | ⏳ | Ring buffer format, decoded-char stream |
| `contracts/` | ⏳ | Signal interfaces (capture → worker → widget → mainWindow) |
| `quickstart.md` | ⏳ | How to route CW audio via virtual cable (per-platform) |

---

## Phase 4: Domain Checklists

**Recommended domains** (from spec analysis):

| Signal in Spec | Domain |
|---|---|
| Scrolling text view, clickable tokens, device selector, threshold slider | **ux** |
| Real-time DSP on audio stream, worker thread, QSO entry latency guarantee | **performance** |
| Audio device unavailable, unsupported format, device disconnected mid-session | **error-handling** |
| Qt6::Multimedia device enumeration differs on Linux / macOS / Windows | **cross-platform** |

### Checklist 1: UX

```
/speckit.checklist ux

Focus on CW Decoder requirements:
- Scrolling text view: how much history is visible? Are decoded tokens
  visually distinct from plain text (e.g., callsigns underlined, RST
  highlighted)? Click targets large enough on HiDPI displays?
- Device selector: is the list refreshed when the operator plugs in a new
  device, or only at startup? How is the "no devices available" state shown?
- Tone/WPM readouts: are they live-updating? Is the auto-detected state
  visually distinct from pinned state?
- Squelch slider: is the effect audible/visible immediately (e.g., decoder
  output stops when threshold exceeds signal)?
- SO2R: if there is a per-radio device setting, how is it presented — a
  tab in the widget, a dropdown, a preference dialog entry?
- Empty states: no audio device selected, device selected but no signal
  detected, squelched out — each needs a clear visual cue
- Pay special attention to: click-to-fill feedback — does the operator get
  confirmation that the CALL field was updated (e.g., brief highlight)?
```

### Checklist 2: Performance

```
/speckit.checklist performance

Focus on CW Decoder requirements:
- End-to-end decode latency: audio-in → decoded char appearing on screen
  MUST be ≤ 200 ms perceptible; what block size (samples per Goertzel
  evaluation) achieves this without excessive CPU?
- QSO entry keyboard latency: must be unaffected — validate with the
  decoder running at 40 WPM against a loud signal for 60 seconds
- DSP worker CPU: target < 5% of one core on a modern CPU; stress-test
  with noisy audio that triggers many Goertzel rejections
- Ring buffer sizing: too small → audio drops; too large → latency. What
  size balances both for 8 kHz / 10 ms block size?
- Qt event throughput: charDecoded signal fires per character — at 40 WPM
  that's ~8 chars/sec. Negligible. At burst-detection false positives it
  could spike — is there a rate limiter?
- Scrolling text view: appending chars forever — when does it get trimmed
  to prevent unbounded growth?
- Pay special attention to: the first seconds after starting capture (device
  warmup, format negotiation) — acceptable if decode takes 1–2 sec to start?
```

### Checklist 3: Error Handling

```
/speckit.checklist error-handling

Focus on CW Decoder requirements:
- No audio device available: widget shows empty state, no crash, no silent
  failure
- Selected device disappears (USB unplug, virtual cable process exits): detect,
  show reconnect state, optionally retry
- Device returns unsupported format: fall back to default or log and bail
  out gracefully
- Decoder worker crashes / throws: main thread survives; widget shows an
  error state, offers restart
- Click-to-fill when no QSO entry panel is focused or no contest is loaded:
  show status bar message, do not crash
- Pay special attention to: what happens if audio is silent for a long time
  (overnight) — does the decoder consume zero CPU, or spin?
```

### Checklist 4: Cross-Platform

```
/speckit.checklist cross-platform

Focus on CW Decoder requirements:
- Linux: PipeWire vs PulseAudio device enumeration — does QMediaDevices
  return the same device names under both? What about pw-loopback /
  module-loopback virtual sinks?
- macOS: CoreAudio device permissions — does Qt6::Multimedia require an
  Info.plist microphone-usage string? What about loopback (BlackHole,
  Loopback.app)?
- Windows: WASAPI shared vs exclusive mode — default is shared; does
  Qt6::Multimedia expose the choice? What about Stereo Mix / VB-CABLE?
- Device name stability: if the operator saves "USB Audio Device" as their
  selection, does it re-resolve correctly across reboots and plugged-in
  order changes?
- CMake: does Qt6::Multimedia linkage succeed on all three CI builds
  (Ubuntu, macOS, Windows MSVC)?
- Documentation: the quickstart should include a minimal per-platform
  recipe for routing radio audio to ContestLogX
- Pay special attention to: the Windows build already uses MSVC — verify
  no GCC-only Qt Multimedia extensions are used
```

### Checklist Results

| Checklist | Items | Gaps | Notes |
|-----------|-------|------|-------|
| ux | | | (fill after running) |
| performance | | | (fill after running) |
| error-handling | | | (fill after running) |
| cross-platform | | | (fill after running) |

---

## Phase 5: Tasks

**Output:** `specs/004-cw-decoder/tasks.md`

### Tasks Prompt

```
/speckit.tasks

## Task Structure
- Small, testable chunks (1-2 hours each)
- Clear acceptance criteria referencing FR-xxx from spec.md
- Dependency ordering: audio capture scaffold → DSP → decoder → widget →
  integration → polish
- Mark parallel-safe tasks with [P]
- Organize by user story

## Project File Layout

Source files:
  src/audio/        — NEW directory: DSP + capture (cwDecoder, audioCapture)
  src/ui/           — UI widgets (cwDecoderWidget.cpp goes here)
  include/          — All headers (flat — note: include/audio/ subdirectory
                      IS allowed per SPEC-005 plan, consistent with the
                      master plan's environment section)
  tests/            — Unit tests

Build commands:
  make              — build
  make test         — unit tests (Morse table, Goertzel, dot-length estimator)
  make test-logs    — not applicable to this spec (no scoring changes)

## Implementation Phases (updated per Clarify 2026-04-21)
1. Foundation — CMake + Qt6::Multimedia linkage; per-radio audio device +
   "Mute on PTT" + "PTT grace ms" settings plumbed through Settings; Rig
   Connection Settings dialog gains all three controls per radio tab
2. User Story 2 (DSP core) — BinChannel class (Goertzel + dot/dash timing
   + Morse table + rolling-median dot-length WPM estimator); CwDecoder
   orchestrates an array of BinChannels; unit-testable with synthetic
   audio, no hardware needed. Covers US2 + US3 WPM adaptation together
   since per-bin WPM is inherent to the BinChannel
3. User Story 1 — AudioCapture wired to a specific QAudioDevice from rig
   config; MainWindow spawns a CwDecoderWidget per configured radio; the
   widget renders N scrolling bin rows stacked vertically with the
   bin's center frequency labeled; radios with empty audio device get no
   widget
4. User Story 4 — Passband + bin-count + spotlight controls in the widget;
   DSP restart on bin-config change within 1 second (FR-018)
5. User Story 7 — Squelch/threshold slider (global across bins)
6. User Story 5 — Callsign token detection per bin + click-to-fill CALL
   that calls the SAME entry-field handler as keyboard input (SCP /
   call-history / dupe-check / name-QTH all fire identically per
   Clarify #3); click does not steal focus
7. User Story 6 — RST token detection per bin + click-to-fill RSTr via
   the same keyboard-equivalent path
8. PTT mute (both paths) — rig-backend PTT signal wired to decoder gate;
   MainWindow internal-send signalling from F-key and CW-console paths
   with estimated-duration + grace-window mute (FR-019a/b/c/d)
9. User Story 8 — SO2R verification: exercise all four audio-config
   combinations (neither / L-only / R-only / both) and confirm click
   routing targets the owning radio in every case; verify both PTT mute
   paths work independently per radio
10. Polish — Settings persistence, Window menu integration, empty states
    (no audio device → no widget), cross-platform quickstart docs
```

### Tasks Results

| Metric | Value |
|--------|-------|
| Total Tasks | (fill after running) |
| Phases | 10 |
| Parallel Opportunities | (fill after running) |
| User Stories Covered | 8 |

---

## Phase 6: Analyze

### Analyze Prompt

```
/speckit.analyze

Focus on:
1. Constitution alignment — Qt6 idioms, QSO entry latency (III), advisory-
   not-authoritative decoder output (I), no third-party deps (II), no
   feature creep beyond scope (V)
2. Coverage gaps — all 8 user stories have tasks; SO2R clarify decision is
   reflected in task file paths and wiring; cross-platform checklist gaps
   addressed in tasks (not just docs)
3. File path consistency — verify src/audio/, include/audio/ task paths
   match plan.md; confirm cwDecoderWidget.cpp lives in src/ui/
4. Dependency ordering — audio capture scaffold MUST land before decoder
   integration; decoder DSP MUST be unit-testable without audio hardware
   before integration tasks
5. Qt6::Multimedia linkage is added in Phase 1 before any audio code is
   written
6. High-risk items covered: token parsing accuracy, SO2R routing, audio
   device reconnection
```

### Analysis Results

| ID | Severity | Issue | Resolution |
|----|----------|-------|------------|
| | | | (fill after running) |

---

## Phase 7: Implement

### Implement Prompt

```
/speckit.implement

## Approach: DSP-first, Integration-second

For each task:
1. Write the code
2. Build with `make` — must succeed with zero warnings on GCC (constitution)
3. For DSP logic (Goertzel, dot-length estimator, Morse table), write a unit
   test in tests/ first against synthetic input — no audio device required
4. For audio path, manual verification on Linux (PipeWire loopback) feeding
   known CW from a web Morse sender or pre-recorded WAV
5. Commit style: concise imperative subject, no Co-Authored-By trailer
   (per constitution and user feedback memory)

## Pre-Implementation Setup

1. Create branch: `git checkout -b 004-cw-decoder` (or let /speckit.specify
   create it)
2. Verify baseline: `make && make test` — must pass before any changes
3. Install Qt6::Multimedia dev package on Linux
   (`sudo apt install libqt6multimedia6`) if not already present
4. Prepare a CW audio source — fldigi's built-in CW sender, websdr.org, or
   a pre-recorded WAV of contest CW

## Implementation Notes

- The audio callback MUST NOT block — always post to a ring buffer and let
  the worker pull
- Morse table is a compile-time std::unordered_map or constexpr table —
  tiny, no need for a file
- WPM auto-detect uses a rolling window of recent dot/dash lengths (median
  filter is more robust than mean under noise)
- Tone pin: store the acquired tone Hz; when pinned, do not re-acquire;
  when unpinned, re-acquire every N seconds or when SNR drops
- Click-to-fill: use QTextEdit's anchorClicked signal (set
  setTextInteractionFlags appropriately) or implement hit-testing in a
  custom widget
- SO2R routing: query m_activeRadio at click time; route to
  m_entryWidgets.callEdit / rstrEdit or m_entryWidgetsR.callEdit / rstrEdit
  (see mainwindow.cpp patterns from SO2R implementation)
- HiDPI: stick to standard widgets; no manual pixel math
```

### Implementation Progress

| Phase | Tasks | Completed | Notes |
|-------|-------|-----------|-------|
| 1 - Foundation / Rig Settings | | | Qt6::Multimedia linkage + per-radio audio device setting |
| 2 - Decoder DSP | | | Goertzel + Morse table; unit-tested |
| 3 - US1: Radio-bound capture | | | Widget spawned per radio with audio configured |
| 4 - US3: Adaptive WPM | | | Rolling-median dot length, live display |
| 5 - US4: Tone acquisition | | | |
| 6 - US7: Squelch | | | |
| 7 - US5: CALL click → owning radio | | | |
| 8 - US6: RST click → owning radio | | | |
| 9 - US8: SO2R combinations | | | All four (neither/L/R/both) verified |
| 10 - Polish | | | Settings, menu, docs |

---

## Post-Implementation Checklist

- [ ] All tasks marked complete in tasks.md
- [ ] `make` succeeds with zero warnings on GCC
- [ ] `make test` passes, including new DSP unit tests
- [ ] Decoder correctly reads CW from a synthetic test signal at 20/30/40 WPM
- [ ] Rig Connection Settings dialog has an Audio Input Device combo for each radio tab; "(none)" is a valid selection
- [ ] In SO2R with audio configured only on Radio L, the decoder widget appears only for Radio L; no widget or dead UI for Radio R
- [ ] Click CALL token on Radio L's decoder fills Radio L's CALL field (even if Radio R is the active radio)
- [ ] Click RST token on Radio R's decoder fills Radio R's RSTr field (even if Radio L is the active radio)
- [ ] No crash when no audio device is available
- [ ] QSO entry keyboard latency unaffected (manual smoke test: type during
      active decode)
- [ ] Widget state persists across restart
- [ ] CI green on Linux, macOS, Windows
- [ ] CHANGELOG.md updated under a "Other Changes and Bugfixes" subsection
      (per user's changelog-format feedback)
- [ ] Update `docs/ai/specs/contestlogx-master-plan.md` — mark SPEC-005 ✅ Complete

---

## Lessons Learned

### What Worked Well

-

### Challenges Encountered

-

### Patterns to Reuse

-

---

## Project Structure Reference

```
ContestLogX/
├── src/
│   ├── ui/               # UI widgets (mainWindow, cwDecoderWidget, ...)
│   ├── core/             # Non-UI logic
│   ├── database/         # DxccDatabase
│   ├── audio/            # NEW: cwDecoder, audioCapture, cwDecoderWorker
│   ├── rig/              # flrig / hamlib / mocked backends
│   └── contestEngine.cpp # Core scoring engine (NOT in src/engine/)
├── include/
│   ├── audio/            # NEW: decoder + capture headers
│   └── *.h               # All other headers flat
├── contests/             # Contest JSON definitions
├── tests/                # Unit tests
├── scripts/              # Test runners
├── test_logs/            # Automated test data
├── docs/
│   └── ai/specs/         # SpecKit spec artifacts
└── .specify/             # SpecKit config and templates
```
