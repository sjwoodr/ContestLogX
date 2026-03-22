# Signal Interface Contracts: Visual Band Map

**Branch**: `001-band-map` | **Date**: 2026-03-21

Qt signals/slots are the internal interface mechanism for this feature.
These contracts define what signals are added, who emits them, and who connects.

---

## New Signal: DxClusterPanel → BandMapWidget

### `DxClusterPanel::spotReceived`

```cpp
// In DxClusterPanel (include/dxClusterPanel.h)
signals:
    void spotReceived(const SpotData &spot);
```

**Emitted by**: `DxClusterPanel` — after parsing a new incoming cluster spot and
adding it to the cluster table.

**Payload**:
| Field | Type | Notes |
|-------|------|-------|
| `spot.callsign` | `QString` | Spotted callsign, uppercase |
| `spot.freqMhz` | `double` | Frequency in MHz, as parsed from cluster message |
| `spot.mode` | `QString` | Mode string: "CW", "SSB", "FT8", etc.; empty if not provided |
| `spot.spotter` | `QString` | Spotter callsign |
| `spot.timestamp` | `QDateTime` | UTC timestamp of local receipt |
| `spot.status` | `ContactStatus` | Set to `Unknown` by DxClusterPanel; resolved by MainWindow |

**Connected by**: `MainWindow` in `createConnections()`:
```cpp
connect(m_dxClusterPanel, &DxClusterPanel::spotReceived,
        this, &MainWindow::onSpotReceived);
```

**MainWindow handler** (`onSpotReceived`):
1. Resolves `spot.status` via `resolveSpotStatus(spot.callsign)`
2. Calls `m_bandMapWidget->addOrUpdateSpot(spot)` if band map is visible

---

## New Signal: BandMapWidget → MainWindow

### `BandMapWidget::spotClicked`

```cpp
// In BandMapWidget (include/bandMapWidget.h)
signals:
    void spotClicked(double freqMhz, const QString &mode);
```

**Emitted by**: `BandMapCanvas::mousePressEvent()` when the operator left-clicks
within ±5 pixels of a spot marker.

**Payload**:
| Field | Type | Notes |
|-------|------|-------|
| `freqMhz` | `double` | Frequency to QSY to |
| `mode` | `QString` | Mode to switch to ("CW", "SSB", etc.) |

**Connected by**: `MainWindow` in `createConnections()`:
```cpp
connect(m_bandMapWidget, &BandMapWidget::spotClicked,
        this, &MainWindow::qsyToFrequency);  // existing method signature match
```

**Behavior if no radio connected**: `MainWindow::qsyToFrequency()` already handles
the no-rig case by showing a status bar message. No additional handling needed in
`BandMapWidget`.

---

## New Public Slots: BandMapWidget

### `addOrUpdateSpot(const SpotData &spot)`

Adds a new spot or updates an existing spot's timestamp if the dedup key matches.
Triggers a repaint.

### `refreshAllStatuses(std::function<ContactStatus(QString)> resolver)`

Re-evaluates the `status` field of every spot in `m_spots` by calling `resolver`
for each callsign. Called by `MainWindow::onLogChanged()`. Triggers a repaint if
any status changed.

### `setBandRange(double minMhz, double maxMhz, const QString &band)`

Sets the visible band range. Resets viewport to full band. Clears spots that fall
outside the new band range. Called by `MainWindow` when rig frequency crosses a
band boundary.

### `clearAllSpots()`

Removes all spots from `m_spots` and triggers a repaint. Called by `MainWindow`
when a new contest file is loaded.

---

## Modified: DxClusterPanel internal parsing

**File**: `src/ui/dxClusterPanel.cpp`

The existing spot-parsing code (which currently populates the table directly) is
refactored to:
1. Parse the raw cluster line into a `SpotData` struct
2. Populate the table row (existing behavior preserved)
3. Emit `spotReceived(spot)` (new)

No behavior change visible to the DX cluster panel or cluster connection. Backward
compatible — existing connections to `DxClusterPanel` are unaffected.

---

## MainWindow connections summary

| Signal | Source | Slot | Notes |
|--------|--------|------|-------|
| `spotReceived(SpotData)` | `DxClusterPanel` | `MainWindow::onSpotReceived` | Resolves status, forwards to band map |
| `spotClicked(double, QString)` | `BandMapWidget` | `MainWindow::qsyToFrequency` | Existing method |
| `logChanged()` (existing) | `MainWindow` internal | `MainWindow::onLogChanged` → `bandMapWidget->refreshAllStatuses()` | Extend existing log-change event |
| Rig frequency change (existing) | Rig polling timer | `MainWindow::onRigFrequencyChanged` → `bandMapWidget->setBandRange()` | Extend existing handler |
