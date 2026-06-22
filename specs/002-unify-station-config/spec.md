# Feature Specification: Migrate stationClasses to userPrompts

**Feature Branch**: `002-unify-station-config`
**Created**: 2026-03-22
**Status**: Draft
**Input**: Replace the legacy stationClasses system with enhanced userPrompts, unifying all station-dependent contest configuration under one mechanism

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Station Type Selection via User Prompt (Priority: P1)

A contest operator starting a new log for a contest with station-type-dependent rules
(e.g., ARRL DX where W/VE and DX stations have different exchanges and multipliers)
selects their station type through the standard userPrompts dialog during contest setup.
The selection drives all downstream behavior: exchange fields, scoring, multipliers, and
partner restrictions.

**Why this priority**: This is the foundation - every other behavior depends on knowing
the operator's station type. If the prompt-based selection doesn't work, nothing else can.

**Independent Test**: Start a new ARRL DX log. Verify that a "Station Type" prompt
appears during setup (not a separate station class dialog). Confirm that the selection
persists in the log file and drives the correct exchange field display.

**Acceptance Scenarios**:

1. **Given** a contest with station-type-dependent rules, **When** the operator creates
   a new log, **Then** a userPrompt asks for station type (not a separate station class dialog).

2. **Given** the operator selects "W/VE" as station type, **When** the log is created,
   **Then** the sent exchange field shows state/province (not power level), and the log
   column headers reflect the W/VE exchange format.

3. **Given** a saved log file with a station type stored via userPrompt, **When** the
   log is reopened, **Then** the station type is restored and all dependent behavior
   matches the original session.

---

### User Story 2 - Conditional Multiplier Categories (Priority: P1)

A W/VE operator in the ARRL DX Contest counts DXCC entities as multipliers, while a DX
operator counts US states and Canadian provinces. The multiplier categories that count
toward the score change based on the station type selected in the userPrompt.

**Why this priority**: Incorrect multiplier counting produces wrong scores. This is a
scoring correctness requirement - the contest results depend on it.

**Independent Test**: Create an ARRL DX log as W/VE, work several DX stations, and verify
DXCC entities appear as multipliers. Create a second log as DX, work W/VE stations, and
verify states/provinces appear as multipliers instead.

**Acceptance Scenarios**:

1. **Given** an ARRL DX log with station type "W/VE", **When** the operator works a DX
   station, **Then** the DXCC entity counts as a multiplier and states/provinces do not.

2. **Given** an ARRL DX log with station type "DX", **When** the operator works a W/VE
   station, **Then** the state/province counts as a multiplier and DXCC entities do not.

3. **Given** a contest without station-type-dependent multipliers (e.g., NAQP), **When**
   the operator logs QSOs, **Then** multiplier behavior is unchanged from current behavior.

---

### User Story 3 - Prompt-Driven Invalid Partner Restrictions (Priority: P1)

In the ARRL DX Contest, W/VE stations cannot work other W/VE stations (contacts score
zero points and don't count). This restriction must be driven by the userPrompt station
type value rather than the legacy stationClasses mechanism.

**Why this priority**: Without partner restrictions, operators could log invalid contacts
that inflate their score. This is a scoring integrity requirement.

**Independent Test**: Create an ARRL DX log as W/VE, attempt to log a contact with
another US station, and verify it scores zero points and does not count as a multiplier.

**Acceptance Scenarios**:

1. **Given** a W/VE operator in ARRL DX, **When** they log a QSO with a US station,
   **Then** the QSO scores 0 points and does not count as a multiplier.

2. **Given** a DX operator in ARRL DX, **When** they log a QSO with a W/VE station,
   **Then** the QSO scores 3 points and counts normally.

3. **Given** a contest without partner restrictions (e.g., NAQP), **When** the operator
   logs any QSO, **Then** no contacts are restricted regardless of station type.

---

### User Story 4 - Conditional Exchange Fields via visibleWhen (Priority: P1)

An operator in a contest where the sent exchange depends on station type (e.g., RDXC
where DX sends serial number but Russian stations send oblast code) sees only the
relevant exchange fields in the log view and entry form, determined by their station
type selection.

**Why this priority**: Showing irrelevant exchange fields confuses operators and leads
to data entry errors. This was already partially implemented in v0.6.14 and validates
the approach for the full migration.

**Independent Test**: Create an RDXC log as "DX" and verify the serial number column
appears. Create a second log as "Russian" and verify the oblast code column appears
instead.

**Acceptance Scenarios**:

1. **Given** a contest with visibleWhen rules on qsoFields, **When** the operator
   selects a station type, **Then** only the fields matching the visibleWhen condition
   appear in the log column headers.

2. **Given** a contest with visibleWhen rules on userPrompts, **When** the operator
   selects a station type, **Then** only the prompts matching the visibleWhen condition
   are shown during setup.

3. **Given** a field hidden by visibleWhen (e.g., SNs for a Russian RDXC operator),
   **When** the operator logs a QSO, **Then** the serial number is still tracked
   internally but not displayed in the log view.

---

### User Story 5 - Contest Migration with Score Parity (Priority: P2)

All 7 contest definitions currently using stationClasses are migrated to use userPrompts
exclusively. After migration, scores computed by the engine match the pre-migration
scores exactly for all existing test logs.

**Why this priority**: Score parity proves the migration is correct. Without it, the
migration cannot be trusted and could produce incorrect contest results.

**Independent Test**: Run `make test-logs` before and after each contest migration.
Compare scores, multiplier counts, and multiplier details. All must match exactly.

**Acceptance Scenarios**:

1. **Given** an existing test log for a migrated contest, **When** `make test-logs` is
   run after migration, **Then** the computed score matches the pre-migration score.

2. **Given** all 7 contests are migrated, **When** `make test-logs` is run, **Then**
   all automated tests pass with no regressions.

---

### User Story 6 - Legacy Code Removal (Priority: P3)

After all contests are migrated, the stationClasses code path is removed from the
contest engine, main window, and station class dialog. The codebase has one unified
mechanism for station-dependent behavior.

**Why this priority**: Removing dead code reduces maintenance burden and eliminates
confusion about which system to use for new contest definitions.

**Independent Test**: Search the codebase for `stationClasses`, `m_stationClass`,
`StationClassDialog`. No references should remain except in git history.

**Acceptance Scenarios**:

1. **Given** all contests use userPrompts only, **When** the stationClasses code is
   removed, **Then** all unit tests and log tests still pass.

2. **Given** the migration is complete, **When** a developer creates a new contest
   definition, **Then** only the userPrompts mechanism is documented and available.

---

## Functional Requirements *(mandatory)*

### FR-001: Prompt-Driven Multiplier Categories
The contest engine must support a `promptMultiplierCategories` configuration in the
scoring.multipliers section that maps userPrompt values to multiplier category lists.
When a prompt value is set, only the specified multiplier categories count toward the
score.

### FR-002: Prompt-Driven Invalid Partners
The contest engine must support an enhanced `invalidPartners` configuration that
references a `promptId`. When the prompt value matches a rule key, contacts with
stations from the listed DXCC entity prefixes score zero points and do not count as
multipliers.

### FR-003: Conditional Field Visibility (visibleWhen)
qsoFields entries with a `visibleWhen` object are shown or hidden based on the current
value of the referenced userPrompt. Hidden fields are excluded from log column headers
but their data is still stored internally when populated by other mechanisms (e.g.,
serial numbers are always tracked).

### FR-004: Conditional Prompt Visibility (visibleWhen on userPrompts)
userPrompts entries with a `visibleWhen` object are shown or skipped during contest setup
based on the current value of a previously answered prompt. Skipped prompts do not store
a value.

### FR-005: Serial Number Always Tracked
The SNs (serial number sent) field is always populated with an auto-incrementing value
on every logged QSO, regardless of whether the SNs column is visible in the log view.

### FR-006: Exchange Field Mapping from Prompts
userPrompts with an `exchangeFieldMapping` property automatically populate the specified
exchange field with the prompt value when a QSO is logged. This replaces the
stationClasses `exchangeSent` mechanism.

### FR-007: Score Parity After Migration
Each contest migrated from stationClasses to userPrompts must produce identical scores,
multiplier counts, and multiplier details as the pre-migration version for all existing
test logs.

### FR-008: Backward-Compatible Log Loading
Saved log files (.clx) created with the stationClasses mechanism must load correctly
after migration. The engine must recognize both the old `stationClass` field and the
new userPrompt values during log restoration.

---

## Success Criteria *(mandatory)*

1. All 12 contest definitions use userPrompts exclusively - no stationClasses blocks remain
2. All automated test suites pass (`make test`, `make test-logs`) with zero regressions
3. Score parity confirmed for every migrated contest across all existing test logs
4. Saved log files from pre-migration versions load and score correctly
5. Station class selection dialog is removed - station type is collected through the
   standard userPrompts flow
6. Contest definition format has one unified mechanism for all station-dependent behavior

---

## Scope & Boundaries *(mandatory)*

### In Scope
- Engine enhancements: promptMultiplierCategories, prompt-driven invalidPartners
- Migration of 7 contest definitions (arrl_10m, arrl_dx, arrl_vhf, cwops_cwt, general_dxcc, naqp, ybdx)
- Removal of stationClasses engine code, StationClassDialog, and related mainWindow code
- Documentation updates (CLAUDE.md, website docs, contest definition README)

### Out of Scope
- Adding new contest definitions (separate work items)
- Changing contest scoring rules (migration must be score-identical)
- UI redesign of the userPrompts dialog beyond what's needed for migration
- Multi-operator station class features not currently implemented

---

## Assumptions *(mandatory)*

1. The visibleWhen mechanism (implemented in v0.6.14) works correctly and does not need redesign
2. All contests with stationClasses have corresponding test logs in the automated test suite
3. The userPrompts dialog can present the same station type choices that stationClasses currently offers
4. Contest log files store userPrompt values in a way that survives save/load cycles
5. The `restrictMode` feature on userPrompts already handles mode restriction, replacing `getStationClassMode`

---

## Dependencies *(optional)*

- **visibleWhen on qsoFields** (completed in v0.6.14): Foundation for conditional exchange fields
- **visibleWhen on userPrompts** (completed in v0.6.14): Foundation for conditional setup prompts
- **exchangeFieldMapping** (existing): Auto-populates sent exchange from prompt values
- **Generic prefix-based scoring** (completed in v0.6.14): Replaces hardcoded euCountry

---

## Migration Order

Contests are ordered by complexity to validate the approach incrementally:

1. **cwops_cwt** - Exchange type only (simplest case)
2. **general_dxcc** - Exchange type only
3. **naqp** - Exchange type with name + state
4. **arrl_10m** - Exchange type with station class selection
5. **ybdx** - Exchange type with prefix multipliers
6. **arrl_vhf** - Exchange type + mode restriction + multiplier categories
7. **arrl_dx** - Exchange type + multiplier categories + invalidPartners (most complex)

Each migration is independently testable via `make test-logs`.
