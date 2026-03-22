# SpecKit Workflow: SPEC-001 — Visual Band Map

**Template Version**: 1.0.0
**Created**: 2026-03-21
**Purpose**: End-to-end SpecKit workflow for the Visual Band Map feature.

---

## Workflow Overview

| Phase | Command | Status | Notes |
|-------|---------|--------|-------|
| Specify | `/speckit.specify` | ⏳ Pending | |
| Clarify | `/speckit.clarify` | ⏳ Pending | Recommended — rendering + interaction details |
| Plan | `/speckit.plan` | ⏳ Pending | |
| Checklist | `/speckit.checklist` | ⏳ Pending | ux, performance, error-handling |
| Tasks | `/speckit.tasks` | ⏳ Pending | |
| Analyze | `/speckit.analyze` | ⏳ Pending | |
| Implement | `/speckit.implement` | ⏳ Pending | |

---

## Gate Checklist

| Gate | After | Pass Criteria |
|------|-------|---------------|
| G1 | Specify | All user stories clear, no `[NEEDS CLARIFICATION]` markers remain |
| G2 | Clarify | Rendering model and click-to-QSY interaction fully resolved |
| G3 | Plan | Architecture approved, constitution gates pass, dependencies identified |
| G4 | Checklist | All `[Gap]` markers addressed |
| G5 | Tasks | Task coverage verified, dependencies ordered |
| G6 | Analyze | No `CRITICAL` issues, `WARNING` items reviewed |
| G7 | Implement | `make` succeeds, `make test` passes, manual spot display verified |

---

## Prerequisites

### Constitution Validation

| Principle | Requirement | Verification |
|-----------|-------------|--------------|
| I. Contest Accuracy | Band map spot coloring (new mult, worked) must use ContestEngine state | Code review |
| II. Qt6-Native Architecture | Widget MUST use Qt6 idioms (QPainter, signals/slots, QDockWidget) | Code review |
| III. Keyboard-First | Click-to-QSY is an enhancement; keyboard QSY path must not regress | Manual test |
| IV. JSON-Driven Contests | Band ranges read from contest JSON `frequencies` field; no hardcoding | Code review |
| V. Simplicity | No waterfall, no SDR — frequency-axis spot map only | Code review |

**Constitution Check:** ⏳ (mark ✅ / ❌ before proceeding to G1)

---

## Specification Context

### Basic Information

| Field | Value |
|-------|-------|
| **Spec ID** | SPEC-001 |
| **Name** | Visual Band Map |
| **Branch** | `sw/spec-001-band-map` |
| **Dependencies** | None |
| **Enables** | SPEC-002 (SO2R adds 2nd radio marker to band map) |
| **Priority** | P1 |

### Success Criteria Summary

- [ ] Band map dock widget appears in Window menu and docks/undocks correctly
- [ ] Spots from active DX cluster connection appear on the frequency axis within 1 second of arrival
- [ ] Current contest band range is shown correctly (read from contest JSON)
- [ ] Spot color coding reflects: new multiplier / already-worked / unworked non-mult
- [ ] Clicking a spot QSYs the active radio via the existing flrig path
- [ ] Spots older than configured age (default 30 min) are removed automatically
- [ ] Widget state (dock position, zoom level) persists across sessions

---

## Phase 1: Specify

**When to run:** At the start. Focus on WHAT and WHY, not implementation details.
**Output:** `specs/spec-001-band-map/spec.md`

### Specify Prompt

```
/speckit.specify

## Feature: Visual Band Map (SPEC-001)

### Problem Statement

Contest operators using DX cluster spots currently see them only in a tabular list
(the DX Cluster panel). There is no visual representation of where spots fall on the
band. Operators must mentally map frequencies to positions, which slows S&P operation.
A frequency-axis band map — like those found in N1MM+ and Win-Test — lets operators
see the entire band at a glance, identify clusters of activity, and click to QSY
directly to a spot.

### Users

- Contest operators running Search & Pounce (S&P) on any supported band
- Operators monitoring a band for new multipliers while running on another frequency
  (SO2R context, to be enhanced in SPEC-002)

### User Stories

1. As an operator, I want to see all current DX cluster spots laid out on a frequency
   axis for my current band, so I can identify QSO opportunities without scanning a list.

2. As an operator, I want spots color-coded by status (new multiplier, already worked,
   unworked non-mult), so I can prioritize the most valuable contacts at a glance.

3. As an operator, I want to click a spot on the band map to QSY my radio to that
   frequency and mode, so I can work the contact without typing the frequency.

4. As an operator, I want stale spots (older than a configurable threshold, default
   30 minutes) to be automatically removed from the map, so the display stays current.

5. As an operator, I want to zoom/pan the frequency axis to focus on a segment of
   the band (e.g., the CW portion), so I can see spot labels without crowding.

### Constraints

- The band map MUST be a QDockWidget — dockable, floatable, closable — consistent
  with all other panels in ContestLogX
- The widget MUST consume spots from the existing DxClusterPanel (via signal) — no
  second cluster connection
- Band range MUST be derived from the contest JSON `frequencies` field for the active
  contest band — no hardcoded band edges
- When no contest is loaded or no cluster connection is active, the widget shows an
  appropriate empty state
- The radio QSY path MUST call the existing flrig QSY mechanism — no duplicate rig
  control logic
- Widget layout state (dock position, size, zoom) MUST persist via QMainWindow
  saveState/restoreState and QSettings

### Out of Scope

- Showing two radios' positions (SPEC-002 will add this)
- Waterfall or panadapter display (audio/SDR input) — frequency-axis spot map only
- Networked spot sharing across multi-op stations (SPEC-003)
- Spotting (transmitting spots to the cluster) — already handled by existing UI
```

### Specify Results

| Metric | Value |
|--------|-------|
| Functional Requirements | (fill after running) |
| User Stories | 5 |
| Acceptance Criteria | (fill after running) |

### Files Generated

- [ ] `specs/spec-001-band-map/spec.md`

---

## Phase 2: Clarify

**When to run:** After spec — rendering model and interaction details benefit from
explicit resolution before planning.

### Clarify Session 1: Rendering & Layout

```
/speckit.clarify Focus on rendering and layout:
- How are overlapping spots (multiple spots within a few kHz) displayed — stacked
  vertically, or truncated with a count indicator?
- What is the minimum zoom granularity (e.g., can the operator zoom to show only
  a 10 kHz window, or is the minimum one full sub-band like the CW segment)?
- Are spot labels shown always, or only on hover/selection?
- How is the operator's own radio frequency (the VFO) shown on the map — as a
  distinct marker, a line, or not shown at all?
```

### Clarify Session 2: Spot Lifecycle & Color Logic

```
/speckit.clarify Focus on spot state and color coding:
- Spot status (new mult / worked / unworked) must be computed from ContestEngine
  state — which ContestEngine method or data structure is the source of truth?
- When a spot's status changes (e.g., operator works the mult), does the color
  update in real time or only on next spot refresh?
- If the same callsign is spotted on slightly different frequencies (cluster
  aggregation), are they shown as one spot or multiple?
- What happens to spots when the operator changes band — are they cleared immediately
  or retained until the next cluster update?
```

### Clarify Results

| Session | Focus Area | Questions | Key Outcomes |
|---------|------------|-----------|--------------|
| 1 | Rendering & Layout | 4 | (fill after running) |
| 2 | Spot Lifecycle & Color | 4 | (fill after running) |

---

## Phase 3: Plan

**When to run:** After spec is finalized and clarify questions resolved.
**Output:** `specs/spec-001-band-map/plan.md`

### Plan Prompt

```
/speckit.plan

## Tech Stack

- Language: C++17
- Framework: Qt6 (Core, Widgets, Network)
- UI: QDockWidget, QPainter (custom widget rendering), QScrollArea or custom pan/zoom
- Build: CMake 3.16+ with Makefile wrapper (`make` to build, `make test` for unit tests)
- Testing: Custom Python test runner (`make test-logs`) for contest log validation;
  Qt unit tests in `tests/` for any testable logic
- Platform: Linux, macOS, Windows (cross-platform required)

## Architecture Notes

- `BandMapWidget` is a QDockWidget containing a custom QWidget subclass that
  overrides `paintEvent()` for frequency-axis rendering
- Spots are received from `DxClusterPanel` via a signal — `DxClusterPanel` needs a
  new `spotReceived(SpotData)` signal added; `SpotData` is a struct with callsign,
  frequency (MHz), mode, spotter, timestamp
- Spot status (new mult / worked) is queried from `ContestEngine` — use existing
  multiplier tracking methods
- QSY on click: call existing `MainWindow::qsyToFrequency(double mhz, QString mode)`
  (or equivalent) — do not duplicate rig control logic
- Band range: read from `ContestEngine::getAllowedBands()` and the `frequencies`
  section of the contest JSON for the active band's min/max frequency
- Spot expiry: a `QTimer` fires every 60 seconds; spots older than the configured
  threshold are removed and the widget repaints

## Project Structure

Source files follow this layout (no subdirectories under src/ except src/ui/,
src/core/, src/database/, src/engine/ — note: contestEngine is at src/contestEngine.cpp):

src/ui/bandMapWidget.cpp
include/bandMapWidget.h
src/ui/dxClusterPanel.cpp  (add spotReceived signal)
include/dxClusterPanel.h
src/ui/mainWindow.cpp      (add dock, wire signals)
include/mainWindow.h

## Constraints

- No new third-party dependencies — Qt6 only
- The custom paint widget MUST handle HiDPI (use logical coordinates, Qt handles
  device pixel ratio)
- Spot storage in BandMapWidget is a QList<SpotData> — simple, no external cache
- All new classes MUST have descriptive objectName() set for QMainWindow state
  persistence
```

### Plan Results

| Artifact | Status | Notes |
|----------|--------|-------|
| `plan.md` | ⏳ | |
| `research.md` | ⏳ | |
| `data-model.md` | ⏳ | SpotData struct |
| `contracts/` | ⏳ | Signal interfaces |
| `quickstart.md` | ⏳ | |

---

## Phase 4: Domain Checklists

**Recommended domains** (from spec analysis):

| Signal in Spec | Domain |
|---|---|
| Custom QPainter rendering, dockable widget, click interaction | **ux** |
| Real-time spot updates, 30-min expiry timer, repaint on every spot | **performance** |
| DX cluster connection may be absent; contest may not be loaded | **error-handling** |

### Checklist 1: UX

```
/speckit.checklist ux

Focus on Visual Band Map requirements:
- Custom QPainter widget: are spot labels readable at all zoom levels? Is crowding
  handled (overlapping spots)?
- Click target areas: are spots large enough to click accurately, especially when
  zoomed out?
- Zoom/pan interaction: is the zoom gesture discoverable (scroll wheel? slider?)?
- Empty states: no contest loaded, no cluster connected, no spots on current band —
  each needs a distinct, informative empty state
- Dock widget behavior: does the widget degrade gracefully when very narrow or
  very short (docked alongside other panels)?
- Pay special attention to: spot label overlap when many spots are clustered in a
  narrow frequency range
```

### Checklist 2: Performance

```
/speckit.checklist performance

Focus on Visual Band Map requirements:
- Repaint frequency: every incoming spot triggers a repaint — is this debounced or
  batched to avoid excessive repaints during cluster bursts?
- Spot expiry timer: runs every 60s scanning all spots — acceptable for typical
  contest spot counts (50–500 spots)?
- ContestEngine status query: called per spot to determine color — is this O(1) or
  does it involve a list scan?
- QPainter performance: full widget repaint vs. dirty-rect optimization for
  incremental spot additions
- Pay special attention to: behavior during cluster burst on band opening (50+ spots
  arriving within a few seconds)
```

### Checklist 3: Error Handling

```
/speckit.checklist error-handling

Focus on Visual Band Map requirements:
- No cluster connection: band map should show "No cluster connection" state, not crash
- No contest loaded: band map should show "No contest loaded" (no band range available)
- Contest loaded but current band not in contest bands: graceful empty state
- flrig not connected: click-to-QSY should show a status bar message and not crash
- Pay special attention to: spot arriving after contest is unloaded mid-session
```

### Checklist Results

| Checklist | Items | Gaps | Notes |
|-----------|-------|------|-------|
| ux | | | (fill after running) |
| performance | | | (fill after running) |
| error-handling | | | (fill after running) |

---

## Phase 5: Tasks

**Output:** `specs/spec-001-band-map/tasks.md`

### Tasks Prompt

```
/speckit.tasks

## Task Structure
- Small, testable chunks (1-2 hours each)
- Clear acceptance criteria referencing FR-xxx from spec.md
- Dependency ordering: data structures → signals → rendering → integration → polish
- Mark parallel-safe tasks with [P]
- Organize by user story

## Project File Layout

Source files:
  src/ui/           — UI widgets (bandMapWidget.cpp goes here)
  src/core/         — Non-UI core logic
  include/          — All headers (flat, no subdirectories)
  tests/            — Unit tests

Build commands:
  make              — build
  make test         — unit tests
  make test-logs    — automated log validation (not needed for this spec)

## Implementation Phases
1. Foundation — SpotData struct, DxClusterPanel signal, ContestEngine band-range query
2. User Story 1 — BandMapWidget rendering (frequency axis, spot markers, labels)
3. User Story 2 — Spot color coding from ContestEngine state
4. User Story 3 — Click-to-QSY
5. User Story 4 — Spot expiry timer
6. User Story 5 — Zoom/pan
7. Polish — Empty states, settings persistence, Window menu integration
```

### Tasks Results

| Metric | Value |
|--------|-------|
| Total Tasks | (fill after running) |
| Phases | 7 |
| Parallel Opportunities | (fill after running) |
| User Stories Covered | 5 |

---

## Phase 6: Analyze

### Analyze Prompt

```
/speckit.analyze

Focus on:
1. Constitution alignment — Qt6 idioms, no hardcoded band edges, no duplicate rig
   control, keyboard path regression check
2. Coverage gaps — all 5 user stories have tasks; empty states covered; settings
   persistence covered
3. File path consistency — verify task file paths match actual project layout
   (src/ui/, include/ flat, no subdirectories)
4. Verify US1 (spot display) and US3 (click-to-QSY) have complete task coverage
   as the highest-risk items
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

## Approach: Incremental Integration

For each task:
1. Write the code
2. Build with `make` — must succeed with zero warnings
3. For any testable logic (e.g., spot expiry calculation, frequency-to-pixel math),
   write a unit test in tests/ first
4. Manual verification: launch ContestLogX with a contest loaded and DX cluster
   connected; confirm spot appears on map within 1 second of cluster arrival

## Pre-Implementation Setup

1. Create branch: `git checkout -b sw/spec-001-band-map`
2. Verify baseline: `make && make test` — must pass before any changes
3. Confirm DX cluster connection is available for integration testing

## Implementation Notes

- All Qt object names set via `setObjectName()` for dock state persistence
- Use `qRound()` for frequency-to-pixel conversions (avoid floating-point pixel gaps)
- DxClusterPanel signal must be added with backward-compatible default — existing
  connections must not break
- QSY path: call the same method the DX Cluster table's row-click already calls
  (find it in mainWindow.cpp before implementing — do not create a parallel path)
- HiDPI: use `painter.setRenderHint(QPainter::Antialiasing)` and work in logical
  pixels; Qt handles device pixel ratio automatically
```

### Implementation Progress

| Phase | Tasks | Completed | Notes |
|-------|-------|-----------|-------|
| 1 - Foundation | | | SpotData, signal, band range |
| 2 - US1: Spot rendering | | | Frequency axis + markers |
| 3 - US2: Color coding | | | ContestEngine query |
| 4 - US3: Click-to-QSY | | | |
| 5 - US4: Spot expiry | | | QTimer |
| 6 - US5: Zoom/pan | | | |
| 7 - Polish | | | Empty states, persistence |

---

## Post-Implementation Checklist

- [ ] All tasks marked complete in tasks.md
- [ ] `make` succeeds with zero warnings on GCC
- [ ] `make test` passes
- [ ] Band map dock appears in Window menu
- [ ] Spots appear on map within 1 second of DX cluster arrival
- [ ] Click on spot QSYs radio (verified with flrig connected)
- [ ] Spots expire after configured threshold
- [ ] No crash when cluster not connected or no contest loaded
- [ ] Dock state persists across app restart
- [ ] Update `docs/ai/specs/contestlogx-master-plan.md` — mark SPEC-001 ✅ Complete

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
│   ├── ui/               # UI widgets (mainWindow, dxClusterPanel, bandMapWidget, ...)
│   ├── core/             # Non-UI logic
│   ├── database/         # DxccDatabase
│   └── contestEngine.cpp # Core scoring engine (note: NOT in src/engine/)
├── include/              # All headers (flat — no subdirectories)
├── contests/             # Contest JSON definitions
├── tests/                # Unit tests
├── scripts/              # Test runners
├── test_logs/            # Automated test data
├── docs/
│   └── ai/specs/         # SpecKit spec artifacts
└── .specify/             # SpecKit config and templates
```
