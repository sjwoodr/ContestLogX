# Research Notes: Online Score Publishing

## Phase 0 Research Findings

### Decision 1: HTTP Client Pattern
**Decision**: Follow the existing QrzApi pattern - dedicated QObject class with QNetworkAccessManager, async signal/slot completion.
**Rationale**: QrzApi already demonstrates the exact pattern needed (HTTPS POST, credential management, XML parsing, error handling). Reusing this approach ensures consistency and leverages proven code patterns.
**Alternatives considered**: Direct QNetworkAccessManager usage in MainWindow (rejected: mixes concerns), synchronous blocking calls (rejected: blocks UI).

### Decision 2: Credential Storage
**Decision**: Use the existing XOR cipher + Base64 pattern from QRZ.com credentials, stored under `m_settings["onlineScoring"]`.
**Rationale**: Consistent with existing credential storage. While XOR is not cryptographically strong, it matches the established pattern and the threat model is local config file access only.
**Alternatives considered**: Plaintext (rejected: worse than existing pattern), OS keychain integration (rejected: adds platform-specific complexity for minimal benefit in this context).

### Decision 3: XML Generation
**Decision**: Use QXmlStreamWriter to build the dynamicresults XML document.
**Rationale**: Qt6::Xml is already a project dependency. QXmlStreamWriter produces well-formed XML with proper escaping, unlike string concatenation.
**Alternatives considered**: String concatenation (rejected: error-prone, no escaping), QDomDocument (rejected: heavier than needed for write-only XML).

### Decision 4: Timer Architecture
**Decision**: Single QTimer in MainWindow for periodic posting, plus a QTimer::singleShot for per-QSO debounce.
**Rationale**: Matches existing timer patterns (m_rigPollTimer, m_dupeFlashTimer). Timer owned by MainWindow, started/stopped via Contest menu toggle.
**Alternatives considered**: Separate thread (rejected: QNetworkAccessManager is already async), worker object (rejected: unnecessary complexity for a simple timer + HTTP POST).

### Decision 5: Station Info Extension
**Decision**: Add cqZone, ituZone, and arrlSection fields to existing StationInfo class. Auto-populate cqZone and ituZone from DxccDatabase when callsign changes.
**Rationale**: StationInfo already has the serialization pattern (toJson/fromJson) and the session vs default distinction. Adding fields is a minimal, backward-compatible change.
**Alternatives considered**: Separate QTH config object (rejected: fragments station data across two places).

### Decision 6: Band/Mode Score Breakdown Access
**Decision**: Use existing ContestEngine::BandModeStats from getRunningScore().bandStats, which already provides per-band CW/SSB/digital QSO counts and points. For multipliers per band, iterate QSOs and call getMultipliersWithCategory() per QSO grouped by band.
**Rationale**: BandModeStats covers QSO counts and points. Multiplier breakdown per band requires the same iteration pattern already used in the summary sheet generator.
**Alternatives considered**: New engine method for full breakdown (rejected: unnecessary API expansion when data is accessible via existing methods).

### Decision 7: Operating Class Mapping
**Decision**: Add an `onlineScoreClassMapping` section to contest definitions that maps userPrompt IDs and values to the XML class element attributes. For contests without explicit mapping, derive from common prompt ID patterns (powerCategory → power, operatingCategory → ops/transmitter/bands).
**Rationale**: Contest definitions already contain all the source data via userPrompts. A mapping section in the JSON avoids hardcoding contest-specific logic in C++.
**Alternatives considered**: Hardcoded mapping table in C++ (rejected: not extensible), infer entirely from prompt values (rejected: too fragile, prompt value formats vary).
