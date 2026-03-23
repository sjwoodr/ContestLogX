# Integration & Error Handling Checklist: Online Score Publishing

**Purpose**: Validate requirements completeness and clarity for the contestonlinescore.com integration and failure handling
**Created**: 2026-03-23
**Feature**: [spec.md](../spec.md)
**Focus**: Integration contract, error handling, edge cases
**Depth**: Standard
**Audience**: Reviewer (PR)

## Requirement Completeness

- [ ] CHK001 - Are XML element ordering requirements specified, or is order assumed from examples? [Completeness, Spec §FR-004]
- [ ] CHK002 - Is the Content-Type header explicitly specified as `application/xml`? [Completeness, Spec §FR-008]
- [x] CHK003 - Are requirements defined for what happens when the contest definition lacks a `contestOnlineScore` block and the operator tries to enable? [Completeness, Spec §FR-005] — toggle grayed out
- [ ] CHK004 - Is the `<ops>` field behavior specified for multi-operator entries (comma-separated callsigns)? [Completeness, Spec §FR-004]
- [ ] CHK005 - Are requirements defined for the `<club>` element source — where does the club name come from? [Completeness, Spec §FR-004]
- [ ] CHK006 - Is the grid square format specified — when to use `<grid4>` vs `<grid6>`? [Completeness, Contract]
- [ ] CHK007 - Are requirements defined for bands that don't appear in the server's band format (e.g., VHF bands like 6m, 2m)? [Gap]
- [ ] CHK008 - Is the timestamp format explicitly specified as UTC? [Completeness, Spec §FR-004]

## Requirement Clarity

- [x] CHK009 - Is "a few seconds" for per-QSO posting debounce quantified with a specific duration? [Clarity, Spec §US3 scenario 4] — fixed: 2-second debounce
- [x] CHK010 - Is "brief debounce" in FR-010 quantified with a specific duration? [Clarity, Spec §FR-010] — fixed: 2-second debounce with reset
- [x] CHK011 - Is the behavior defined when a mode-dependent contest ID mapping has no match for the current mode? [Clarity, Spec §FR-005a] — fixed: falls back to base contestId
- [x] CHK012 - Are the specific HTTP status codes that constitute an "authentication failure" defined (401? 403? other)? [Clarity, Spec §FR-008] — fixed: 401 and 403
- [x] CHK013 - Is "auto-disables online scoring" defined in terms of what UI state changes occur? [Clarity, Spec §FR-008] — fixed: stops timer, shows dialog, operator re-enables via menu

## Requirement Consistency

- [ ] CHK014 - Is the mode mapping (SSB→PH, RTTY→RY) consistent between FR-006 (class element) and the band/mode breakdown XML generation? [Consistency, Spec §FR-004 vs §FR-006]
- [ ] CHK015 - Are the required station info fields consistent between FR-002 (field definitions) and FR-003 (validation gate)? [Consistency, Spec §FR-002 vs §FR-003]
- [ ] CHK016 - Is the `contestOnlineScore.contestId` the same field used in both the `<contest>` XML element and the validation gate check? [Consistency, Spec §FR-005]

## Scenario Coverage

- [x] CHK017 - Are requirements defined for what happens when the operator recalculates the score while a post is in-flight? [Coverage] — fixed: snapshot model, in-flight post defers next trigger
- [x] CHK018 - Are requirements defined for what happens if the operator deletes or edits a QSO between posting intervals? [Coverage] — fixed: reflected in next post snapshot
- [x] CHK019 - Are requirements defined for the initial post when the log has zero QSOs (operator enables before logging)? [Coverage, Edge Case] — fixed: posts immediately even with zero QSOs
- [x] CHK020 - Are requirements defined for posting when the contest has no multipliers (e.g., Winter Field Day with objectiveMultipliers)? [Coverage, Spec §FR-005] — fixed: omit mult elements
- [x] CHK021 - Is the behavior specified when the operator switches contest definitions while online scoring is enabled? [Coverage] — fixed: auto-disables on contest switch

## Edge Case Coverage

- [x] CHK022 - Is the behavior defined when the server returns an unexpected HTTP status code (e.g., 500, 503)? [Edge Case, Spec §FR-008] — fixed: treated as transient, retry at next interval
- [x] CHK023 - Is the behavior defined when the server returns malformed JSON (not parseable)? [Edge Case, Spec §FR-008] — fixed: treated as transient error
- [x] CHK024 - Is the behavior defined when the network request times out? Is a timeout duration specified? [Edge Case] — fixed: 15-second timeout
- [x] CHK025 - Are requirements defined for handling the case where the operator's callsign in settings differs from the session callsign? [Edge Case] — fixed: documented as valid (guest operator assumption #8)
- [ ] CHK026 - Is the behavior defined when the server returns success but the contest has ended (stale posting)? [Edge Case, Gap]
- [x] CHK027 - Are requirements defined for ARRL section being empty/optional in the QTH element? [Edge Case] — fixed: assumption #9

## Error Recovery

- [x] CHK028 - After auto-disable from 3 auth failures, are requirements defined for how the operator re-enables? [Recovery, Spec §FR-008] — fixed: re-enable via Contest menu after correcting credentials
- [x] CHK029 - Is the consecutive auth failure counter reset on a successful post? [Recovery] — fixed: resets to zero on success
- [x] CHK030 - Are requirements defined for what happens to the timer when a post is in-flight and the next interval arrives? [Recovery] — fixed: skip and reschedule

## Dependencies & Assumptions

- [ ] CHK031 - Is the assumption that the XML format is stable documented with a mitigation plan if it changes? [Assumption, Spec §Assumptions #1]
- [ ] CHK032 - Is the dependency on an active internet connection stated as a user-facing requirement (not just an assumption)? [Dependency, Spec §Assumptions #4]
- [ ] CHK033 - Are the DxccDatabase fields (cqZone, ituZone) documented as nullable for callsigns not found in cty.dat? [Dependency, Spec §FR-002]

## Summary

- **Remediated**: 19 items (CHK003, CHK009-CHK013, CHK017-CHK025, CHK027-CHK030)
- **Remaining open**: 14 items (low-to-medium impact, acceptable for implementation)
- **Critical gaps closed**: debounce timing, auth failure codes, in-flight post handling, zero-QSO case, timeout duration, recovery flow
