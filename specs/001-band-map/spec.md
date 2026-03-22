# Feature Specification: Visual Band Map

**Feature Branch**: `001-band-map`
**Created**: 2026-03-21
**Status**: Draft
**Input**: DX cluster spot visualization on a frequency axis with click-to-QSY for contest operators

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Spot Display on Frequency Axis (Priority: P1)

A contest operator running Search & Pounce wants to see all active DX cluster spots
for their current band laid out visually by frequency. Instead of scanning a table
of rows, they can see the entire band at a glance — where activity is clustered,
which portions of the band are quiet, and where specific stations are operating.

**Why this priority**: This is the core value of the feature. Without the
frequency-axis display, all other user stories are meaningless. It is also the
minimum viable version of the band map — even without color coding or click-to-QSY,
operators benefit from the spatial overview.

**Independent Test**: Open the band map panel with an active DX cluster connection
and a contest loaded. Verify that spots appearing in the cluster list also appear
as labeled markers at the correct positions on the frequency axis.

**Acceptance Scenarios**:

1. **Given** a contest is loaded and the DX cluster is connected, **When** the band
   map panel is opened, **Then** all spots for the current band appear as labeled
   markers at positions corresponding to their spotted frequency.

2. **Given** the band map is visible, **When** a new spot arrives from the cluster
   for the current band, **Then** it appears on the map within 2 seconds of arrival.

3. **Given** the band map is visible, **When** the operator changes bands (radio
   QSYs to a different band), **Then** the map updates to show spots for the new
   band.

4. **Given** the band map is open but no contest is loaded, **When** the operator
   views the panel, **Then** an informative empty state is shown ("No contest
   loaded") rather than an error or blank screen.

---

### User Story 2 - Spot Color Coding by Contact Status (Priority: P2)

A contest operator wants to instantly know, from the band map, which spotted stations
represent a new multiplier, which have already been worked, and which are unworked
non-multipliers — so they can prioritize new multipliers without reading through
the full spot list.

**Why this priority**: Color coding transforms the band map from a spatial index
into a prioritization tool. Operators in competitive contesting make decisions in
seconds; visual differentiation of contact value is essential. Depends on P1.

**Independent Test**: Work a station displayed as a new multiplier. Verify its
marker color changes to the already-worked color without the spot disappearing.

**Acceptance Scenarios**:

1. **Given** a spot represents a station that would be a new scoring multiplier,
   **When** the spot appears on the band map, **Then** it is displayed in a
   visually distinct highlighted color.

2. **Given** a spot represents a station already logged in the current contest,
   **When** the spot appears on the band map, **Then** it is displayed in a
   muted or subdued color.

3. **Given** a spot represents an unworked non-multiplier, **When** it appears
   on the band map, **Then** it is displayed in a neutral default color.

4. **Given** an operator logs a QSO with a station shown as a new multiplier,
   **When** the QSO is saved, **Then** that spot's color updates to the worked
   color without the spot being removed.

---

### User Story 3 - Click to QSY (Priority: P3)

A contest operator sees an interesting spot on the band map and wants to immediately
QSY their radio to that frequency and mode by clicking the spot, rather than reading
the frequency and manually tuning.

**Why this priority**: Click-to-QSY is the primary action that distinguishes the
band map from the existing spot table. It collapses a multi-step workflow into a
single click.

**Independent Test**: With a radio connected, click any spot on the band map.
Verify the radio tunes to the spot's frequency and switches to the spot's mode
within 1 second.

**Acceptance Scenarios**:

1. **Given** a radio is connected, **When** the operator clicks a spot on the
   band map, **Then** the radio tunes to that spot's frequency and mode within
   1 second.

2. **Given** the radio is not connected, **When** the operator clicks a spot,
   **Then** a clear status message explains that no radio is connected; no crash
   or silent failure occurs.

3. **Given** the operator clicks a spot and the QSY completes, **When** the
   QSO entry panel is observed, **Then** it reflects the new frequency and mode
   (consistent with clicking a spot in the existing cluster table).

---

### User Story 4 - Automatic Spot Expiry (Priority: P4)

A contest operator wants stale spots to be automatically removed from the band map
so the display reflects current band conditions rather than accumulating hours of
historical spots.

**Why this priority**: Without expiry, the band map becomes cluttered with outdated
information. The display is usable without it for short sessions but degrades over
a full contest weekend. Lower priority because the core display value exists without it.

**Independent Test**: Set a short expiry threshold (e.g., 2 minutes) in settings,
allow spots to arrive, then wait past the threshold. Verify spots disappear without
manual action.

**Acceptance Scenarios**:

1. **Given** the spot expiry threshold is configured (default: 30 minutes), **When**
   a spot's age exceeds that threshold, **Then** it is automatically removed from
   the band map.

2. **Given** a spot has been removed by expiry, **When** the same station is
   re-spotted by the cluster, **Then** the spot reappears with a fresh timestamp.

3. **Given** the operator sets a custom expiry threshold in settings, **When**
   the band map is in use, **Then** expiry uses the configured value.

---

### User Story 5 - Zoom and Pan the Frequency Axis (Priority: P5)

A contest operator running CW wants to zoom the band map to show only the CW
sub-band so spot labels are readable without crowding when many stations are
active in a narrow frequency range.

**Why this priority**: Zoom/pan is a usability enhancement for crowded band
conditions. The feature delivers value without it (P1–P4 complete), but
readability degrades with 50+ spots compressed into a full-band view.

**Independent Test**: With 20+ spots visible at full-band zoom, zoom in to a
50 kHz window. Verify spot labels are readable and non-overlapping in the
zoomed view.

**Acceptance Scenarios**:

1. **Given** the band map is showing a congested band, **When** the operator
   zooms in, **Then** the visible frequency range narrows and spot labels become
   more spread out and readable.

2. **Given** the operator has zoomed in, **When** they pan the frequency axis,
   **Then** the visible window slides along the band without losing the zoom level.

3. **Given** the operator has zoomed or panned, **When** the band changes,
   **Then** the zoom and pan reset to the full-band default for the new band.

4. **Given** the operator has zoomed or panned, **When** the application restarts,
   **Then** the previous zoom/pan state is restored.

---

### Edge Cases

- **No contest loaded**: Band map has no band range to display; shows "No contest
  loaded" empty state — no error, no crash.
- **Cluster disconnects while map is open**: Existing spots remain visible until
  expiry; new spots stop arriving; an indicator shows the cluster is not connected.
- **Multiple spots for the same callsign at slightly different frequencies**: Each
  report is displayed as a distinct marker at its reported frequency; no
  deduplication is performed.
- **Radio not connected on click-to-QSY**: Status message shown; no crash.
- **All spots expire simultaneously**: Map transitions to empty state gracefully;
  no flicker or error.
- **No spots on current band**: Informative empty state shown ("No spots on [BAND]"),
  not a blank or broken view.
- **Band map panel very narrow** (docked alongside other panels): Spot labels
  truncate gracefully; frequency axis remains usable.

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The band map MUST display all current DX cluster spots for the
  operator's active contest band as markers on a frequency axis.

- **FR-002**: Each spot marker MUST show the spotted callsign and indicate the
  reported operating mode.

- **FR-003**: A new spot MUST appear on the band map within 2 seconds of being
  received from the DX cluster.

- **FR-004**: Spot markers MUST be color-coded by contact status: new scoring
  multiplier, already-worked, and unworked non-multiplier MUST be visually
  distinguishable without a legend.

- **FR-005**: Contact status color MUST update in real time when the operator logs
  a QSO with a spotted station (no manual refresh required).

- **FR-006**: Operators MUST be able to click a spot marker to QSY the active
  radio to that spot's frequency and mode.

- **FR-007**: When click-to-QSY is attempted with no radio connected, the band map
  MUST display an informative status message and MUST NOT crash.

- **FR-008**: Spots MUST be automatically removed when their age exceeds a
  configurable expiry threshold (default: 30 minutes).

- **FR-009**: The spot expiry threshold MUST be configurable by the operator via
  application settings.

- **FR-010**: The displayed band range MUST be derived from the active contest's
  definition for the current band; no band edges are hard-coded.

- **FR-011**: The band map MUST update its displayed band and reload spots when
  the operator's active band changes.

- **FR-012**: Operators MUST be able to zoom the frequency axis to narrow the
  visible range and pan to shift the visible window along the band.

- **FR-013**: The band map MUST show an informative empty state when no contest
  is loaded, no cluster connection is active, or no spots exist for the current band.

- **FR-014**: The band map panel MUST be dockable, floatable, and closable,
  consistent with other panels in the application.

- **FR-015**: Band map panel position, size, and zoom/pan state MUST persist
  across application sessions.

### Key Entities

- **Spot**: A DX cluster report identifying a station operating at a specific
  frequency and mode. Attributes: spotted callsign, frequency (MHz), mode
  (CW/SSB/RTTY/FT8/etc.), spotter callsign, timestamp received.

- **Contact Status**: Classification of a spot relative to the operator's current
  log. Values: *new multiplier* (adds a scoring multiplier if worked), *worked*
  (already logged this contest), *unworked non-multiplier* (not logged, no
  multiplier value).

- **Band Range**: The frequency boundaries for the active contest band segment,
  sourced from the contest definition. Varies by contest and band.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All spots for the current band are visible simultaneously in the
  default full-band view without scrolling.

- **SC-002**: A new spot appears on the band map within 2 seconds of being received
  from the DX cluster (measured from cluster message arrival to marker on screen).

- **SC-003**: Clicking a spot QSYs the radio to the correct frequency and mode
  within 1 second of the click.

- **SC-004**: The map remains readable (spot labels non-overlapping at default zoom)
  with up to 30 simultaneously active spots; zoom is available to manage denser
  conditions.

- **SC-005**: The three contact status colors are correctly identified by operators
  without referring to a legend (verified by informal user testing).

- **SC-006**: The band map panel opens, docks, floats, resizes, and closes without
  affecting the layout or behavior of any other panel.

- **SC-007**: After application restart, the band map panel position, size, and
  zoom state are restored to the previous session's values.

---

## Assumptions

- The DX cluster connection and spot delivery are managed by the existing DX Cluster
  panel; the band map consumes spots passively and does not manage its own cluster
  connection.
- Contact status (multiplier/worked classification) is computed by the application's
  existing contest scoring logic and is authoritative for the current log state.
- The "active band" follows the operator's active radio frequency as reported by
  the existing rig control integration.
- Spot deduplication (multiple cluster reports of the same callsign) is not
  performed; each report appears as a distinct marker at its reported frequency.
- Mouse-driven interaction is the primary model for spot selection; keyboard
  navigation of spot markers is not required in this spec but must not be
  architecturally precluded.
