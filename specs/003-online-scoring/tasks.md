# Tasks: Online Score Publishing

**Feature**: SPEC-003 Online Score Publishing
**Branch**: `sw/0.7.0`
**Total tasks**: 32
**User stories**: 7 (US1-US7)

---

## Phase 1: Setup

- [x] T001 Add `src/net/` directory and update `CMakeLists.txt` with new source files `src/net/onlineScoreClient.cpp` and `include/onlineScoreClient.h`

---

## Phase 2: Foundational (blocking prerequisites)

### Station Info Extension [FR-002]

- [x] T002 [P] Add `m_cqZone` (int), `m_ituZone` (int), `m_arrlSection` (QString) fields with getters/setters to `include/stationInfo.h`
- [x] T003 [P] Implement `toJson()` and `fromJson()` serialization for new fields in `src/core/stationInfo.cpp` - add to `location` object, default cqZone/ituZone to 0, arrlSection to ""
- [x] T004 Add CQ zone and ITU zone auto-population from DxccDatabase callsign lookup in `src/ui/mainWindow.cpp` station dialog handler

### Settings Extension [FR-001]

- [x] T005 [P] Add online scoring getters/setters to `include/settings.h`: `getOnlineScoringCallsign()`, `getOnlineScoringPassword()`, `setOnlineScoringCredentials()`, `getOnlineScoringInterval()`, `setOnlineScoringInterval()`, `getOnlineScoringPerQso()`, `setOnlineScoringPerQso()`
- [x] T006 [P] Implement online scoring settings in `src/utils/settings.cpp` under `m_settings["onlineScoring"]` with XOR+Base64 password encoding (follow QRZ credential pattern)

---

## Phase 3: US1 - Configure Online Scoring Credentials [P1]

**Story goal**: Operator configures credentials and posting interval in Settings/Preferences.
**Independent test**: Open preferences, enter callsign/password/interval, restart app, verify retained.

- [x] T007 [US1] Add "Online Scoring" section to preferences dialog in `src/ui/preferencesDialog.cpp` with fields: callsign (QLineEdit), password (QLineEdit, echoMode Password), interval dropdown (1/2/5/10/15 min), per-QSO checkbox
- [x] T008 [US1] Connect preferences dialog fields to Settings getters/setters - load on open, save on accept in `src/ui/preferencesDialog.cpp`
- [x] T009 [US1] Add station info fields (CQ Zone, ITU Zone, ARRL Section) to the station information dialog in `src/ui/mainWindow.cpp`

---

## Phase 4: US2 - Enable Online Scoring for a Contest Session [P1]

**Story goal**: Operator enables/disables online scoring from Contest menu with validation.
**Independent test**: Enable with missing fields → warning dialog. Enable with all fields → activates. Close log → auto-disables.

- [x] T010 [US2] Add checkable "Online Score Publishing" action to Contest menu in `src/ui/mainWindow.cpp` setupMenus(), grayed out when no `contestOnlineScore` block in contest definition
- [x] T011 [US2] Implement `onToggleOnlineScoring()` slot in `src/ui/mainWindow.cpp` - validate required fields (callsign, password, cqZone, ituZone, state, grid, contestOnlineScore), show warning dialog listing missing fields if any
- [x] T012 [US2] Add auto-disable on log file close and contest switch in `src/ui/mainWindow.cpp` - uncheck menu action, stop timer, reset state

---

## Phase 5: US3 - Automatic Score Posting on Timer [P1]

**Story goal**: System posts scores at configured interval without blocking UI.
**Independent test**: Enable with 1-min interval, log QSOs, verify post occurs and status bar updates.

- [x] T013 [US3] Create `OnlineScoreClient` class in `include/onlineScoreClient.h` - QObject with signals: `postSuccess(QString)`, `postFailed(QString)`, `authFailed()`, and slot: `postScore()`
- [x] T014 [US3] Implement `buildXml()` in `src/net/onlineScoreClient.cpp` - generate dynamicresults XML using QXmlStreamWriter with contest ID, call, ops, soft, version, class, club, qth, breakdown, score, timestamp elements
- [x] T015 [US3] Implement HTTP POST with Basic Auth in `src/net/onlineScoreClient.cpp` - QNetworkAccessManager, 15-second timeout, Content-Type application/xml
- [x] T016 [US3] Implement `onReplyFinished()` in `src/net/onlineScoreClient.cpp` - parse JSON response, emit postSuccess/postFailed/authFailed signals, track consecutive auth failure count (reset on success)
- [x] T017 [US3] Add `QTimer *m_scorePostTimer` and `OnlineScoreClient *m_onlineScoreClient` to `include/mainWindow.h`, initialize in `src/ui/mainWindow.cpp` constructor
- [x] T018 [US3] Implement `onPostScore()` in `src/ui/mainWindow.cpp` - build ScorePostData from contest engine score, band breakdown, station info, contest definition, userPrompt values; skip if post already in-flight
- [x] T019 [US3] Start timer on enable (post immediately first), stop on disable in `src/ui/mainWindow.cpp` - connect timer timeout to `onPostScore()`
- [x] T020 [US3] Implement per-QSO posting mode in `src/ui/mainWindow.cpp` `onLogQso()` - QTimer::singleShot(2000) debounced, reset on each new QSO

---

## Phase 6: US4 - Score Breakdown in Posted Data [P1]

**Story goal**: Posted XML includes per-band/mode QSO counts, points, and multipliers.
**Independent test**: Log QSOs on multiple bands/modes, capture outgoing XML, verify breakdown matches log.

- [x] T021 [US4] Implement band/mode breakdown generation in `src/net/onlineScoreClient.cpp` `buildXml()` - iterate BandModeStats for QSO counts and points per band/mode, map modes (SSB→PH, RTTY→RY, DIGI→DG)
- [x] T022 [US4] Implement multiplier breakdown in `src/net/onlineScoreClient.cpp` `buildXml()` - use contestOnlineScore mult1Attribute/mult2Attribute to emit `<mult>` elements per band with correct type attribute
- [x] T023 [US4] Add totals row to breakdown in `src/net/onlineScoreClient.cpp` `buildXml()` - `<qso band="total" mode="ALL">`, `<point band="total" mode="ALL">`, `<mult band="total" mode="ALL" type="...">` for each mult type

---

## Phase 7: US5 - Station Information in Posted Data [P2]

**Story goal**: Posted XML includes QTH element with DXCC country, zones, section, state, grid.
**Independent test**: Configure station info, verify QTH section in posted XML contains all fields.

- [x] T024 [US5] Implement QTH element generation in `src/net/onlineScoreClient.cpp` `buildXml()` - dxcccountry from DXCC prefix lookup, cqzone, iaruzone, arrlsection (empty if unset), stprvoth, grid6/grid4 from station info

---

## Phase 8: US6 - Operating Category in Posted Data [P2]

**Story goal**: Posted XML class element reflects operator's contest category from userPrompts.
**Independent test**: Start contest as SO LP CW, verify class element has power="LOW" ops="SINGLE-OP" mode="CW".

- [x] T025 [US6] Implement class attributes derivation in `src/net/onlineScoreClient.cpp` - map powerCategory prompt (HP→HIGH, LP→LOW, QRP→QRP), operatingCategory prompt to ops/transmitter, contestMode to mode (SSB→PH, RTTY→RY), defaults for missing prompts
- [x] T026 [US6] Implement contestIdMapping resolution in `src/net/onlineScoreClient.cpp` - if contestOnlineScore.contestIdMapping exists, look up the mode prompt value to get the correct contest ID; fall back to base contestId

---

## Phase 9: US7 - Error Handling and Server Responses [P2]

**Story goal**: Operator sees clear error feedback and system auto-disables on auth failures.
**Independent test**: Use wrong password, verify error in status bar after first post. After 3 failures, verify auto-disable dialog.

- [x] T027 [US7] Add `QLabel *m_onlineScoringLabel` to status bar in `src/ui/mainWindow.cpp` - show/hide based on online scoring state, positioned after propagation label
- [x] T028 [US7] Connect OnlineScoreClient signals to status bar updates in `src/ui/mainWindow.cpp` - postSuccess→"Score: HH:MM UTC", postFailed→"Score: Error" (red text), authFailed→auto-disable + warning dialog
- [x] T029 [US7] Implement auto-disable on 3 consecutive auth failures in `src/ui/mainWindow.cpp` - stop timer, uncheck menu action, show QMessageBox warning to check credentials

---

## Phase 10: Contest Definition Updates [FR-005a]

- [x] T030 [P] Add `contestOnlineScore` blocks to 5 contest definitions: `contests/arrl_10m.json`, `contests/arrl_vhf.json`, `contests/cwops_cwt.json`, `contests/eudx.json`, `contests/fqp.json`
- [x] T031 [P] Add `contestOnlineScore` blocks to 5 contest definitions: `contests/mnqp.json`, `contests/naqp.json` (with contestIdMapping), `contests/vaqp.json`, `contests/winter_field_day.json`, `contests/arrl_dx.json` (with contestIdMapping)
- [x] T032 Run `make test-logs` to verify contest definition changes don't affect scoring

---

## Dependencies

```
T001 (setup) ──→ T013-T016 (OnlineScoreClient)
T002-T003 (station info) ──→ T009 (station dialog) ──→ T024 (QTH XML)
T005-T006 (settings) ──→ T007-T008 (preferences UI) ──→ T011 (validation gate)
T013-T016 (client) ──→ T017-T020 (timer/trigger)
T013-T016 (client) ──→ T027-T029 (status bar/errors)
T030-T031 (contest defs) ──→ T032 (test-logs validation)
```

## Parallel Opportunities

| Phase | Parallelizable tasks |
|-------|---------------------|
| Phase 2 | T002+T003 (station info) ∥ T005+T006 (settings) |
| Phase 10 | T030 ∥ T031 (contest definition batches) |

## Implementation Strategy

**MVP (minimum viable)**: Phases 1-5 (US1-US3) - credentials, enable/disable, timer posting with basic XML. This gets scores on the scoreboard.

**Incremental additions**:
- Phase 6 (US4): Band breakdown detail
- Phases 7-8 (US5-US6): QTH and category in XML
- Phase 9 (US7): Polish error handling and status bar
- Phase 10: Contest definition updates (can be done anytime)
