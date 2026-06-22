# Implementation Plan: CW Decoder

**Branch**: `004-cw-decoder` | **Date**: 2026-04-21 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/004-cw-decoder/spec.md`

## Summary

Add a dockable multi-channel CW decoder, one per radio that has an audio input device configured. Each decoder captures audio from the configured system input device via `Qt6::Multimedia` (`QAudioSource` + `QMediaDevices`) and runs N parallel single-frequency Goertzel detectors across a configurable passband (default 6 bins at 100 Hz spacing from 400-1000 Hz). Each bin independently drives its own dot/dash classifier, rolling-median WPM estimator, and Morse decoder, emitting decoded characters per bin. The widget renders N horizontal scrolling rows, one per bin, each labeled with its center frequency. Clickable callsign and RST tokens within any row route to the **owning radio's** QSO entry via the same keyboard-entry handler (so SCP / call-history / dupe-check / name-QTH auto-fill all fire identically). Two independent TX-mute paths suppress self-decode: the owning radio's rig-backend PTT state, and explicit signalling from `MainWindow` when it initiates a CW send via F-key memory or CW console. Both paths respect a per-radio "Mute decoder on PTT" setting (default ON) stored in Rig Connection Settings alongside the audio device selector.

## Technical Context

**Language/Version**: C++17
**Primary Dependencies**: Qt6 (Core, Widgets, Network, Xml) + **NEW: `Qt6::Multimedia`** - a standard Qt6 module, not a third-party library
**Storage**: In-memory ring buffer for audio blocks (lock-free SPSC or QMutex-guarded); per-bin `QString` buffers capped at ~10,000 chars (rolling); `QSettings` for decoder runtime state (passband/bin count/spotlight/squelch/WPM range); rig config for audio device name, "Mute on PTT" flag, PTT grace-window ms
**Testing**: `make test` runs unit tests for Goertzel coefficients on synthetic sine input, Morse-table lookup, rolling-median dot-length estimator, and token regex (callsign + RST patterns). Audio path verified manually with virtual audio cable feeding known CW. `make test-logs` is NOT required (no contest engine changes)
**Target Platform**: Linux (PipeWire/PulseAudio), macOS (CoreAudio), Windows (WASAPI) - all via `Qt6::Multimedia` abstraction
**Project Type**: desktop-app feature within existing Qt6 application
**Performance Goals**: End-to-end audio-in → on-screen char ≤ 200 ms at 25 WPM (SC-004); ≥ 95% character accuracy on clean signal (SC-001); WPM convergence within ±2 WPM in 10 chars (SC-002); per-bin decode accuracy preserved under speed change of ±10 WPM within 5 chars (SC-003); CPU < 10% of one core with 6 default bins and active audio (SC-009); QSO-entry keystroke latency indistinguishable from decoder-off baseline (SC-005)
**Constraints**: No new third-party libraries (only `Qt6::Multimedia`, a new Qt6 module - approved at this gate per Constitution Check below). Audio capture MUST NOT block the main thread. Click-to-fill MUST NOT steal keyboard focus (Principle III). Decoder output MUST NOT auto-log QSOs (Principle I). Cross-platform code only - no `Q_OS_*` branches except where Qt6::Multimedia device enumeration has documented platform-specific behavior. `make` MUST succeed with zero GCC warnings
**Scale/Scope**: Up to 2 radios concurrently × up to ~16 bins per radio (default 6) = up to 32 parallel Goertzel detectors at 8 kHz × 10 ms blocks. DSP cost is linear in bin count; design targets 6 bins default with headroom to 16

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Requirement | Status |
|-----------|-------------|--------|
| I. Contest Accuracy (NON-NEGOTIABLE) | Decoder is advisory / copy-assist only. FR-028 forbids auto-logging. Click-to-fill routes through the existing keyboard-entry handler - no new scoring, dupe, or exchange-validation paths. `make test-logs` is NOT required because ContestEngine is untouched. | ✅ Pass |
| II. Qt6-Native Architecture | All new code uses Qt6 idioms: `QDockWidget`, `QAudioSource`, `QMediaDevices`, `QThread`, signals/slots, `QSettings`. Adds one new Qt6 module (`Qt6::Multimedia`) - extending the constitution's listed module set by one Qt6 module. **Gate approval:** per prior user decision (option b), we approve the Multimedia module addition here instead of bumping the constitution to v1.1.0 - no third-party dependency is introduced, and the module is standard Qt6. Cross-platform via Qt6's abstraction layer; `#ifdef Q_OS_*` used only where documented platform behavior requires it. | ✅ Pass (with explicit module-scope approval) |
| III. Keyboard-First Operator Experience | Click-to-fill is a mouse enhancement that routes through the same keyboard-entry handler as typed input (Clarify #3), so SCP/call-history/dupe/auto-fill all fire identically. Click does not steal focus (FR-022). QSO-entry keystroke latency must be unaffected by the decoder (SC-005). Decoder widget must not intercept keyboard events destined for QSO entry. | ✅ Pass - verify in implementation via SC-005 latency test |
| IV. JSON-Driven Contests | No contest JSON changes. Decoder behavior is contest-agnostic. | ✅ Pass |
| V. Simplicity and YAGNI | Multi-channel is justified by direct operator value (skimmer-style view of the whole passband). Still Goertzel-only - no full FFT, no waterfall, no panadapter, no cross-bin fusion. Per-bin state is minimal (Goertzel coeffs, rolling median buffer, Morse state machine, text buffer). Bin count bounded to sensible range. Single global squelch per session. No multi-signal separation within a bin. Rejected alternatives (D from Clarify #1: full skimmer with auto-harvesting / RBN output) deferred out of scope. | ✅ Pass |

**Post-design re-check** (after Phase 1): ✅ All gates still pass. No Complexity Tracking entries required.

## Project Structure

### Documentation (this feature)

```text
specs/004-cw-decoder/
├── plan.md                 # This file
├── research.md             # Phase 0 output
├── data-model.md           # Phase 1 output
├── quickstart.md           # Phase 1 output
├── contracts/
│   └── signals.md          # Phase 1 output - Qt signal interfaces
├── checklists/
│   └── requirements.md     # Already created by /speckit.specify
└── tasks.md                # Phase 2 output (/speckit.tasks - NOT created here)
```

### Source Code (repository root)

```text
src/audio/                  # NEW directory - DSP + audio capture
├── binChannel.cpp          # NEW - single-frequency Goertzel + dot/dash + WPM + Morse (one bin)
├── cwDecoder.cpp           # NEW - orchestrates an array of BinChannels per AudioCapture
├── audioCapture.cpp        # NEW - QAudioSource wrapper; emits audio blocks to ring buffer
└── cwDecoderWorker.cpp     # NEW - QObject on a QThread; owns CwDecoder; consumes ring buffer

include/audio/              # NEW directory - audio subsystem headers
├── binChannel.h
├── cwDecoder.h
├── audioCapture.h
├── cwDecoderWorker.h
└── audioTypes.h            # shared enums, AudioBlock struct, MuteState

src/ui/
├── cwDecoderWidget.cpp     # NEW - QDockWidget with N scrolling bin rows + controls
├── rigControlDialog.cpp    # MODIFY - add Audio Input Device combo + Mute-on-PTT + grace-ms per radio tab
└── mainWindow.cpp          # MODIFY - spawn decoder widgets per configured radio; route click signals; signal internal CW sends; wire rig-backend PTT

include/
├── cwDecoderWidget.h       # NEW - flat include/ per project convention
├── rigControlDialog.h      # MODIFY - new combo/checkbox/spin members
└── mainWindow.h            # MODIFY - m_cwDecoderLeft, m_cwDecoderRight; internal-send signal wiring

src/utils/settings.cpp      # MODIFY - add rig.{left,right}.audioInputDevice / .muteDecoderOnPtt / .decoderPttGraceMs; audio/cwDecoder/{left,right}/* runtime keys
include/settings.h          # MODIFY - settings accessors

CMakeLists.txt              # MODIFY - add `find_package(Qt6 REQUIRED COMPONENTS Multimedia)` and link Qt6::Multimedia

src/rig/flrigClient.cpp     # MODIFY (if needed) - ensure pttStateChanged(bool) signal is emitted; already present in many backends but verify
src/rig/hamlibClient.cpp    # MODIFY (if needed) - same
src/rig/mockedRigClient.cpp # MODIFY (if needed) - emit constant pttStateChanged(false) so decoder fallback path is exercised
include/rigInterface.h      # MODIFY - add `void pttStateChanged(bool active)` signal to base interface if not present

tests/
├── test_goertzel.cpp       # NEW - synthetic sine input → expected magnitude
├── test_binChannel.cpp     # NEW - dot/dash classifier, rolling-median WPM estimator, Morse-table lookup
└── test_cwDecoder.cpp      # NEW - multi-bin integration test with synthetic audio blocks
```

**Structure Decision**: Single-project (existing ContestLogX layout). Adds one new `src/audio/` source subdirectory and a matching `include/audio/` header subdirectory. This is the first header subdirectory in the project; justified because the audio DSP subsystem has distinct internals (Goertzel, Morse table, ring buffer, worker thread) that warrant grouping and would otherwise clutter the flat `include/` namespace. All other new files sit in the existing `src/ui/` / `include/` pattern. Follows CLAUDE.md project-structure note.

## Complexity Tracking

> No Constitution Check violations. No justified complexity entries required.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| (none) | - | - |
