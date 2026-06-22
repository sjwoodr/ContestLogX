# Implementation Plan: Online Score Publishing

**Feature**: SPEC-003 Online Score Publishing
**Branch**: `sw/0.7.0`
**Spec**: [spec.md](spec.md)

---

## Technical Context

| Aspect | Details |
|--------|---------|
| Language | C++17 |
| Framework | Qt6 (Core, Widgets, Network, Xml) |
| Build | CMake 3.16+ |
| HTTP Client | QNetworkAccessManager (async, already used by QrzApi) |
| XML Writer | QXmlStreamWriter (Qt6::Xml) |
| Settings Storage | JSON file with XOR+Base64 credential encoding |
| Existing Patterns | QrzApi (HTTP+auth), Settings (credential storage), QTimer (polling) |

---

## Constitution Check

| Gate | Status | Notes |
|------|--------|-------|
| No new dependencies | PASS | Uses existing Qt6::Network and Qt6::Xml |
| C++17 + Qt6 only | PASS | No third-party libraries added |
| In-memory data structures | PASS | ScorePostData is transient, built per posting cycle |
| Async UI operations | PASS | QNetworkAccessManager is non-blocking |

---

## Phase 1: Station Info Extension [FR-002]

**Goal**: Add CQ zone, ITU zone, and ARRL section to StationInfo.

**Changes**:
- `include/stationInfo.h` - Add `int m_cqZone`, `int m_ituZone`, `QString m_arrlSection` with getters/setters
- `src/core/stationInfo.cpp` - Add to `toJson()` and `fromJson()` serialization
- `src/ui/mainWindow.cpp` - Auto-populate cqZone/ituZone from DxccDatabase when callsign is set in station dialog

**Validation**: Load a saved CLX file with old station info format → new fields default to 0/"". Save and reload → new fields persist.

---

## Phase 2: Settings Extension [FR-001]

**Goal**: Add online scoring credentials and interval to application settings.

**Changes**:
- `include/settings.h` - Add getters/setters for online scoring callsign, password, interval, perQso flag
- `src/utils/settings.cpp` - Implement under `m_settings["onlineScoring"]` with XOR+Base64 password encoding (same as QRZ pattern)
- `src/ui/preferencesDialog.cpp` - Add "Online Scoring" section with callsign, password, interval dropdown, per-QSO checkbox

**Validation**: Set credentials in preferences, restart app, verify retained. Change interval, verify saved.

---

## Phase 3: OnlineScoreClient Class [FR-004, FR-008]

**Goal**: New class that generates XML and posts to the server.

**New files**:
- `include/onlineScoreClient.h`
- `src/net/onlineScoreClient.cpp`

**Class design**:
```
class OnlineScoreClient : public QObject
    Signals: postSuccess(QString timestamp), postFailed(QString error), authFailed()
    Slots: postScore(ScorePostData)
    Private: QNetworkAccessManager, buildXml(), onReplyFinished()
```

**Responsibilities**:
- Build dynamicresults XML from ScorePostData using QXmlStreamWriter
- HTTP POST with Basic Auth header
- Parse JSON response (status 200 = success, 404/405 = error)
- Emit signals for MainWindow to update status bar
- Track consecutive auth failure count

**Validation**: Unit test XML generation with known input → verify XML matches expected format. Integration test with actual server (manual).

---

## Phase 4: Contest Menu Toggle & Validation Gate [FR-003]

**Goal**: Add enable/disable toggle to Contest menu with field validation.

**Changes**:
- `include/mainWindow.h` - Add `QAction *m_onlineScoringAction`, `OnlineScoreClient *m_onlineScoreClient`, `QTimer *m_scorePostTimer`, slots
- `src/ui/mainWindow.cpp`:
  - Add checkable menu action to Contest menu
  - `onToggleOnlineScoring()` - validate required fields, show warning if missing, start/stop timer
  - Required fields: callsign, password, cqZone, ituZone, state, grid, contestOnlineScore block in contest def
  - Disable on log file close

**Validation**: Try to enable with missing CQ zone → dialog lists "CQ Zone". Fill all fields → enables successfully. Close log → auto-disables.

---

## Phase 5: Posting Timer & Per-QSO Trigger [FR-010]

**Goal**: Implement timer-based and per-QSO posting modes.

**Changes**:
- `src/ui/mainWindow.cpp`:
  - Timer mode: `m_scorePostTimer` fires at configured interval, calls `onPostScore()`
  - Per-QSO mode: in `onLogQso()`, trigger `QTimer::singleShot(2000, ...)` debounced post
  - `onPostScore()` builds ScorePostData from engine/station/contest, calls client
  - Immediate post on enable (per clarification)

**Data gathering in onPostScore()**:
- Contest ID from `m_contestDefinition["contestOnlineScore"]`
- Score from `m_contestEngine->getRunningScore()`
- Band breakdown from `score.bandStats`
- Multipliers from QSO iteration + `getMultipliersWithCategory()`
- Station info from `m_sessionStationInfo`
- Class attributes derived from userPrompt values

---

## Phase 6: Status Bar Feedback [FR-009]

**Goal**: Show posting status in the status bar.

**Changes**:
- `include/mainWindow.h` - Add `QLabel *m_onlineScoringLabel`
- `src/ui/mainWindow.cpp`:
  - Add label to status bar (between existing labels)
  - Connect to OnlineScoreClient signals:
    - `postSuccess` → "Score: HH:MM UTC"
    - `postFailed` → "Score: Error" (red)
    - `authFailed` → auto-disable + dialog
  - Hidden when online scoring is disabled

---

## Phase 7: Operating Class Derivation [FR-006]

**Goal**: Map userPrompt values to XML class element attributes.

**Changes**:
- `src/net/onlineScoreClient.cpp` - ClassAttributes derivation logic:
  - Check for `powerCategory` prompt → map HP/LP/QRP to HIGH/LOW/QRP
  - Check for `operatingCategory` prompt → extract ops (SINGLE-OP/MULTI-OP) and transmitter (ONE/TWO/UNLIMITED)
  - Check for `contestMode` prompt → map to CW/PH/MIXED/etc.
  - Mode mapping: SSB→PH, RTTY→RY, FT8/FT4/PSK→DG
  - Defaults for missing prompts: HIGH, NON-ASSISTED, ONE, SINGLE-OP, ALL, MIXED, N/A

---

## Phase 8: Contest Definition Updates [FR-005, FR-005a]

**Goal**: Add contestOnlineScore blocks to 10 eligible contest definitions.

**Files**: All in `contests/` directory:
- `arrl_10m.json` - contestId: "ARRL-10", country + state
- `arrl_dx.json` - contestIdMapping by mode (ARRL-DX-CW / ARRL-DX-SSB), country
- `arrl_vhf.json` - contestId: "ARRL-VHF", gridsquare
- `cwops_cwt.json` - contestId: "CW-Ops", state
- `eudx.json` - contestId: "EUDXC", country + state
- `fqp.json` - contestId: "FL-QSO-PARTY", state
- `mnqp.json` - contestId: "MN-QSO-PARTY", state
- `naqp.json` - contestIdMapping by mode (NAQP-CW / NAQP-SSB / NAQP-RTTY), state
- `vaqp.json` - contestId: "VA-QSO-PARTY", state
- `winter_field_day.json` - contestId: "WFDA-CONTEST", wpxprefix

**Validation**: `make test-logs` passes - contest definition changes don't affect scoring.

---

## Implementation Order

1. Station Info extension (Phase 1) - no dependencies
2. Settings extension (Phase 2) - no dependencies
3. OnlineScoreClient class (Phase 3) - needs Qt6::Network/Xml (already linked)
4. Contest menu toggle (Phase 4) - needs Phase 2 + 3
5. Posting timer & trigger (Phase 5) - needs Phase 3 + 4
6. Status bar feedback (Phase 6) - needs Phase 3 + 4
7. Operating class derivation (Phase 7) - needs Phase 3
8. Contest definition updates (Phase 8) - independent, can be done anytime

Phases 1, 2, 3, and 8 can be done in parallel. Phases 4-7 depend on Phase 3.

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Server rejects XML format | Medium | High | Test with actual account early; match examples exactly |
| Mode-dependent contest IDs break | Low | Medium | contestIdMapping handles this explicitly |
| Auth failure loop annoys operator | Low | Medium | Auto-disable after 3 failures (per clarification) |
| Timer fires during scoring recalculation | Low | Low | Post uses snapshot of current score; engine is thread-safe for reads |
| Network latency blocks UI | Very Low | High | QNetworkAccessManager is fully async |
