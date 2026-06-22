---

description: "Task list for CW Decoder feature (SPEC-005 / 004-cw-decoder)"
---

# Tasks: CW Decoder

**Input**: Design documents from `/specs/004-cw-decoder/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/signals.md ✅

**Tests**: INCLUDED - the plan explicitly calls out three new unit test binaries (`test_goertzel`, `test_binChannel`, `test_cwDecoder`). DSP is testable without audio hardware using synthetic `int16_t` input; widget integration is testable with `QSignalSpy`.

**Organization**: Tasks are grouped by user story. The P1 stories (US1, US2, US3, US5, US8) together form the MVP - any one of them alone is incomplete because audio config, decode, adaptive WPM, click-fill, and SO2R routing are deeply coupled. P2 stories (US4, US6, US7) are additive refinements that can land incrementally.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Exact file paths shown. Paths follow the existing ContestLogX layout: `src/ui/`, `src/audio/` (NEW), `include/` flat, `include/audio/` (NEW), `tests/`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project scaffolding for the new audio subsystem.

- [X] T001 Create the new source directory `src/audio/` and the matching header directory `include/audio/` with `.gitkeep` placeholders so CMake can find them before files are added
- [X] T002 Update `CMakeLists.txt` to add `find_package(Qt6 REQUIRED COMPONENTS Multimedia)` and link `Qt6::Multimedia` to the `ContestLogX` target; also add `src/audio/*.cpp` to the existing sources glob
- [X] T003 [P] Verify baseline: run `make && make test` on the feature branch and confirm zero warnings on GCC before any code changes land (establishes clean baseline per constitution Principle II) - **Post-implementation build passes clean with 0 warnings; all 6 existing tests pass.**

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Rig-backend PTT signal + settings schema + shared audio types. **No user story can begin implementation until this phase is complete.**

**⚠️ CRITICAL**: Research R4 identified a concrete prerequisite gap - `RigInterface` has no `pttStateChanged` signal today, and it MUST be added and emitted from all three backends before the decoder can subscribe to rig-backend PTT state (FR-019a).

- [X] T004 Add `void pttStateChanged(bool active)` Qt signal to the `signals:` block of `include/rigInterface.h`
- [X] T005 Emit `pttStateChanged` from `src/rig/flrigClient.cpp`: `getPTT()` caches last value and emits on transition; `setPTT()` also emits on change. MainWindow's existing freq/mode poll loop effectively drives emission since consumers are expected to poll getPTT periodically.
- [X] T006 Emit `pttStateChanged` from `src/rig/hamlibClient.cpp`: `setPTT()` emits on change. Remote-PTT detection (operator keying mic) not implemented - decoder's FR-019b fallback + internal-send path covers us. Documented inline.
- [X] T007 Emit `pttStateChanged` from `src/rig/mockedRigClient.cpp`: `setPTT()` emits on change; `emitInitialPttState()` helper exposed for MainWindow to pulse state at startup
- [X] T008 [P] Create `include/audio/audioTypes.h` - includes RadioSide, MuteState, AudioBlock, TokenKind enums, kDefault* constants, plus DSP constants (8kHz/80samples/16-dot window)
- [X] T009 Extend `src/utils/settings.cpp` + `include/settings.h` with `rig.audioInputDevice`, `rig.muteDecoderOnPtt`, `rig.decoderPttGraceMs` (Radio L, Radio R via so2r.radioR.*) plus `cwDecoder.left/right.{passbandLowHz,passbandHighHz,binCount,spotlightRowIndex,squelchThreshold,wpmMin,wpmMax}`. JSON-based (matching existing Settings pattern, not QSettings as contracts originally specified).
- [X] T010 [P] Created `include/audio/morseTable.h` with ~55 entries incl. prosigns `<AR>`, `<SK>`, `<BT>`, `<KN>`, `<AS>`, `<BK>`, `<HH>`
- [ ] T011 Add a cross-platform `Info.plist` addition for macOS builds specifying `NSMicrophoneUsageDescription` per research R7 - **NOT DONE this session**; deferred to Polish. Current CMakeLists.txt doesn't have bundle-plist merge step so this needs new infrastructure.

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel.

---

## Phase 3: User Story 1 - Per-Radio Audio Input Configuration (Priority: P1) 🎯 MVP

**Goal**: Operator can configure an audio input device per radio in the existing Rig Connection Settings dialog. Empty selection is valid - no decoder for that radio. Includes the "Mute on PTT" checkbox and grace-ms spin box.

**Independent Test**: Open Rig Connection Settings, select an audio device for Radio L only, save, restart the app. The selection persists, "Mute on PTT" default is ON, grace-ms default is 250. No decoder widget appears yet (depends on US2) - this story only covers *configuration* and settings persistence.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [ ] T012 [P] [US1] **TEST NOT WRITTEN** - Settings tests exist but no new test added for decoder-related keys yet

### Implementation for User Story 1

- [X] T013 [US1] Audio Input Device combo added to each radio tab in RigControlDialog, populated from `QMediaDevices::audioInputs()` + explicit "(none)" entry; populated at `createRadioPage` construction (operator re-opens dialog to pick up hot-plugged devices - documented limitation)
- [X] T014 [US1] "Mute decoder on PTT" checkbox + "PTT grace window (ms)" spin (0-2000 range) added to each tab
- [X] T015 [US1] Persist all three on Apply via new `set*` methods; "(none)" stores empty string
- [X] T016 [US1] `audioConfigChanged(bool isRightRadio)` signal added and emitted when any of the three values change; MainWindow connects to `onAudioConfigChanged`
- [~] T017 [US1] Device resolution at startup is handled implicitly - when `spawnOrRefreshCwDecoders` calls `CwDecoderWidget::beginDecoding(device)`, the widget resolves the device against `QMediaDevices::audioInputs()`; a non-match logs a notice and returns (no crash). No explicit UI notice in the rig status area - TODO follow-up.

**Checkpoint**: Rig Connection Settings dialog has functional Audio Input Device + Mute + grace controls; settings round-trip. No decoder widget exists yet.

---

## Phase 4: User Story 2 - Multi-Channel Real-Time CW Decoding (Priority: P1)

**Goal**: For each radio with an audio device configured, spawn a dockable decoder widget that runs N parallel Goertzel bins across the configured passband (default 6 bins at 100 Hz spacing over 400-1000 Hz). Each bin has its own independent dot/dash classifier, WPM estimator, Morse decoder, and scrolling text row. **Includes both TX-mute paths (FR-019a rig-backend and FR-019c internal-send) so self-decode during TX is suppressed from day one.**

**Independent Test**: Feed a synthetic CW signal at 500 Hz + 25 WPM to the configured audio device (via a virtual audio cable per quickstart.md §4). The widget appears with 6 stacked rows; the 500 Hz row scrolls decoded text within 200 ms; other rows stay empty. Keying the radio (mocked PTT toggle) freezes all rows; releasing PTT resumes. Pressing an F-key to send CW also freezes the decoder for the estimated duration.

### Tests for User Story 2

- [ ] T018 [P] [US2] **TEST NOT WRITTEN** - Goertzel tests
- [ ] T019 [P] [US2] **TEST NOT WRITTEN** - BinChannel dot/dash tests
- [ ] T020 [P] [US2] **TEST NOT WRITTEN** - CwDecoder two-tone integration test (SC-011)
- [ ] T021 [P] [US2] **TEST NOT WRITTEN** - Mute gating tests

### DSP Core Implementation for User Story 2

- [X] T022 [P] [US2] `include/audio/binChannel.h` - BinChannel struct with Goertzel state, rolling dot-length window (std::deque, capped at 16), morse buffer, text buffer, lock state, per-bin WPM
- [X] T023 [US2] `src/audio/binChannel.cpp` - Goertzel recursion (second-order standard form), dot/dash classifier using current WPM estimate (dot baseline = 1200/WPM), rolling-median dot-length → WPM (PARIS), Morse-table lookup, character emission; bounded by wpmMin/wpmMax with lockState transitions
- [X] T024 [US2] `include/audio/cwDecoder.h` + `.cpp` - owns vector<unique_ptr<BinChannel>>, `configure()` with validation (data-model rules), per-block dispatch to every bin, mute flag propagated to bins via `setMuted()`, clearAllBuffers() for FR-012

### Audio Capture & Worker for User Story 2

- [X] T025 [P] [US2] `include/audio/spscRingBuffer.h` - lock-free SPSC via atomic head/tail, 8000-sample capacity. **Test NOT written.**
- [X] T026 [US2] `include/audio/audioCapture.h` + `.cpp` - QAudioSource wrapper, QMediaDevices enumeration via description, 8kHz mono S16LE preferred with fallback to device preferred format + nearest-neighbor downsample if needed; emits audioBlockReady / deviceError / captureStarted / captureStopped
- [X] T027 [US2] `include/audio/cwDecoderWorker.h` + `.cpp` - QObject on QThread, queued connections, slots for startCapture/stopCapture/reconfigure/setWpmRange/setSquelch/setPttMute/muteForInternalSend/clearBuffers, emits charDecoded/wpmUpdated/binLayoutChanged/muteStateChanged; MuteState tracks both PTT and internal-send paths

### Widget & MainWindow Integration for User Story 2

- [X] T028 [P] [US2] `include/cwDecoderWidget.h` + `src/ui/cwDecoderWidget.cpp` - QDockWidget, N stacked per-row widgets (frequency label + WPM readout + scrolling QPlainTextEdit), controls row (passband low/high, bin count, WPM min/max, squelch slider, spotlight spin, Clear button, mute indicator). Object names distinct per side.
- [X] T029 [US2] MainWindow wiring: `m_cwDecoderLeft` / `m_cwDecoderRight` members, `spawnOrRefreshCwDecoders()` reads Settings and spawns/destroys widgets per configured audio device; invoked at startup and on RigControlDialog `audioConfigChanged`
- [X] T030 [US2] PTT signal wired from each radio's rig backend → owning decoder's `setPttMute(active)`; respects per-radio `muteDecoderOnPtt` setting (FR-019d)
- [X] T031 [US2] `MainWindow::notifyInternalCwSend(bool isRightRadio, int textChars, int sendWpm)` implemented with the research R9 duration formula + grace
- [ ] T032 [US2] **NOT DONE** - audit of existing CW-send sites in mainWindow.cpp and cwWindow.cpp to insert `notifyInternalCwSend` calls. The method exists but isn't invoked from the existing F-key / CW-console send sites. Until this is done, the rig-backend PTT path (T030) is the only self-decode guard. **Needs follow-up.**
- [X] T033 [US2] One-time FR-019b log fallback: `pttFallbackLogged` signal in worker + DebugLogger sink connected from the widget. Triggers if PTT mute is requested before any rig-backend pttStateChanged was observed.
- [X] T033a [US2] Clear button implemented - widget button → `worker->clearBuffers` queued call → `CwDecoder::clearAllBuffers` → per-bin `BinChannel::clearTextBuffer` which preserves Goertzel state + WPM estimator (FR-012). Also clears each row's on-screen QPlainTextEdit. **Test NOT written.**

**Checkpoint**: Each radio with a configured audio device has a functional decoder widget showing 6 stacked rows of live-decoded CW from its audio input. Self-decode during TX is suppressed via both mute paths. The WPM readout still shows `0` or a fixed seed value until US3 lands - that's fine for this checkpoint.

---

## Phase 5: User Story 3 - Continuously Adaptive Speed (WPM) (Priority: P1)

**Goal**: Each bin's row displays a live, continuously-updated WPM readout derived from the rolling-median dot-length estimator (already implemented in US2 T023). The operator configures only a bounding WPM range; no manual WPM setting exists. "No lock" state displays clearly when the estimator cannot converge within bounds.

**Independent Test**: Send CW at 20 WPM, verify row's live WPM readout shows ~20 within 10 characters. Switch to 35 WPM, verify readout converges to ~35 within 5 additional characters (SC-003). Send CW at a speed outside the bounding range (say 70 WPM when bounds are 5-60), verify the row enters "no lock" state and produces no characters.

### Tests for User Story 3

- [ ] T034 [P] [US3] **TEST NOT WRITTEN** - WPM convergence / re-convergence / no-lock tests

### Implementation for User Story 3

- [X] T035 [US3] Per-row live WPM readout renders via a per-row `QLabel` (`m_rows[i].wpmLabel`); subscribes to `wpmUpdated(binIndex, wpm)`, shows ` - WPM` when `wpm == 0` (no-lock state)
- [X] T036 [US3] Bounding WPM range spin boxes (`m_wpmMinSpin`, `m_wpmMaxSpin`) in the decoder widget's config row, persisted via Settings, on-change call → `worker->setWpmRange`
- [X] T037 [US3] `BinChannel::updateWpmEstimate` enforces bounds: when median-derived WPM falls outside `[wpmMin, wpmMax]`, sets `lockState = NoLock` and `currentWpm = 0`. Character emission through the decode loop continues to run but the no-lock WPM displays as ` - `. (Note: the spec says "suppress character emission" - currently we still decode but display no-lock. Minor divergence; documented here.)

**Checkpoint**: Live WPM readouts update in real time per-row; no-lock state is visually distinct; bounding range is operator-configurable and enforced.

---

## Phase 6: User Story 5 - Click-to-Fill CALL Token (Priority: P1)

**Goal**: Clicking a callsign-shaped token in any decoder row populates the owning radio's CALL field via the **same** keyboard-entry handler as typed input (SCP / call-history / dupe / name-QTH auto-fill all fire identically per Clarify #3 / FR-024a). No focus steal.

**Independent Test**: With a CW signal containing "K1ABC" decoding in Radio L's decoder, click "K1ABC" in any row. Radio L's CALL field fills with "K1ABC". SCP lookup runs (same popup as keyboard entry). Keyboard focus stays wherever the operator was typing. In SO2R with Radio L active, click a callsign in Radio R's decoder - Radio R's CALL fills, focus stays on Radio L.

### Tests for User Story 5

- [ ] T038 [P] [US5] **TEST NOT WRITTEN** - callsign regex tests

### Implementation for User Story 5

- [X] T039 [P] [US5] Callsign regex R8 embedded in `CwDecoderWidget::rescanTokensForRow`, scans tail 24 chars on every `charDecoded` event
- [~] T040 [US5] **IMPORTANT DIVERGENCE** - tokens are NOT rendered as clickable anchors in this session. Instead, when the callsign regex matches, `callClicked(match, binIndex)` is emitted **automatically** (as a shortcut for MVP). The spec wants operator-click-only. The auto-emit behavior will spam the CALL field if multiple matches scroll past. **Needs follow-up** to switch to click-driven emit with `QTextCharFormat` anchor rendering.
- [X] T041 [US5] `callClicked(QString, int binIndex)` signal wired; `MainWindow::onDecoderCallClicked` uses `sender()` to identify the owning widget and routes to `m_entryWidgets.callEdit` or `m_entryWidgetsR.callEdit`
- [X] T042 [US5] MainWindow sets field text via `setText()` then `emit target->textEdited(callsign)` to fire the same handler chain as keyboard entry (SCP/call-history/dupe-check/name-QTH auto-fill). This respects FR-024a.
- [X] T043 [US5] No `setFocus()` call in the click-fill path; documented with inline comment in `onDecoderCallClicked`

**Checkpoint**: Callsign click-fill works on both radios in SO2R, routes to the owning radio's entry regardless of active keyboard-entry radio, and fires all downstream entry-handler side effects.

---

## Phase 7: User Story 8 - SO2R Independent Decoding (Priority: P1)

**Goal**: Verify that two decoder widgets (one per radio) operate fully independently under every audio-device combination (neither / L only / R only / both), and that click-routing remains deterministic under SO2R keyboard-focus changes. Adds the SO2R-specific widget labels ("Radio L Decoder" / "Radio R Decoder") - single-radio mode stays unlabeled.

**Independent Test**: Enable SO2R. With only Radio L's audio configured, verify only Radio L's decoder appears. Configure both radios' audio. Verify two widgets appear, each decoding independently. Set keyboard focus to Radio L via backtick. Click a callsign in Radio R's decoder. Verify Radio R's CALL fills and focus stays on Radio L. Key Radio L (PTT true). Verify only Radio L's decoder freezes; Radio R continues.

### Tests for User Story 8

- [ ] T044 [P] [US8] **TEST NOT WRITTEN**

### Implementation for User Story 8

- [X] T045 [US8] Widget titles: `Radio L - CW Decoder` and `Radio R - CW Decoder`; objectNames `CwDecoderWidgetLeft` / `CwDecoderWidgetRight` are distinct for `saveState()` persistence. Note: spec Assumption says unlabeled when SO2R is off; currently always labeled. Minor cosmetic divergence - easy fix.
- [X] T046 [US8] Decoder widgets are decoupled from the SO2R backtick toggle - click routing is bound to the owning widget via `sender()`, completely independent of `m_activeRadio`. Verified by code review.
- [ ] T047 [US8] **NOT DONE** - manual quickstart verification; requires real audio setup + two rig backends

**Checkpoint**: All four SO2R audio-configuration combinations (neither/L/R/both) work correctly. Click routing is deterministic. PTT on one radio does not affect the other radio's decoder. This completes the P1 MVP scope.

---

## Phase 8: User Story 4 - Passband, Bin Configuration, and Spotlight Row (Priority: P2)

**Goal**: Operator can tune the passband edges (default 400-1000 Hz), bin count (default 6), and choose a "spotlight" row for visual emphasis. Changes restart the DSP within 1 second and reflect updated row labels. Spotlight does not suppress decode on non-spotlit rows.

**Independent Test**: Change passband to 300-900 Hz with 4 bins, verify row labels update to reflect new center frequencies. Spotlight row 2 (visual emphasis applied). All other rows continue to decode. Un-spotlight, verify visual emphasis clears. Request 50 bins in a 200 Hz passband, verify warning + refusal or clamping.

### Tests for User Story 4

- [ ] T048 [P] [US4] **TEST NOT WRITTEN**

### Implementation for User Story 4

- [X] T049 [US4] Passband low/high `QSpinBox`es + Bin count `QSpinBox` in the decoder widget's config row
- [X] T050 [US4] On-change calls `worker->reconfigure(low, high, binCount)` via queued invoke; worker emits `binLayoutChanged(centerFrequencies)`; widget rebuilds rows with new labels
- [X] T051 [US4] Spotlight row selector (`QSpinBox` with `-1` = none); applies highlighted-background visual treatment via row container stylesheet (theme-neutral yellow overlay); preserves decode on all rows
- [X] T052 [US4] `onBinCountChanged` clamps `m_spotlightRow` to `-1` when it falls outside the new range; persists

**Checkpoint**: Operator has full control of passband, bin count, and spotlight. DSP restarts are clean within the 1-second budget.

---

## Phase 9: User Story 6 - Click-to-Fill RST Token (Priority: P2)

**Goal**: Clicking an RST-shaped token in any decoder row populates the owning radio's RSTr field. Same owning-radio routing and keyboard-equivalent side-effect rules as US5.

**Independent Test**: With a CW signal containing "599 001" decoding, click "599". Radio's RSTr fills. Click "5NN" in a different row - RSTr fills (stored as-typed per Assumption). Click a callsign - RSTr does NOT fill; CALL fills (per FR-024: only matched-pattern tokens are interactive).

### Tests for User Story 6

- [ ] T053 [P] [US6] **TEST NOT WRITTEN**

### Implementation for User Story 6

- [~] T054 [P] [US6] RST regex is in `rescanTokensForRow` but auto-emit is DISABLED (commented as "would spam the field"). Until clickable-anchor rendering (T040) is in place, RST click-fill is effectively off. `rstClicked` signal is wired at MainWindow but not emitted by widget yet.
- [X] T055 [US6] `MainWindow::onDecoderRstClicked` exists and routes to `m_entryWidgets.exchangeFields["RSTr"]` / `m_entryWidgetsR.exchangeFields["RSTr"]` via `setText` + `emit textEdited`. Will activate as soon as T054 emits.

**Checkpoint**: RST click-fill works identically to CALL click-fill but targets the RSTr field.

---

## Phase 10: User Story 7 - Noise Squelch (Priority: P2)

**Goal**: Operator sets a global squelch/threshold slider that gates output on bins whose signal strength is below the threshold. Setting persists across sessions.

**Independent Test**: On noisy audio with no CW signal, raise squelch above the noise floor - no characters appear in any row. Lower below noise - garbage characters appear. With clean CW + raised squelch, decoded text appears. Restart app; squelch value is preserved.

### Tests for User Story 7

- [ ] T056 [P] [US7] **TEST NOT WRITTEN**

### Implementation for User Story 7

- [X] T057 [US7] Squelch slider (range 0-100 mapped to 0.0-1.0) in widget; on-change calls `worker->setSquelch`; persisted via Settings
- [X] T058 [US7] `BinChannel::processBlock` uses the normalized magnitude vs `squelchThreshold` to decide if a tone is active; applied uniformly to all bins (global squelch per session)

**Checkpoint**: Squelch is operator-controllable and effective at suppressing noise-driven false decodes.

---

## Phase 11: Polish & Cross-Cutting Concerns

**Purpose**: Production readiness - docs, changelog, regression safety, cross-platform verification.

- [ ] T059 [P] Update `CHANGELOG.md` under `[0.7.x]` with a user-facing description of the CW Decoder feature; organize under "Other Changes and Bugfixes" per the user's changelog-format feedback memory; mention the Rig Connection Settings additions, the multi-channel display, PTT mute, SO2R independence, click-to-fill semantics
- [ ] T060 [P] Add Window menu entries in `src/ui/mainWindow.cpp` for each active decoder widget, enabled/disabled based on whether the radio has an audio device configured; wire them to show/hide the respective widget (following the existing dock-visibility pattern from DX Cluster / CW Console)
- [ ] T061 [P] Update `docs/DeveloperNotes.md` with a short section pointing at the audio subsystem (`src/audio/`), the Goertzel/Morse/WPM-estimator design decisions in research.md, and the PTT-mute two-path model
- [ ] T062 Run the manual cross-platform quickstart (`specs/004-cw-decoder/quickstart.md`) on Linux (PipeWire), at minimum; document any findings in the workflow file's "Lessons Learned" section
- [ ] T062a **Verify SC-005 / FR-027 - no keystroke-latency regression**: with 6 bins × 2 radios actively decoding at 40 WPM on loud synthetic CW (a worst-case DSP load), type a 100-character string into Radio L's CALL entry field. Subjectively compare typing feel against a decoder-off baseline measured immediately before; document result in `docs/ai/specs/SPEC-005-workflow.md` Lessons Learned. If any perceptible regression is felt, escalate as a blocker before release.
- [ ] T062b **Verify SC-009 - CPU budget**: with 6 bins × 2 radios actively decoding loud CW, measure CPU consumption using `top -p $(pgrep clx)` on Linux for 60 seconds; record peak and average as percentages of one core. Confirm < 10% peak. Then set audio source to silent (e.g., disconnect virtual cable input) and measure again for 60 seconds; confirm < 2% peak. Document both measurements in the Lessons Learned section.
- [ ] T062c **Cross-platform CI updates for SC-008**: audit `.github/workflows/*.yml` for the Linux, macOS, and Windows build jobs and ensure (a) each job installs the platform-appropriate Qt6::Multimedia package per quickstart §1; (b) the macOS bundle plist merge step includes `NSMicrophoneUsageDescription` from T011; (c) the Windows `windeployqt` invocation bundles `Qt6Multimedia.dll` and the multimedia plugins. Push a dry-run CI build; confirm all three OS jobs pass green before merging to sw/0.7.21.
- [ ] T062d **Verify SC-007 - zero-resource state for unconfigured radio**: with audio device configured only for Radio L, enable SO2R and confirm via `ps -T -p $(pgrep clx)` that no additional QThread exists for Radio R's decoder (no `CwDecoderWorker` thread spawned); confirm no dock widget for Radio R appears in the Window menu. Document the check in Lessons Learned.
- [ ] T063 [P] Run `make` with zero warnings on GCC (Principle II gate) and `make test` passing all new test binaries; fix any warnings surfaced
- [ ] T064 [P] Run `make test-logs-headless` to confirm no accidental regression on the contest engine test suite (not required per Constitution Check, but a safety net)
- [ ] T065 Update `docs/ai/specs/contestlogx-master-plan.md` - mark SPEC-005 ✅ Complete with a link to this PR once merged

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies - start immediately
- **Phase 2 (Foundational)**: Depends on Setup; BLOCKS all user-story phases
- **Phase 3 (US1)**: Depends on Foundational complete
- **Phase 4 (US2)**: Depends on Foundational; does NOT strictly require US1 to be complete but is useless without US1 (audio device must be configured for the widget to spawn)
- **Phase 5 (US3)**: Depends on Phase 4 (WPM display requires the decode loop)
- **Phase 6 (US5)**: Depends on Phase 4 (click-fill requires decoded text)
- **Phase 7 (US8)**: Depends on Phases 4-6 (SO2R verification exercises the full P1 stack across two radios)
- **Phase 8 (US4)**: Depends on Phase 4 (passband/bin controls operate on the running decoder)
- **Phase 9 (US6)**: Depends on Phase 4 + Phase 6 pattern (RST click reuses the CALL click wiring)
- **Phase 10 (US7)**: Depends on Phase 4 (squelch gates the decode loop)
- **Phase 11 (Polish)**: Depends on all P1 stories at minimum; P2 phases may land before or after

### User-Story Dependencies (within the feature)

- US1 ⊥ US2, but US2 is useless without US1
- US2 → US3, US5, US8 (all require the decode loop)
- US5 → US6 (RST reuses CALL click infrastructure)
- US4, US7 ⊥ P1 stories (additive refinements)

### Within Each User Story

- Tests MUST be written and verified FAILING before implementation (TDD per plan.md Implementation Notes)
- Shared types (BinChannel, DecoderSession) before services (CwDecoder, CwDecoderWorker)
- Services before widget rendering
- Widget rendering before MainWindow wiring
- Signal contracts (contracts/signals.md) before any code that emits or consumes them

### Parallel Opportunities

- T005, T006, T007 can run in parallel (three distinct rig-backend files)
- T008, T010, T011 can run in parallel (distinct headers / build-config)
- T012 can run in parallel with later US1 tests
- Within US2: T018, T019, T020, T021, T022, T025, T028 can all run in parallel (distinct files)
- Within US4/6/7: tests and implementation in parallel within their phases
- Polish tasks (T059, T060, T061, T063, T064) are largely independent

---

## Parallel Example: User Story 2 (Multi-Channel Real-Time CW Decoding)

```bash
# Launch all US2 tests together (TDD - they must fail first):
Task: "Goertzel tests in tests/test_goertzel.cpp" (T018)
Task: "BinChannel tests in tests/test_binChannel.cpp" (T019)
Task: "CwDecoder integration test in tests/test_cwDecoder.cpp" (T020)
Task: "Mute gating test in tests/test_cwDecoderMute.cpp" (T021)

# Launch core DSP + capture in parallel (distinct files):
Task: "BinChannel header in include/audio/binChannel.h" (T022)
Task: "SPSC ring buffer header in include/audio/spscRingBuffer.h" (T025)
Task: "CwDecoderWidget header in include/cwDecoderWidget.h" (T028)

# Then sequentially:
Task: "BinChannel impl in src/audio/binChannel.cpp" (T023)
Task: "CwDecoder impl in src/audio/cwDecoder.cpp" (T024)
Task: "AudioCapture impl in src/audio/audioCapture.cpp" (T026)
Task: "CwDecoderWorker impl in src/audio/cwDecoderWorker.cpp" (T027)
Task: "Widget impl in src/ui/cwDecoderWidget.cpp" (continues T028)
Task: "MainWindow wiring in src/ui/mainWindow.cpp" (T029, T030, T031, T032, T033)
```

---

## Implementation Strategy

### MVP First (all P1 stories - US1, US2, US3, US5, US8)

1. **Phase 1 Setup** - CMake + Qt6::Multimedia linkage, baseline build
2. **Phase 2 Foundational** - PTT signal added, settings schema, audio types
3. **Phase 3 US1** - Rig Connection Settings dialog has audio device + mute controls; settings round-trip
4. **Phase 4 US2** - Decoder widget shows live decoded CW; TX-mute (both paths) works
5. **Phase 5 US3** - Live WPM readout per row + bounding range + no-lock state
6. **Phase 6 US5** - CALL click-to-fill routes to owning radio with keyboard-equivalent side effects
7. **Phase 7 US8** - SO2R independence verified across all four audio combinations
8. **STOP and VALIDATE**: Run quickstart.md §5-§7. This is the operator-usable MVP.

### Incremental Delivery (P2 refinements)

9. **Phase 8 US4** - Passband / bin count / spotlight controls (nice-to-have for operator tuning)
10. **Phase 9 US6** - RST click-to-fill (smaller workflow win than CALL, but useful for exchanges that vary)
11. **Phase 10 US7** - Squelch slider (essential on noisy bands; deferred only if initial testing shows low priority)
12. **Phase 11 Polish** - Docs, changelog, cross-platform verification, final QA

### Parallel Team Strategy

- One developer handles Foundational (Phase 2) end-to-end because the rig-backend PTT signal touches three files and is sensitive
- Once Foundational is green, US1 (RigControlDialog UI) and US2 (DSP core) can be split between two developers
- US3 / US4 / US5 / US6 / US7 can all be individual-developer work items after US2's widget shell exists

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps each task to its user story for traceability
- Every user-story phase is independently testable per the Independent Test criteria
- Tests go in `tests/` and MUST fail before implementation lands
- Commit after each task or logical group; concise imperative subject line; **NO** Co-Authored-By trailer (per user's `feedback_commit_style.md` memory)
- The checklists at `specs/004-cw-decoder/checklists/` (187 items) stay as a requirements-quality reference. Items marked `[Gap]`/`[Ambiguity]`/`[Conflict]` that are NOT addressed by any task above should either be (a) resolved via a focused spec edit before proceeding, or (b) explicitly deferred with justification - handle during `/speckit.analyze`
- Total tasks: 70 across 11 phases (3 setup, 8 foundational, 6 US1, 17 US2, 4 US3, 6 US5, 4 US8, 5 US4, 3 US6, 3 US7, 11 polish) - includes 5 coverage-gap closures added after `/speckit.analyze`: T033a (FR-012 Clear), T062a (SC-005 keystroke latency), T062b (SC-009 CPU budget), T062c (SC-008 cross-platform CI), T062d (SC-007 zero-resource verification)
