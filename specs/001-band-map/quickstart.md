# Developer Quickstart: Visual Band Map

**Branch**: `001-band-map` | **Date**: 2026-03-21

---

## Prerequisites

- Existing ContestLogX dev environment (`make` builds cleanly)
- flrig running for QSY testing (optional for rendering work)
- DX cluster connection for live spot testing (optional; spots can be injected
  manually for unit testing)

## Build

```bash
cd /home/steve/src/other/ContestLogX
git checkout 001-band-map
make
```

Zero warnings required before committing.

## Run Tests

```bash
make test          # unit tests - run after any logic change
make test-logs     # NOT required for this feature (no contest engine changes)
```

## Key Files

| File | Role |
|------|------|
| `include/bandMapWidget.h` | NEW - BandMapWidget + BandMapCanvas declarations |
| `src/ui/bandMapWidget.cpp` | NEW - Full implementation |
| `include/dxClusterPanel.h` | MODIFY - add `spotReceived(SpotData)` signal |
| `src/ui/dxClusterPanel.cpp` | MODIFY - emit signal after parsing each spot |
| `src/ui/mainWindow.cpp` | MODIFY - add dock, wire signals, extend rig + log handlers |
| `include/mainWindow.h` | MODIFY - `m_bandMapWidget` member, new slots |
| `tests/test_bandmap.cpp` | NEW - unit tests for SpotData logic |

## SpotData Struct Location

Defined in `include/bandMapWidget.h` - no separate header needed. If other
files need it, include `bandMapWidget.h`.

## Testing the Band Map Manually

1. Start ContestLogX with a contest loaded (e.g., NAQP CW)
2. Open Window → Band Map
3. Connect to a DX cluster (e.g., telnet to a test cluster or local NC7J/W6YX)
4. Verify spots appear on the frequency axis within ~2 seconds of arriving in
   the cluster panel
5. Hover over a spot - verify tooltip shows callsign, freq, mode, spotter, age
6. Click a spot - verify radio QSYs (requires flrig connected)
7. Log a QSO with a spotted station - verify spot color changes to Worked gray
8. Wait 2+ minutes with a short expiry setting - verify spots disappear
9. Use scroll wheel on the band map - verify zoom in/out
10. Drag on the band map - verify pan

## Injecting a Test Spot (without live cluster)

In `MainWindow` or a test harness, directly call:
```cpp
SpotData s;
s.callsign = "W1AW";
s.freqMhz = 14.025;
s.mode = "CW";
s.spotter = "K1TTT";
s.timestamp = QDateTime::currentDateTimeUtc();
s.status = ContactStatus::NewMultiplier;
m_bandMapWidget->addOrUpdateSpot(s);
```

## Common Issues

| Symptom | Likely Cause |
|---------|-------------|
| Band map empty despite cluster spots | `spotReceived` signal not connected, or `setBandRange` not called |
| Spots appear but wrong frequency position | Check `freqMhz` parsing in `DxClusterPanel` (MHz vs kHz) |
| QSY does nothing | `spotClicked` not connected to `qsyToFrequency`, or flrig not connected |
| Colors all gray | `resolveSpotStatus` returning `Unknown`; check ContestEngine call |
| Dock state not restored | `objectName()` not set on `BandMapWidget` |
