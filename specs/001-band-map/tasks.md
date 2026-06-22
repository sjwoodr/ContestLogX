# Tasks: Visual Band Map

**Input**: Design documents from `specs/001-band-map/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/signals.md ✅, quickstart.md ✅

**Organization**: Tasks grouped by user story - each story is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no incomplete dependencies)
- **[Story]**: Which user story this task belongs to (US1-US5)
- Exact file paths included in all descriptions

---

## Phase 1: Setup (New Files & CMake)

**Purpose**: Create the new files and register them with the build system. No existing files modified yet.

- [x] T001 Create `include/bandMapWidget.h` with empty class declarations for `BandMapWidget` and `BandMapCanvas` (placeholder stubs - no implementation yet)
- [x] T002 Create `src/ui/bandMapWidget.cpp` with minimal empty constructor/destructor bodies that compile cleanly
- [x] T003 Create `tests/test_bandmap.cpp` with a single placeholder test that asserts `true` (ensures the test file compiles and links)
- [x] T004 Add `src/ui/bandMapWidget.cpp` to `CMakeLists.txt` under the existing source list (follow the pattern of other `src/ui/*.cpp` entries)
- [x] T005 Add `tests/test_bandmap.cpp` to `CMakeLists.txt` under the test target (follow the pattern of other test files)
- [x] T006 Run `make` and `make test` - confirm zero warnings, zero errors, placeholder test passes

**Checkpoint**: `make` and `make test` pass cleanly before any logic is added.

---

## Phase 2: Foundation (Blocking Prerequisites for All Stories)

**Purpose**: Data model, dedup logic, signal wiring, and band-range query - must be complete before US1 rendering can start.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete.

- [x] T007 Define `ContactStatus` enum and `SpotData` struct in `include/bandMapWidget.h`. Fields: `callsign` (QString), `freqMhz` (double), `mode` (QString), `spotter` (QString), `timestamp` (QDateTime), `status` (ContactStatus). Values: `NewMultiplier`, `Worked`, `UnworkedNonMult`, `Unknown`.
- [x] T008 [P] Implement `BandMapWidget::dedupKey(const SpotData &spot)` static helper in `src/ui/bandMapWidget.cpp`: returns `callsign + "|" + QString::number(qRound(freqMhz * 10000))`. Add matching unit test in `tests/test_bandmap.cpp` (same callsign ±0.05 kHz → same key; same callsign ±0.15 kHz → different key).
- [x] T009 [P] Add `BandRange` struct to `include/bandMapWidget.h`: fields `band` (QString), `minMhz` (double), `maxMhz` (double). Add `m_bandRange` member to `BandMapWidget`.
- [x] T010 Add `m_spots` member (`QHash<QString, SpotData>`) and `m_expirySeconds` (`int`, default 1800) to `BandMapWidget` in `include/bandMapWidget.h`.
- [x] T011 Add `void spotReceived(const SpotData &spot)` signal declaration to `include/dxClusterPanel.h` in the `signals:` section (after existing signals).
- [x] T012 Refactor spot-parsing in `src/ui/dxClusterPanel.cpp`: extract the cluster-line parser into a `SpotData` struct, preserve existing table-population behavior, then emit `spotReceived(spot)` after the table is updated. `spot.status` is set to `ContactStatus::Unknown` by `DxClusterPanel`.
- [x] T013 Add `m_bandMapWidget` member (`BandMapWidget*`) and `onSpotReceived(const SpotData &spot)` slot declaration to `include/mainWindow.h`.
- [x] T014 In `src/ui/mainWindow.cpp`: implement `MainWindow::onSpotReceived` - calls `resolveSpotStatus(spot.callsign)` (stub returning `Unknown` for now), sets `spot.status`, calls `m_bandMapWidget->addOrUpdateSpot(spot)`.
- [x] T015 Run `make` - confirm zero warnings before proceeding to US1.

**Checkpoint**: Foundation complete - `SpotData`, dedup key, signal wiring, and `onSpotReceived` stub all compile cleanly.

---

## Phase 3: User Story 1 - Spot Display on Frequency Axis (Priority: P1) 🎯 MVP

**Goal**: Band map panel shows all DX cluster spots for the current band as labeled markers on a frequency axis.

**Independent Test**: Open Window → Band Map with a contest loaded. Connect to a DX cluster. Verify spots appear as labeled markers within ~2 seconds of arrival in the cluster panel. Verify each marker is at the correct relative horizontal position for its frequency.

- [x] T016 [US1] Declare `BandMapCanvas` as a `QWidget` subclass in `include/bandMapWidget.h` with `paintEvent(QPaintEvent*)` override. Declare `BandMapWidget` as a `QDockWidget` subclass with `addOrUpdateSpot(const SpotData&)` public slot and `setBandRange(double, double, const QString&)` public slot.
- [x] T017 [US1] Implement `BandMapWidget` constructor in `src/ui/bandMapWidget.cpp`: set `objectName("BandMapWidget")`, set window title "Band Map", create `BandMapCanvas` as central widget, set `QDockWidget::DockWidgetMovable | DockWidgetFloatable | DockWidgetClosable` features.
- [x] T018 [US1] Implement `BandMapCanvas::paintEvent()` in `src/ui/bandMapWidget.cpp`: draw a horizontal frequency axis line across the full widget width; draw tick marks at 10 kHz intervals labeled in kHz (relative offset from band start, e.g., "+25k"). Use `QPainter`.
- [x] T019 [US1] Implement the spot marker rendering loop in `BandMapCanvas::paintEvent()`: for each spot in `m_spots` whose `freqMhz` is within `[m_visibleMinMhz, m_visibleMaxMhz]`, compute x-pixel as `(freqMhz - visMin) / (visMax - visMin) * width()`, draw a small filled rectangle, draw the callsign label above it. Use `ContactStatus::Unknown` color (gray) for all spots at this phase.
- [x] T020 [US1] Implement `BandMapWidget::addOrUpdateSpot(const SpotData &spot)` in `src/ui/bandMapWidget.cpp`: compute dedup key; if key exists, update `timestamp` and other fields; if new, insert into `m_spots`; call `m_canvas->update()` to trigger repaint. Enforce FR-016: if `m_spots.size()` would exceed `m_maxSpots` after insertion, evict the spot with the oldest `timestamp` before inserting.
- [x] T021 [US1] Implement `BandMapWidget::setBandRange(double minMhz, double maxMhz, const QString &band)` in `src/ui/bandMapWidget.cpp`: set `m_bandRange`; reset `m_visibleMinMhz = minMhz`, `m_visibleMaxMhz = maxMhz`; clear `m_spots` entirely (band change clears all spots per spec); call `m_canvas->update()`.
- [x] T022 [US1] In `src/ui/mainWindow.cpp` `createBandMapDock()`: instantiate `m_bandMapWidget`, add it as a dock widget via `addDockWidget(Qt::RightDockWidgetArea, m_bandMapWidget)`, add a "Band Map" toggle action to the Window menu (follow the pattern of other dock toggles in the menu).
- [x] T023 [US1] In `src/ui/mainWindow.cpp` `createConnections()`: connect `m_dxClusterPanel::spotReceived` → `MainWindow::onSpotReceived`. Call `createBandMapDock()` from `MainWindow` constructor (after contest and cluster panel setup).
- [x] T024 [US1] Extend the existing rig-frequency polling handler in `src/ui/mainWindow.cpp` (locate the existing `onRigFrequencyChanged` or equivalent): after the existing band-derivation logic, compare derived band to `m_currentBandName`; if changed, call `m_bandMapWidget->setBandRange(minMhz, maxMhz, band)` and update `m_currentBandName`.
- [x] T025 [US1] Add an "empty state" text render to `BandMapCanvas::paintEvent()`: if `m_bandRange.minMhz == 0` (no contest), draw centered text "No contest loaded"; if `m_spots` is empty and band range is valid, draw centered text "No spots on [BAND]".
- [x] T026 [US1] Add unit tests in `tests/test_bandmap.cpp` for `addOrUpdateSpot`: (a) add a spot, verify `m_spots.size() == 1`; (b) add duplicate spot (same key), verify size remains 1 and timestamp is updated; (c) add `m_maxSpots + 1` spots, verify size stays at `m_maxSpots` and the oldest is gone.
- [x] T027 [US1] Run `make` and `make test` - confirm zero warnings and all tests pass. Manually verify: start app with a contest, open band map, connect to cluster, confirm spots appear.

---

## Phase 4: User Story 2 - Spot Color Coding by Contact Status (Priority: P2)

**Goal**: Spots are color-coded by contact status: new multiplier (orange-red), worked (gray), unworked non-mult (blue). Colors update when QSOs are logged, edited, or deleted.

**Independent Test**: Work a station shown as a new multiplier. Without closing the band map, log the QSO. Verify the marker color changes from orange-red to dark gray without the spot disappearing.

- [x] T028 [P] [US2] Add `resolveSpotStatus(const QString &callsign)` method declaration to `include/mainWindow.h` and implement it in `src/ui/mainWindow.cpp`: query `ContestEngine::isMultiplier(callsign)` and the worked-calls set; return the appropriate `ContactStatus` enum value. Return `Unknown` if no contest is loaded.
- [x] T029 [P] [US2] Update `MainWindow::onSpotReceived` in `src/ui/mainWindow.cpp` to call `resolveSpotStatus(spot.callsign)` and set `spot.status` before calling `m_bandMapWidget->addOrUpdateSpot(spot)` (replacing the stub from T014).
- [x] T030 [US2] Add `refreshAllStatuses(std::function<ContactStatus(const QString&)> resolver)` public slot to `BandMapWidget`. Implement in `src/ui/bandMapWidget.cpp`: iterate `m_spots`, call `resolver` for each callsign, update `status` field; call `m_canvas->update()` if any status changed.
- [x] T031 [US2] Update `BandMapCanvas::paintEvent()` in `src/ui/bandMapWidget.cpp` to use per-status fill colors: `NewMultiplier` → `QColor("#FF6B00")`, `Worked` → `QColor("#505050")`, `UnworkedNonMult` → `QColor("#1E90FF")`, `Unknown` → `QColor("#808080")`.
- [x] T032 [US2] Identify the existing log-change event path in `src/ui/mainWindow.cpp` (the signal/slot triggered after `onLogQso`, edit, or delete). Extend it to call `m_bandMapWidget->refreshAllStatuses([this](const QString &call) { return resolveSpotStatus(call); })`.
- [x] T033 [US2] Add unit tests in `tests/test_bandmap.cpp` for `refreshAllStatuses`: add spots with `Unknown` status; call `refreshAllStatuses` with a lambda returning `NewMultiplier` for one callsign, `Worked` for another; verify the `status` fields updated correctly.
- [x] T034 [US2] Run `make` and `make test`. Manually verify color coding: load a contest with known worked stations and multipliers; open band map; confirm spots show correct colors; log a QSO with a new-mult spot; confirm color changes to gray.

---

## Phase 5: User Story 3 - Click to QSY (Priority: P3)

**Goal**: Clicking a spot marker on the band map QSYs the connected radio to that spot's frequency and mode within 1 second.

**Independent Test**: With flrig connected, click a spot on the band map. Verify the radio tunes to the correct frequency and mode. With no radio connected, click a spot; verify a status bar message appears and no crash occurs.

- [x] T035 [US3] Add `void spotClicked(double freqMhz, const QString &mode)` signal declaration to `BandMapWidget` in `include/bandMapWidget.h`.
- [x] T036 [US3] Add `mousePressEvent(QMouseEvent*)` override to `BandMapCanvas` in `include/bandMapWidget.h`. Implement in `src/ui/bandMapWidget.cpp`: on left click, iterate visible spots to find the nearest whose x-pixel is within ±5 pixels of the click x; emit `BandMapWidget::spotClicked(spot.freqMhz, spot.mode)`. No action if no spot is within tolerance.
- [x] T037 [US3] In `src/ui/mainWindow.cpp` `createConnections()`: connect `m_bandMapWidget::spotClicked` → `MainWindow::qsyToFrequency` (the existing method used by the DX cluster table row-click - confirm signature compatibility: `void qsyToFrequency(double freqMhz, const QString &mode)`).
- [x] T038 [US3] Run `make`. Manually verify with flrig: click a spot - radio QSYs. Disconnect flrig, click again - confirm status bar message, no crash.

---

## Phase 6: User Story 4 - Automatic Spot Expiry (Priority: P4)

**Goal**: Spots older than the configurable expiry threshold are automatically removed from the band map without manual action.

**Independent Test**: Set the expiry threshold to 2 minutes in Preferences. Allow spots to arrive. Wait 2 minutes. Verify spots disappear without any manual action and the map transitions to the empty state gracefully.

- [x] T039 [US4] Add `m_expiryTimer` (`QTimer*`) member to `BandMapWidget` in `include/bandMapWidget.h`. In `BandMapWidget` constructor (`src/ui/bandMapWidget.cpp`): create `m_expiryTimer`, connect its `timeout()` signal to `BandMapWidget::onExpiryTimer()`, start with 60-second interval.
- [x] T040 [US4] Implement `BandMapWidget::onExpiryTimer()` private slot in `src/ui/bandMapWidget.cpp`: compute cutoff as `QDateTime::currentDateTimeUtc().addSecs(-m_expirySeconds)`; iterate `m_spots` with erase-if pattern; call `m_canvas->update()` if any spots were removed.
- [x] T041 [US4] Load `m_expirySeconds` from `QSettings` key `BandMap/ExpiryMinutes` (converted to seconds) in `BandMapWidget` constructor. Default: 30 minutes (1800 seconds). Load `m_maxSpots` from `QSettings` key `BandMap/MaxSpots` with default 30.
- [x] T042 [US4] Add expiry threshold and max spots settings to the application Preferences dialog in `src/ui/mainWindow.cpp` (or the existing settings/preferences widget - locate the existing preferences dialog and add a "Band Map" section): spin box for "Spot expiry (minutes)", range 1-120, default 30; spin box for "Max spots displayed", range 10-500, default 30. Save to QSettings on accept.
- [x] T043 [US4] Add unit tests in `tests/test_bandmap.cpp` for expiry: add spots with timestamps set to `QDateTime::currentDateTimeUtc().addSecs(-m_expirySeconds - 1)` (already expired); call `onExpiryTimer()` directly; verify `m_spots` is empty afterward.
- [x] T044 [US4] Run `make` and `make test`. Manually verify with short expiry threshold (2 min): spots disappear, no crash on empty map.

---

## Phase 7: User Story 5 - Zoom and Pan the Frequency Axis (Priority: P5)

**Goal**: Operators can zoom the frequency axis via scroll wheel or zoom slider, and pan via click-drag. Zoom/pan resets to full-band default on band change.

**Independent Test**: With 20+ spots visible at full-band zoom, scroll the wheel over the band map. Verify the visible frequency range narrows and spot labels spread out. Drag to pan. Change bands; verify zoom resets to full-band default.

- [x] T045 [US5] Add `m_visibleMinMhz` and `m_visibleMaxMhz` (`double`) members to `BandMapWidget` in `include/bandMapWidget.h`. Initialize them in `setBandRange()` to `minMhz` and `maxMhz` (already done in T021 - verify). Ensure `paintEvent()` uses `m_visibleMinMhz`/`m_visibleMaxMhz` for all pixel-to-frequency mapping (not `m_bandRange.minMhz`/`maxMhz` directly).
- [x] T046 [US5] Add `wheelEvent(QWheelEvent*)` override to `BandMapCanvas` in `include/bandMapWidget.h`. Implement in `src/ui/bandMapWidget.cpp`: compute zoom factor (wheel up = 0.8, down = 1.25); compute center frequency at cursor pixel; compute new range = current range × factor; clamp `[visMin, visMax]` to `[bandRange.minMhz, bandRange.maxMhz]`; enforce minimum zoom of 5 kHz; call `m_canvas->update()`.
- [x] T047 [US5] Add a `QSlider` (horizontal, range 1-20) to the `BandMapWidget` toolbar row (in the constructor in `src/ui/bandMapWidget.cpp`): connect `valueChanged` to a lambda that maps slider value to range width (`fullBandWidth / value`), centered on current viewport midpoint, clamped to band edges. Add a `QLabel` next to the slider showing the visible range (e.g., "14.000-14.070 MHz").
- [x] T048 [US5] Add `mousePressEvent(QMouseEvent*)` and `mouseMoveEvent(QMouseEvent*)` overrides to `BandMapCanvas` for pan (separate from the click-to-QSY press handler in T036). On left button press: record `m_dragStartX` and `m_dragStartVisMin`. On left button move: compute delta pixels → MHz offset; shift `m_visibleMinMhz` and `m_visibleMaxMhz` by the offset; clamp to band edges; call `update()`. Distinguish pan-drag from click: only emit `spotClicked` if mouse did not move more than 3 pixels between press and release.
- [x] T049 [US5] Update `setBandRange()` in `src/ui/bandMapWidget.cpp` to reset `m_visibleMinMhz = minMhz` and `m_visibleMaxMhz = maxMhz` (full-band default) on every call - zoom/pan state is ephemeral and does not persist across band changes (per FR-015).
- [x] T050 [US5] Add unit tests in `tests/test_bandmap.cpp` for zoom clamping: simulate wheel zoom past band edge; verify `m_visibleMinMhz >= m_bandRange.minMhz` and `m_visibleMaxMhz <= m_bandRange.maxMhz`; verify minimum zoom of 5 kHz is enforced.
- [x] T051 [US5] Run `make` and `make test`. Manually verify: scroll to zoom; drag to pan; change bands - verify zoom resets; use slider - verify range label updates.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Tooltip detail, cluster-disconnect indicator, rig VFO line, dock state persistence, HiDPI, and final build verification.

- [x] T052 Override `mouseMoveEvent(QMouseEvent*)` in `BandMapCanvas` for tooltip (in addition to the pan handler from T048 - combine the two if using the same override). When the cursor is within ±3 pixels of a spot marker's x-position: call `QToolTip::showText(event.globalPos(), tooltipText)` where `tooltipText` is formatted as: `"{callsign}\n{freq:.1f} kHz · {mode}\nSpotter: {spotter}\nAge: {N} min"`. When not hovering over any spot, call `QToolTip::hideText()`.
- [x] T053 [P] Add a cluster-disconnect indicator to `BandMapWidget`. Add `m_clusterConnected` (`bool`, default true) member. Add `setClusterConnected(bool)` public slot. In `BandMapCanvas::paintEvent()`: if `!m_clusterConnected`, draw a small red indicator (e.g., top-right corner label "No cluster") without removing existing spots. In `MainWindow`: connect the existing DX cluster connection/disconnection signals to `m_bandMapWidget->setClusterConnected()`. On reconnect, also call `m_bandMapWidget->clearAllSpots()`.
- [x] T054 [P] Draw a rig VFO frequency line on `BandMapCanvas`. Add `m_rigFreqMhz` (`double`, default 0.0) member and `setRigFrequency(double)` public slot to `BandMapWidget`. In `paintEvent()`: if `m_rigFreqMhz > 0` and within visible range, draw a vertical `QColor("#00FF00")` line at the corresponding x-pixel. In `MainWindow`: connect the rig-frequency polling output to `m_bandMapWidget->setRigFrequency(freqMhz)`.
- [x] T055 Implement `BandMapWidget` dock state persistence in `src/ui/mainWindow.cpp`: confirm `objectName("BandMapWidget")` is set (done in T017). Verify `QMainWindow::saveState()` and `restoreState()` calls (already present for other docks) include the band map dock. Zoom/pan state is NOT saved to QSettings (ephemeral by design per FR-015).
- [x] T056 Verify HiDPI rendering: in `BandMapCanvas::paintEvent()`, use `devicePixelRatioF()` for line widths and hit-test tolerances if needed. On a HiDPI display (or simulated via `QT_SCALE_FACTOR=2`), confirm marker positions and labels remain sharp and correctly placed.
- [x] T057 Run `make` (zero warnings required). Run `make test` (all tests pass). Manually run the full quickstart checklist from `specs/001-band-map/quickstart.md` steps 1-10.

---

## Dependencies

```
Phase 1 (Setup)
    │
    ▼
Phase 2 (Foundation - T007-T015)
    │
    ├──► Phase 3: US1 Spot Display (T016-T027) ─► MVP
    │         │
    │         ├──► Phase 4: US2 Color Coding (T028-T034)
    │         │         │
    │         │         ├──► Phase 5: US3 Click-to-QSY (T035-T038)
    │         │         │
    │         │         ├──► Phase 6: US4 Expiry (T039-T044)
    │         │         │
    │         │         └──► Phase 7: US5 Zoom/Pan (T045-T051)
    │         │
    │         └──► Phase 8: Polish (T052-T057) - requires all story phases
    │
    (Phase 4-7 can begin as soon as US1 is complete; US4 and US5 are independent of US2/US3)
```

**Key parallelism within phases**:
- T008 + T009 (dedup key, BandRange): both Phase 2, different concerns, parallel
- T028 + T029 (resolveSpotStatus, onSpotReceived update): parallel within Phase 4
- T053 + T054 (disconnect indicator, VFO line): parallel within Phase 8

---

## Parallel Execution Examples

**Phase 2** (two developers):
- Dev A: T007, T010, T011, T012 (data model + DxClusterPanel signal)
- Dev B: T008, T009, T013, T014 (dedup key + MainWindow stubs)

**Phase 4 + Phase 5 + Phase 6** (after Phase 3 complete):
- Dev A: Phase 4 (US2 color coding)
- Dev B: Phase 6 (US4 expiry - independent of color coding)

**Phase 8** (two developers):
- Dev A: T052 (tooltip) + T055 (persistence)
- Dev B: T053 (disconnect indicator) + T054 (VFO line) + T056 (HiDPI)

---

## Implementation Strategy

**MVP scope**: Phase 1 + Phase 2 + Phase 3 (US1) - spots appear on the frequency axis, unlabeled status color (gray OK for MVP). This delivers the core spatial overview value.

**Increment 2**: Phase 4 (US2) - color coding. Transforms the display into a prioritization tool.

**Increment 3**: Phase 5 (US3) - click-to-QSY. The primary interaction differentiator from the existing cluster table.

**Increment 4**: Phase 6 + Phase 7 (US4 + US5) - expiry and zoom/pan. Quality-of-life for full contest sessions.

**Final**: Phase 8 - tooltip, VFO line, cluster-disconnect indicator, state persistence, HiDPI.
