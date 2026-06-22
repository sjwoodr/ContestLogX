# Implementation Plan: Visual Band Map

**Branch**: `001-band-map` | **Date**: 2026-03-21 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/001-band-map/spec.md`

## Summary

Add a dockable `BandMapWidget` that displays DX cluster spots on a frequency axis
for the operator's current contest band, with contact-status color coding, tooltip
detail, click-to-QSY, automatic expiry, and zoom/pan. Spots are consumed from the
existing `DxClusterPanel` via a new Qt signal; no second cluster connection is
created. Band range is read from the contest JSON. Contact status is queried
event-driven from `ContestEngine` on spot arrival and on every log change.

## Technical Context

**Language/Version**: C++17
**Primary Dependencies**: Qt6 (Core, Widgets) - no new dependencies
**Storage**: In-memory `QHash<QString, SpotData>` per `BandMapWidget` instance;
  `QSettings` for zoom/pan/dock state persistence
**Testing**: `make test` (unit tests for frequency-to-pixel math, expiry logic,
  dedup key generation); `make test-logs` not required (no contest engine changes)
**Target Platform**: Linux, macOS, Windows (cross-platform Qt6)
**Project Type**: desktop-app feature within existing Qt6 application
**Performance Goals**: New spot visible within 2 seconds of cluster arrival;
  repaint debounced to ≤60ms during cluster bursts; expiry scan O(n) over
  typical 10-200 spots acceptable at 60-second interval
**Constraints**: No new third-party dependencies; no duplicate rig control code;
  no hardcoded band edges; `make` must succeed with zero warnings
**Scale/Scope**: Single-user desktop session; 10-200 spots per band typical;
  up to ~500 spots at peak during major contest band openings

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Requirement | Status |
|-----------|-------------|--------|
| I. Contest Accuracy | Band map reads ContestEngine multiplier state (read-only). No changes to scoring, dupe, or exchange logic. `make test-logs` not required. | ✅ Pass |
| II. Qt6-Native Architecture | `BandMapWidget` uses `QPainter`, `QDockWidget`, `QTimer`, signals/slots, `QSettings`. No third-party UI frameworks. | ✅ Pass |
| III. Keyboard-First | Click-to-QSY is a mouse enhancement; existing keyboard QSO entry and QSY paths are unchanged. Band map dock must not intercept keyboard events used by QSO entry. | ✅ Pass - verify in implementation |
| IV. JSON-Driven Contests | Band frequency ranges read from contest JSON `frequencies` field via existing `ContestEngine` API. No hardcoded band edges. | ✅ Pass |
| V. Simplicity & YAGNI | Frequency-axis spot map only. No waterfall, no SDR, no external cache. `QHash` for O(1) dedup lookup. Zoom/pan state in two `double` members. | ✅ Pass |

**Post-design re-check**: ✅ All gates pass. No violations requiring Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/001-band-map/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   └── signals.md       # Phase 1 output - Qt signal interfaces
└── tasks.md             # Phase 2 output (/speckit.tasks - NOT created here)
```

### Source Code (repository root)

```text
src/ui/
├── bandMapWidget.cpp    # NEW - BandMapWidget implementation
├── dxClusterPanel.cpp   # MODIFY - add spotReceived(SpotData) signal
└── mainWindow.cpp       # MODIFY - add dock, wire signals, band-change detection

include/
├── bandMapWidget.h      # NEW
└── dxClusterPanel.h     # MODIFY - add signal declaration

tests/
└── test_bandmap.cpp     # NEW - unit tests for SpotData logic
```

**Structure Decision**: Single-project (existing ContestLogX layout). All new UI
source in `src/ui/`, all headers in `include/` (flat - no subdirectories). Follows
the existing pattern of every other widget in the project.

## Execution Design

### 1. Data Model

See `data-model.md` for full entity definitions.

**`SpotData`** struct (header only, no Qt dependency required):
```
callsign - spotted station's callsign (QString)
freqMhz - spotted frequency in MHz (double)
mode - operating mode string: "CW", "SSB", "FT8", etc. (QString)
spotter - callsign of the spotting station (QString)
timestamp - when the spot was received (QDateTime)
status - contact status enum: NewMultiplier | Worked | UnworkedNonMult
```

**Dedup key**: `callsign + "|" + QString::number(qRound(freqMhz * 10000))` - rounds
frequency to nearest 0.1 kHz. Same callsign spotted within 0.1 kHz = same spot.
Spots of the same callsign further apart appear as separate markers.

### 2. Signal Interface

`DxClusterPanel` adds one new signal:
```cpp
signals:
    void spotReceived(const SpotData &spot);
```
Emitted after each parsed spot is added to the cluster table. `MainWindow`
connects this to `BandMapWidget::addOrUpdateSpot(const SpotData &spot)`.

See `contracts/signals.md` for full interface specification.

### 3. BandMapWidget Architecture

```
BandMapWidget (QDockWidget)
└── BandMapCanvas (QWidget) - overrides paintEvent(), mousePressEvent(),
                               wheelEvent(), mouseMoveEvent()
    Toolbar row (QHBoxLayout):
    ├── QLabel "Band Map"
    ├── QSlider (zoom) - horizontal, range 1-20 (1=full band, 20=narrow)
    └── QLabel showing visible range (e.g., "14.000-14.070")
```

**Rendering (paintEvent)**:
1. Map `m_visibleMinMhz`→`m_visibleMaxMhz` to widget pixel width
2. Draw frequency axis (tick marks at 10 kHz intervals)
3. For each spot in `m_spots` whose frequency is within visible range:
   - Compute x-pixel: `(freqMhz - visMin) / (visMax - visMin) * width`
   - Draw marker rectangle; fill color from status
   - Draw callsign label above marker; truncate if overlapping
4. Draw rig-frequency indicator line (VFO position) if rig connected

**Status colors** (configurable in future; hardcoded for now):
- NewMultiplier: `#FF6B00` (orange-red - draws attention)
- Worked: `#505050` (dark gray - muted)
- UnworkedNonMult: `#1E90FF` (dodger blue - neutral, visible)
- Rig VFO line: `#00FF00` (green)

### 4. Band Change Detection

`MainWindow` already polls rig frequency every 500ms for QSO entry. The existing
`onRigFrequencyChanged(double freqMhz)` handler (or equivalent) will be extended:
- Derive band from `ContestEngine::getAllowedBands()` and the contest `frequencies`
  JSON object (find which band's min/max contains `freqMhz`)
- If the derived band differs from `m_currentBand`, call:
  - `bandMapWidget->setBandRange(double minMhz, double maxMhz)`
  - `bandMapWidget->setCurrentBand(QString band)` (for display)
  - Reset zoom/pan to full-band default for the new band

### 5. Contact Status Query

`BandMapWidget` does not query `ContestEngine` directly. Status is resolved by
`MainWindow` and passed in:

```cpp
// In MainWindow - called when a spot arrives:
SpotData resolved = spot;
resolved.status = resolveSpotStatus(spot.callsign);
bandMapWidget->addOrUpdateSpot(resolved);

// Called on every log change (QSO logged/edited/deleted):
void MainWindow::onLogChanged() {
    bandMapWidget->refreshAllStatuses(
        [this](const QString &call) { return resolveSpotStatus(call); }
    );
}
```

`resolveSpotStatus()` queries `ContestEngine::isMultiplier()` and the worked-calls
set. This keeps `BandMapWidget` decoupled from `ContestEngine`.

### 6. Spot Expiry

A `QTimer` in `BandMapWidget` fires every 60 seconds:
```cpp
void BandMapWidget::onExpiryTimer() {
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-m_expirySeconds);
    bool changed = false;
    for (auto it = m_spots.begin(); it != m_spots.end(); ) {
        if (it->timestamp < cutoff) { it = m_spots.erase(it); changed = true; }
        else ++it;
    }
    if (changed) update(); // trigger repaint
}
```
Default `m_expirySeconds = 1800` (30 minutes). Configurable via QSettings key
`BandMap/ExpiryMinutes`.

### 7. Zoom and Pan

**State**: `double m_visibleMinMhz, m_visibleMaxMhz` (current viewport).
Initialized to full band range on band change.

**Scroll wheel**: `wheelEvent()` - zoom in/out centered on cursor frequency.
```
zoomFactor = (delta > 0) ? 0.8 : 1.25
center = pixelToFreq(event.x())
newRange = (visMax - visMin) * zoomFactor
m_visibleMinMhz = center - newRange/2  (clamped to band edges)
m_visibleMaxMhz = center + newRange/2
```

**Slider**: `QSlider::valueChanged` maps slider value (1-20) to a range fraction:
`rangeWidth = fullBandWidth / sliderValue`. Centered on current viewport center.

**Pan**: `mouseMoveEvent()` with left button held - drag shifts viewport by pixel
delta converted to MHz offset. Clamped to band edges.

**Persistence**: `QSettings` saves `BandMap/ZoomMin` and `BandMap/ZoomMax` on
dock close and app quit; restored on startup.

### 8. Tooltip

`BandMapCanvas` overrides `mouseMoveEvent()` - on hover over a spot marker (within
±3 pixels of marker center), call `QToolTip::showText()` with:
```
W1AW
14.025.0 kHz · CW
Spotter: K1TTT
Age: 12 min
```

### 9. Click-to-QSY

`mousePressEvent()` - on left click, find nearest spot within ±5 pixel tolerance:
```cpp
emit spotClicked(spot.freqMhz, spot.mode);
```
`MainWindow` connects `spotClicked` to the existing `qsyToFrequency(double, QString)`
method (the same one the DX cluster table row-click uses). No new rig control logic.

### 10. State Persistence

`BandMapWidget::objectName()` = `"BandMapWidget"` - required for
`QMainWindow::saveState()`/`restoreState()`.

Dock visibility and position: `QMainWindow::saveState()` (already called on app
quit). Zoom/pan: separate `QSettings` keys (dock state does not preserve custom
widget internals).

## Implementation Sequence

Phase ordering matches user story priorities (P1→P5):

1. **Foundation** - `SpotData` struct, `DxClusterPanel::spotReceived` signal, dedup
   key logic, `ContestEngine` band-range query helper. Unit tests for dedup key
   and expiry logic.
2. **US1 - Spot Display** - `BandMapWidget` shell, `BandMapCanvas::paintEvent()`,
   frequency axis, spot markers, callsign labels. Wire into `MainWindow`, Window menu.
3. **US2 - Color Coding** - `ContactStatus` enum, `resolveSpotStatus()` in
   `MainWindow`, pass to `BandMapWidget`, `refreshAllStatuses()` on log change.
4. **US3 - Click-to-QSY** - `spotClicked` signal, `mousePressEvent()` hit testing,
   `MainWindow` connection to existing QSY path.
5. **US4 - Expiry** - `QTimer`, `onExpiryTimer()`, QSettings for threshold.
6. **US5 - Zoom/Pan** - `wheelEvent()`, `QSlider`, `mouseMoveEvent()` pan,
   QSettings persistence.
7. **Polish** - Tooltip, empty states, rig VFO line, Window menu entry, dock state
   restore, HiDPI verification.
