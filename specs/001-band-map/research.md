# Research: Visual Band Map

**Branch**: `001-band-map` | **Date**: 2026-03-21

All technical unknowns resolved from existing project knowledge and Qt6
documentation. No external library research required — pure Qt6.

---

## Decision 1: Spot Storage Structure

**Decision**: `QHash<QString, SpotData>` keyed by `callsign + "|" + roundedFreq`

**Rationale**: O(1) lookup for deduplication on every incoming spot. The key
combines callsign and rounded frequency (nearest 0.1 kHz) to treat same-callsign
spots within 0.1 kHz as the same spot (reset timestamp) while showing the same
callsign on widely separated frequencies as distinct markers.

**Alternatives considered**:
- `QList<SpotData>` with linear scan — O(n) dedup, unacceptable at 200+ spots
- `std::unordered_map` — no advantage over QHash in a Qt6 context; adds
  inconsistency with project's Qt container usage

---

## Decision 2: Custom Rendering vs. Qt Chart / QGraphicsView

**Decision**: Custom `QPainter` rendering in a `QWidget::paintEvent()` override

**Rationale**: Qt Charts (a separate Qt module not currently in the project's
dependencies) adds weight and complexity for what is essentially a one-dimensional
axis with labeled point markers. `QGraphicsScene`/`QGraphicsView` is powerful but
over-engineered for this use case. A `paintEvent()` override gives full control
over rendering, is lightweight, and is the established Qt idiom for custom
visualizations in the project.

**Alternatives considered**:
- Qt Charts (`QChart` + `QScatterSeries`) — would require adding Qt6::Charts
  dependency; violates constitution Principle V (Simplicity). Rejected.
- `QGraphicsView` — appropriate for complex interactive scene graphs; unnecessary
  here. Rejected.

---

## Decision 3: Band Range Derivation

**Decision**: Parse the contest JSON `frequencies` object to find the band's
`min` and `max` fields for the active band name.

**Rationale**: `ContestEngine::getAllowedBands()` already returns the list of band
names from the contest JSON. A new `ContestEngine::getBandFrequencyRange(QString band)`
method will look up the `frequencies[band]` object and return `{min, max}` in MHz.
This keeps band-range logic inside `ContestEngine` where all other contest-definition
reading lives.

**Alternatives considered**:
- Hardcoding band edges (14.000–14.350 MHz for 20m, etc.) — violates Principle IV
  (JSON-Driven). Rejected.
- Reading the JSON directly from `BandMapWidget` — violates separation of concerns.
  Rejected.

---

## Decision 4: Contact Status Resolution

**Decision**: `MainWindow` resolves status by calling `ContestEngine` methods and
passes the resolved `ContactStatus` enum value to `BandMapWidget`. `BandMapWidget`
is decoupled from `ContestEngine`.

**Rationale**: `BandMapWidget` should not depend on `ContestEngine` directly —
this would couple the UI widget to the core engine and complicate unit testing.
`MainWindow` already orchestrates all `ContestEngine` interactions. A lambda-based
`refreshAllStatuses()` callback pattern allows `MainWindow` to supply status without
`BandMapWidget` knowing how it's computed.

**Status determination logic** (implemented in `MainWindow::resolveSpotStatus()`):
1. If `contestEngine.isNamedMultiplier(callsign)` returns true AND callsign is not
   yet in the worked-mults set → `NewMultiplier`
2. If callsign is in the logged QSO set → `Worked`
3. Otherwise → `UnworkedNonMult`

---

## Decision 5: Repaint Strategy During Cluster Bursts

**Decision**: Direct `update()` call per spot (Qt defers actual repaints and
coalesces multiple `update()` calls within the same event loop iteration).

**Rationale**: Qt's `update()` is non-blocking and idempotent within an event loop
cycle — multiple calls before the next paint event result in a single `paintEvent()`
call. No explicit debouncing timer is needed; Qt's event loop provides it for free.

**Alternatives considered**:
- Explicit debounce timer (e.g., 100ms) — more control but unnecessary complexity
  given Qt's built-in coalescing. Rejected.
- `repaint()` (immediate, synchronous) — blocks the event loop during bursts.
  Rejected.

---

## Decision 6: Tooltip Implementation

**Decision**: `QToolTip::showText()` called from `mouseMoveEvent()` in
`BandMapCanvas`.

**Rationale**: Standard Qt approach. No custom tooltip widget needed. Tooltip
content (callsign, frequency, mode, spotter, age in minutes) formatted as a
multi-line string. `setMouseTracking(true)` required on `BandMapCanvas` to receive
move events without button held.

---

## No-Research Items (Already Determined)

| Item | Resolution |
|------|-----------|
| Dock state persistence | `objectName()` + `QMainWindow::saveState()` — established Qt pattern used by all other docks in the project |
| Zoom/pan state persistence | `QSettings` — used throughout the project for all widget state |
| QSY path | Re-use existing `MainWindow::qsyToFrequency()` — verified it exists via DX cluster table row-click |
| HiDPI rendering | `QPainter` logical coordinates + `setRenderHint(Antialiasing)` — Qt handles device pixel ratio |
| Expiry timer interval | 60-second `QTimer` — matches user expectation; 30-min spots need only minute-level accuracy |
