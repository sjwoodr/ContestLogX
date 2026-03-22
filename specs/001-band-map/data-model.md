# Data Model: Visual Band Map

**Branch**: `001-band-map` | **Date**: 2026-03-21

---

## Entities

### SpotData

Represents a single DX cluster spot as received and stored by the band map.

| Field | Type | Description |
|-------|------|-------------|
| `callsign` | `QString` | Spotted station's callsign (e.g., "W1AW") |
| `freqMhz` | `double` | Spotted frequency in MHz (e.g., 14.025) |
| `mode` | `QString` | Operating mode: "CW", "SSB", "FT8", "RTTY", etc. |
| `spotter` | `QString` | Callsign of the spotting station |
| `timestamp` | `QDateTime` | UTC date/time when the spot was received locally |
| `status` | `ContactStatus` | Derived contact status (see enum below) |

**Identity rule**: Two spots are considered the same if their callsign matches
exactly and their frequencies are within 0.1 kHz of each other. When a duplicate
arrives, the existing entry's `timestamp` is reset; all other fields are updated
to the new report's values.

**Dedup key**: `callsign + "|" + QString::number(qRound(freqMhz * 10000))`
(rounds to nearest 0.1 kHz before comparison).

**Lifecycle**:
```
Arrived → Stored (status evaluated) → [status updated on log changes]
                                     → Expired (age > threshold) → Removed
```

---

### ContactStatus (enum)

Classifies a spot relative to the operator's current contest log.

| Value | Meaning | Display Color |
|-------|---------|---------------|
| `NewMultiplier` | Working this station would add a scoring multiplier | Orange-red `#FF6B00` |
| `Worked` | Station already logged in the current contest session | Dark gray `#505050` |
| `UnworkedNonMult` | Not yet logged; does not add a multiplier | Dodger blue `#1E90FF` |
| `Unknown` | Status not yet resolved (transient; not displayed) | Gray |

**Re-evaluation triggers**:
- When the spot first arrives (evaluated before storage)
- When any QSO is logged, edited, or deleted (all visible spots re-evaluated)
- When a new contest file is loaded (all spots cleared and re-evaluated from scratch)

---

### BandRange

Defines the frequency boundaries for the active contest band, sourced from the
contest JSON.

| Field | Type | Description |
|-------|------|-------------|
| `band` | `QString` | Band name as used in contest JSON (e.g., "20m", "40m") |
| `minMhz` | `double` | Lower bound of the band segment in MHz |
| `maxMhz` | `double` | Upper bound of the band segment in MHz |

**Source**: `ContestEngine::getBandFrequencyRange(QString band)` — reads the
`frequencies[band].min` and `frequencies[band].max` fields from the loaded
contest JSON.

**Default**: If no contest is loaded or the current frequency falls outside all
known contest bands, `BandRange` is `{band: "", minMhz: 0, maxMhz: 0}` and the
band map shows the "No contest loaded" empty state.

---

### ViewState

Internal state of the `BandMapWidget` viewport (zoom and pan). Persisted to
`QSettings`.

| Field | Type | Description | Default |
|-------|------|-------------|---------|
| `visibleMinMhz` | `double` | Left edge of visible frequency window | `bandRange.minMhz` |
| `visibleMaxMhz` | `double` | Right edge of visible frequency window | `bandRange.maxMhz` |

**Constraints**:
- `visibleMinMhz` ≥ `bandRange.minMhz`
- `visibleMaxMhz` ≤ `bandRange.maxMhz`
- `visibleMaxMhz - visibleMinMhz` ≥ 5 kHz (minimum zoom — prevents degenerate state)
- Reset to full band range when band changes

**QSettings keys**:
- `BandMap/ZoomMin` — `visibleMinMhz` (stored as double)
- `BandMap/ZoomMax` — `visibleMaxMhz` (stored as double)
- `BandMap/ExpiryMinutes` — spot expiry threshold (default: 30)

---

## Storage Structure

```
BandMapWidget
├── m_spots: QHash<QString, SpotData>   // keyed by dedup key
├── m_bandRange: BandRange              // current contest band boundaries
├── m_viewState: ViewState              // zoom/pan viewport
├── m_expirySeconds: int                // loaded from QSettings
└── m_expiryTimer: QTimer*             // fires every 60s
```

**Spot count expectations**:
- Typical contest: 10–100 spots per band
- Peak (major DX contest, good band opening): up to 500 spots
- `QHash` lookup and insert: O(1) — acceptable at all expected scales

---

## State Transitions

```
[No Contest]  ──load contest──►  [Contest Loaded, Band Known]
                                       │
                          rig freq change to different band
                                       │
                                       ▼
                              [Band Change] ──► clear view state,
                                               reload band range,
                                               filter spots to new band

[Spot Arrives] ──dedup check──► [Existing? Update timestamp]
                              └► [New? Insert, evaluate status]
                                       │
                              log change event
                                       │
                                       ▼
                              [Re-evaluate all spot statuses]

[Expiry Timer] ──scan──► [Remove spots older than threshold]
                                       │
                              [Repaint if any removed]
```
