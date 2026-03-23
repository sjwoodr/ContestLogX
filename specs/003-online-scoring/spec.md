# Feature Specification: Online Score Publishing

**Feature Branch**: `003-online-scoring`
**Created**: 2026-03-23
**Status**: Draft
**Input**: Real-time contest score posting to contestonlinescore.com so operators and clubs can track live results on the public scoreboard

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Configure Online Scoring Credentials (Priority: P1)

A contest operator wants to set up their online scoring account before a contest begins.
They open Settings/Preferences and enter their contestonlinescore.com credentials
(callsign and password) along with their preferred posting interval. These settings
persist across sessions so they only need to configure once.

**Why this priority**: Without credentials configured, no score posting can happen.
This is the prerequisite for all other functionality.

**Independent Test**: Open Settings/Preferences, enter credentials and posting interval,
close and reopen the application, verify credentials are retained.

**Acceptance Scenarios**:

1. **Given** the operator opens Settings/Preferences, **When** they navigate to the
   online scoring section, **Then** they see fields for callsign, password, and
   posting interval.

2. **Given** the operator enters credentials and sets interval to 5 minutes, **When**
   they close and reopen the application, **Then** the credentials and interval are
   retained.

3. **Given** the operator wants per-QSO posting, **When** they select "After each QSO"
   as the posting interval, **Then** the timer-based interval field is disabled.

4. **Given** the operator has not configured credentials, **When** they try to enable
   online scoring from the Contest menu, **Then** they see a warning listing the
   missing fields and the feature is not enabled.

5. **Given** the operator wants to verify their setup before a contest, **When** they
   enable online scoring with no active contest loaded (or a test log), **Then** the
   system posts using the configured server URL — operators can test by pointing to
   an alternate URL or by using a test contest log.

---

### User Story 2 - Enable Online Scoring for a Contest Session (Priority: P1)

A contest operator has loaded a contest log and wants to start publishing scores to the
live scoreboard. They toggle the feature on from the Contest menu. The system validates
that all required station information is present before enabling.

**Why this priority**: This is the activation gate — the operator must be able to
turn publishing on/off during a contest and see clear feedback about what's missing.

**Independent Test**: Load a contest log with complete station info, toggle online
scoring on from the Contest menu, verify the status bar shows an indicator. Toggle
off and verify posting stops.

**Acceptance Scenarios**:

1. **Given** credentials are configured and all station info fields are set, **When**
   the operator enables online scoring from the Contest menu, **Then** the feature
   activates and the status bar shows a posting indicator.

2. **Given** the operator's station info is missing CQ zone, **When** they try to
   enable online scoring, **Then** a warning dialog lists "CQ Zone" as missing
   and the feature is not enabled.

3. **Given** online scoring is enabled, **When** the operator toggles it off from
   the Contest menu, **Then** posting stops immediately and the status bar indicator
   is removed.

4. **Given** online scoring is enabled, **When** the operator closes the log file,
   **Then** online scoring is automatically disabled for safety.

---

### User Story 3 - Automatic Score Posting on Timer (Priority: P1)

During an active contest with online scoring enabled, the system automatically posts
the current score to the server at the configured interval (default: every 5 minutes).
The operator continues logging QSOs without interruption — posting happens silently
in the background.

**Why this priority**: This is the core value — live scores appearing on the public
scoreboard without operator intervention. Background operation is critical so it
never interrupts the operator's workflow.

**Independent Test**: Enable online scoring with a 1-minute interval, log several QSOs,
verify that a score post occurs within 1 minute and the status bar updates with the
last successful post time.

**Acceptance Scenarios**:

1. **Given** the operator enables online scoring, **When** the feature activates,
   **Then** the system posts the current score immediately and updates the status bar.

2. **Given** online scoring is enabled with a 5-minute interval, **When** 5 minutes
   elapse after the last post, **Then** the system posts the current score and
   updates the status bar with the post time.

2. **Given** online scoring is enabled, **When** a post succeeds, **Then** the status
   bar shows the last successful post time (e.g., "Score posted: 14:35 UTC").

3. **Given** online scoring is enabled, **When** a post fails due to a network error,
   **Then** the status bar shows an error indicator and the system retries at the
   next interval.

4. **Given** online scoring is enabled with "after each QSO" mode, **When** the
   operator logs a QSO, **Then** the system posts the updated score after a
   2-second debounce (to coalesce rapid sequential QSOs).

5. **Given** online scoring is enabled, **When** the operator is actively logging
   QSOs during a posting cycle, **Then** the UI remains responsive with no lag or
   freezing.

---

### User Story 4 - Score Breakdown in Posted Data (Priority: P1)

The posted score includes a detailed band/mode breakdown showing QSO counts, points,
and multipliers per band — not just a total score. This allows the scoreboard to
display per-band performance and is required by the scoring server's format.

**Why this priority**: The scoring server requires the band/mode breakdown format.
Without it, posts would be rejected or incomplete.

**Independent Test**: Enable online scoring, log QSOs across multiple bands and modes,
capture the outgoing post and verify it contains per-band QSO counts, points, and
multipliers plus summary totals.

**Acceptance Scenarios**:

1. **Given** the operator has logged QSOs on 20m CW and 40m SSB, **When** a score
   post occurs, **Then** the posted data includes separate entries for each band/mode
   combination with correct QSO counts, points, and multiplier counts.

2. **Given** the operator has logged QSOs, **When** a score post occurs, **Then**
   the posted data includes a totals row summarizing all bands and modes.

3. **Given** the contest has multiple multiplier types (e.g., zones and countries),
   **When** a score post occurs, **Then** each multiplier type appears as a separate
   entry in the breakdown using the server-recognized type identifier.

---

### User Story 5 - Station Information in Posted Data (Priority: P2)

The posted score includes the operator's station location information (DXCC country,
CQ zone, ITU zone, ARRL section, state/province, grid square) so the scoreboard can
categorize entries geographically. These fields come from the station info settings.

**Why this priority**: Station location enables regional scoreboard filtering and is
part of the required posting format. New station info fields (CQ zone, ITU zone, ARRL
section) need to be added to the station configuration.

**Independent Test**: Configure complete station info including CQ zone and ITU zone,
enable online scoring, verify the posted data contains all QTH fields.

**Acceptance Scenarios**:

1. **Given** the operator's station info includes CQ zone 4, ITU zone 8, state FL,
   and grid EL96, **When** a score post occurs, **Then** the QTH section contains
   all four values plus the DXCC country derived from the operator's callsign.

2. **Given** the operator changes their callsign in station info, **When** a score
   post occurs, **Then** the DXCC country in the QTH section reflects the new
   callsign's entity.

3. **Given** the operator configures station info for the first time, **When** they
   enter their callsign, **Then** CQ zone and ITU zone are pre-populated from the
   DXCC database as a convenience (editable by the operator).

---

### User Story 6 - Operating Category in Posted Data (Priority: P2)

The posted score includes the operator's contest category (power level, single/multi
operator, assisted/non-assisted, band selection, mode) so the scoreboard places the
entry in the correct competition category. These values are derived from the contest
setup prompts the operator already answered.

**Why this priority**: Correct category placement on the scoreboard is essential for
fair comparison. This data already exists in userPrompts — it just needs mapping.

**Independent Test**: Start a contest as Single-Op Low Power CW, enable online scoring,
verify the posted data's class element contains power="LOW", ops="SINGLE-OP",
mode="CW".

**Acceptance Scenarios**:

1. **Given** the operator selected "Low Power" and "Single-Op All Band CW" during
   contest setup, **When** a score post occurs, **Then** the class element shows
   power="LOW", ops="SINGLE-OP", mode="CW", transmitter="ONE".

2. **Given** the contest definition does not include a power or category prompt,
   **When** a score post occurs, **Then** reasonable defaults are used (HIGH power,
   SINGLE-OP, MIXED mode, ONE transmitter, NON-ASSISTED, N/A overlay).

---

### User Story 7 - Error Handling and Server Responses (Priority: P2)

When the scoring server rejects a post (invalid contest, empty call, server down),
the operator sees clear feedback about what went wrong so they can take corrective
action or continue without online scoring.

**Why this priority**: Operators need to know if their scores aren't appearing on the
scoreboard, especially during a contest when time is limited.

**Independent Test**: Configure an invalid password, enable online scoring, verify
that the status bar shows an error after the first post attempt. Correct the password
and verify the next post succeeds.

**Acceptance Scenarios**:

1. **Given** the scoring server returns "Contest is closed or not valid", **When** the
   operator views the status bar, **Then** they see the error message and posting
   continues to retry at the configured interval.

2. **Given** the network is unavailable, **When** a post attempt fails, **Then** the
   system logs the error, shows a brief status bar indicator, and retries at the
   next interval without interrupting the operator.

3. **Given** a post succeeds after a previous failure, **When** the status bar updates,
   **Then** the error indicator is cleared and replaced with the successful post time.

4. **Given** the operator entered an incorrect password, **When** 3 consecutive posts
   fail with authentication errors, **Then** the system auto-disables online scoring
   and shows a dialog telling the operator to check their credentials. The operator
   can re-enable by toggling the Contest menu item again after correcting their
   credentials in Settings.

---

## Clarifications

### Session 2026-03-23
- Q: When online scoring is enabled, should the system post immediately or wait for the first interval? → A: Post immediately on enable, then continue at configured interval.
- Q: How does test mode differ from live mode? → A: No separate test mode. Server URL is fixed to https://contestonlinescore.com/post/. Testing done via a test contest log.
- Q: Should the system auto-disable after repeated authentication failures? → A: Auto-disable after 3 consecutive auth failures with a dialog prompting the operator to check credentials.
- Q: Should the "online scoring enabled" state persist in the CLX log file? → A: Never persist — operator must re-enable each session.

---

## Functional Requirements *(mandatory)*

### FR-001: Online Scoring Settings
The application must provide settings for online scoring configuration including:
callsign (username for authentication), password, and posting interval. The posting interval must support both time-based values (1, 2, 5, 10, 15
minutes) and an "after each QSO" option. These settings persist across application
sessions. The feature is disabled by default.

### FR-001a: Server URL
The server URL is fixed to `https://contestonlinescore.com/post/`. There is no separate
test mode — operators can verify their setup by enabling online scoring with a test
contest log.

### FR-002: Station Info Fields
The station information (both default and per-session) must include CQ zone, ITU zone,
ARRL section, and state/province fields in addition to the existing callsign, name,
and grid square. CQ zone and ITU zone should be pre-populated from the DXCC database
when the operator enters their callsign.

### FR-003: Per-Session Enable/Disable
The Contest menu must include a toggle to enable or disable online score publishing for
the current session. Before enabling, the system validates that all required fields are
present: callsign, password, CQ zone, ITU zone, state/province, and grid square. If
any field is missing, a dialog lists the missing fields and the feature is not enabled.
When enabled, the system posts immediately (even if the log has zero QSOs — this
validates connectivity), then continues at the configured interval. The enable state
is never saved to the log file — the operator must re-enable each session. If the
operator switches contest definitions or closes the log, online scoring is automatically
disabled.

### FR-004: Score Post Generation
The system must generate score posts in the contestonlinescore.com XML format
(dynamicresults). Each post includes: contest identifier, operator callsign, operator
list, operating class, club name, software identification, station QTH, band/mode
breakdown with QSO counts, points, and multipliers, summary totals, total score, and
UTC timestamp. The score data is a point-in-time snapshot taken when the post is
initiated — score recalculations or QSO edits/deletions that happen between posts are
reflected in the next post, not retroactively. If a post is already in-flight, the
next trigger is deferred until the current post completes.

### FR-005: Contest Online Score Configuration
Each contest definition may include a `contestOnlineScore` object containing all metadata
needed for score posting: `contestId` (the server-recognized contest identifier, e.g.,
"NAQP-CW"), `mult1Name` and `mult1Attribute` (first multiplier display name and type
identifier), and optionally `mult2Name` and `mult2Attribute` for contests with two
multiplier types. Valid attribute values are: zone, country, state, gridsquare,
wpxprefix, prefix, hq (lowercase only). Contests with no standard multipliers (e.g.,
objectiveMultipliers like Winter Field Day) omit the mult attributes — the breakdown
XML includes only QSO counts and points, no `<mult>` elements. If the
`contestOnlineScore` block is absent from the contest definition, online scoring is
unavailable for that contest and the enable toggle is grayed out.

### FR-005a: Contest Definition Updates
All existing contest definitions with a matching entry on the scoring server must be
updated to include the `contestOnlineScore` block. The following 10 contests are eligible:

| CLX Contest       | Server Contest ID | Mult 1 Attribute | Mult 2 Attribute |
|-------------------|-------------------|------------------|------------------|
| arrl_10m          | ARRL-10           | country          | state            |
| arrl_dx           | ARRL-DX-CW / ARRL-DX-SSB (mode-dependent) | country | —   |
| arrl_vhf          | ARRL-VHF          | gridsquare       | —                |
| cwops_cwt         | CW-Ops            | state            | —                |
| eudx              | EUDXC             | country          | state            |
| fqp               | FL-QSO-PARTY      | state            | —                |
| mnqp              | MN-QSO-PARTY      | state            | —                |
| naqp              | NAQP-CW / NAQP-SSB / NAQP-RTTY (mode-dependent) | state | — |
| vaqp              | VA-QSO-PARTY      | state            | —                |
| winter_field_day  | WFDA-CONTEST      | wpxprefix        | —                |

Contests with mode-dependent IDs (arrl_dx, naqp) must support a `contestIdMapping`
within the `contestOnlineScore` block that maps the contest mode userPrompt value to
the appropriate server contest ID. If the mapping has no entry for the current mode
prompt value, the base `contestId` is used as a fallback. Contests not on the server
(general_dxcc, rdxc) do not include this block and are ineligible for online scoring.

### FR-006: Operating Class Derivation
The operating class (power, assisted, transmitter, ops, bands, mode, overlay) is derived
from existing contest userPrompt values. Mode mapping: SSB maps to PH, RTTY maps to RY.
Unspecified values use defaults: HIGH power, NON-ASSISTED, ONE transmitter, SINGLE-OP,
ALL bands, MIXED mode, N/A overlay.

### FR-008: Asynchronous HTTP Posting
Score posts are sent asynchronously via HTTP POST with Basic Authentication. The posting
must not block the user interface. The system handles success responses (status 200),
server errors (status 404, 405), and network failures gracefully. HTTP 401 and 403
responses are treated as authentication failures. After 3 consecutive authentication
failures, the system auto-disables online scoring, stops the posting timer, and shows
a dialog prompting the operator to verify their credentials. The auth failure counter
resets to zero on any successful post. Network timeouts use a 15-second deadline. Unexpected HTTP status codes (500, 503,
etc.) and unparseable responses are treated as transient network errors — logged,
shown briefly in the status bar, and retried at the next interval. They do not count
toward the auth failure counter.

### FR-009: Status Bar Feedback
The status bar displays online scoring status: disabled (no indicator), enabled and
idle (waiting for next post), last successful post time, or error indicator with brief
description. The indicator updates after each post attempt.

### FR-010: Timer and Per-QSO Posting Modes
In timer mode, the system posts at the configured interval (default 5 minutes) measured
from the last post attempt. In per-QSO mode, the system posts 2 seconds after the last
QSO is logged (debounce resets on each new QSO to coalesce rapid logging). If a post is
already in-flight when the next interval or QSO trigger fires, the new post is skipped
and rescheduled for the next interval.

---

## Success Criteria *(mandatory)*

1. Operators can configure online scoring credentials in under 2 minutes on first setup
2. Score posts appear on contestonlinescore.com within the configured interval
3. The application remains responsive during score posting — no UI lag or freezing
4. Missing station information prevents enabling with a clear message listing what's needed
5. Score posts include accurate band/mode breakdown matching the operator's actual log
6. Failed posts show clear status feedback and retry automatically at the next interval
7. Feature works correctly for at least 5 different contest types (different multiplier configurations)

---

## Scope & Boundaries *(mandatory)*

### In Scope
- Settings UI for online scoring configuration (credentials, interval)
- Station info fields: CQ zone, ITU zone, ARRL section, state/province
- Contest menu toggle with validation gate
- XML generation in dynamicresults format
- HTTP POST with Basic Authentication
- Status bar posting indicator
- Timer-based and per-QSO posting modes
- Contest online score metadata per contest definition (contestOnlineScore block)
- Operating class derivation from userPrompts

### Out of Scope
- OAuth or token-based authentication (server uses Basic Auth only)
- Posting to multiple scoring servers simultaneously
- Historical score retrieval from the server
- Scoreboard display within CLX (view scores in a browser)
- Offline score queuing for later posting (post only when connected)
- Custom XML format variations for other scoring servers

---

## Assumptions *(mandatory)*

1. The contestonlinescore.com XML format is stable and will not change during development
2. Basic HTTP Authentication over HTTPS is acceptable security for contest score posting
3. Passwords stored in the CLX settings JSON file follow the same pattern as existing QRZ.com credentials (plaintext in local config)
4. The operator's internet connection is available during contest operation (no offline queuing)
5. A 2-second debounce in per-QSO mode is sufficient to avoid excessive posts during rapid logging
6. Contests without a `contestOnlineScore` block in their definition are not eligible for online scoring
7. The `<ops>` field should contain only the session callsign for single-operator entries, matching the `<call>` field
8. The online scoring credentials callsign (for HTTP auth) may differ from the session callsign (for the `<call>` element) — this is valid for guest operators using the station owner's account
9. ARRL section is optional — if empty, the `<arrlsection>` element is included but empty in the XML

---

## Dependencies *(optional)*

- **Station info fields** (CQ zone, ITU zone, ARRL section, state/province) must be added to both default and session station info before posting can include complete QTH data
- **DXCC database** (existing) provides CQ zone, ITU zone, and country prefix for auto-population
- **Contest engine scoring** (existing) provides QSO counts, points, and multiplier breakdowns per band/mode
- **Qt Network module** (existing dependency) provides HTTP client capabilities

---

## Key Entities *(optional)*

### OnlineScoringConfig
- Callsign (username for Basic Auth)
- Password (for Basic Auth)
- Posting interval (minutes, or "per-QSO" flag)
- Enabled (global default: false)

### ScorePost (generated per posting cycle)
- Contest ID
- Operator callsign and ops list
- Operating class attributes (power, assisted, transmitter, ops, bands, mode, overlay)
- Club name
- Software name and version
- QTH (DXCC country, CQ zone, ITU zone, ARRL section, state/province, grid)
- Band/mode breakdown (QSO counts, points, multipliers per type)
- Total score
- UTC timestamp

### StationInfo (extended)
- Existing: callsign, name, grid square, state
- New: CQ zone, ITU zone, ARRL section
