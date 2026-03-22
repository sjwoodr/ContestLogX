# Error Handling Checklist: Visual Band Map

**Purpose**: Validate completeness, clarity, and consistency of failure-mode and
edge-case requirements — cluster disconnects, missing contest, radio absence, and
state transitions under adverse conditions.
**Created**: 2026-03-21
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md)

## Requirement Completeness

- [ ] CHK046 - FR-013 requires an empty state for "no cluster connection active" — is the requirement defined for the transition *into* this state (cluster drops mid-session) separately from startup with no cluster configured? [Completeness, Spec §FR-013, §Edge Cases]
- [ ] CHK047 - Are requirements defined for what happens to spots already displayed when the cluster connection drops? The edge case states they "remain visible until expiry" — is this a firm requirement or an assumption? [Completeness, Spec §Edge Cases]
- [ ] CHK048 - Are requirements defined for re-connecting to the cluster after a disconnect — do spots reload, or does the map only show spots received after reconnection? [Completeness, Gap]
- [ ] CHK049 - FR-007 requires a status message when QSY fails (no radio). Are requirements defined for where this message appears (status bar, tooltip, modal?) and how long it persists? [Completeness, Spec §FR-007, Gap]
- [ ] CHK050 - Are requirements defined for what happens when a contest is *unloaded* (File → New) while the band map is open and showing spots? [Completeness, Gap]
- [ ] CHK051 - Are requirements defined for the band map's behavior during contest file loading (the period between "old contest unloaded" and "new contest loaded")? [Completeness, Gap]

## Requirement Clarity

- [ ] CHK052 - The edge case "cluster disconnects while map is open: an indicator shows the cluster is not connected" — is the indicator specified? Is it on the band map widget itself, the status bar, or the DX cluster panel? [Clarity, Spec §Edge Cases]
- [ ] CHK053 - FR-013 requires "informative empty state" for three conditions. Is each condition required to show a distinct message, or is a generic "Band map unavailable" acceptable for some? [Clarity, Spec §FR-013]
- [ ] CHK054 - The edge case "all spots expire simultaneously" requires "graceful transition to empty state" — is "graceful" defined? Does it mean no flicker, no UI freeze, or both? [Clarity, Spec §Edge Cases]
- [ ] CHK055 - FR-007 says "MUST NOT crash" — is this the only negative-path requirement for click-to-QSY failure, or should the requirement also specify that the failed QSY is reported in a consistent way with other QSY failures in the application? [Clarity, Spec §FR-007]

## Requirement Consistency

- [x] CHK056 - **Resolved**: Edge case updated — on disconnect, existing spots remain until expiry; the "no cluster" indicator is shown alongside spots (not instead of them). On reconnect, all spots are cleared and the map starts fresh. FR-013 empty state applies only when no spots remain and no cluster is connected. [Consistency, Spec §FR-013, §Edge Cases]
- [ ] CHK057 - US3 scenario 2 says clicking a spot with no radio connected shows "a clear status message." FR-007 says the band map "MUST display an informative status message." Are these consistent in specifying the same behavior, or could they lead to two different message mechanisms? [Consistency, Spec §US3, §FR-007]
- [x] CHK058 - **Resolved**: All spots are cleared on band change. This is documented in Assumptions and is consistent with the edge case for "no spots on current band." Spots are not filtered — the entire spot store is reset when the active band changes. [Consistency, Spec §Assumptions, §Edge Cases]

## Scenario Coverage

- [ ] CHK059 - Are requirements defined for the failure mode where the ContestEngine returns an error or unexpected value when resolveSpotStatus is called? [Coverage, Gap]
- [ ] CHK060 - Are requirements defined for the scenario where the rig frequency polling stops (flrig loses connection) while the band map is open? Does the band map freeze on the last known band, or transition to an unknown-band state? [Coverage, Gap]
- [ ] CHK061 - Are requirements defined for what happens when the contest JSON does not contain frequency information for the current band? (FR-010 requires band range from contest JSON; what if the field is missing?) [Coverage, Spec §FR-010, Gap]
- [ ] CHK062 - Are requirements defined for a spot arriving for a frequency outside the contest's defined band ranges (e.g., a spot for 14.500 MHz when the 20m contest band ends at 14.350)? Should it be filtered silently or shown at the band edge? [Coverage, Gap]
- [ ] CHK063 - Are recovery requirements defined for when QSettings fails to restore zoom/pan state (corrupted, missing, or wrong data type)? FR-015 requires state persistence — what is the fallback? [Coverage, Spec §FR-015, Gap]

## Edge Case Coverage

- [ ] CHK064 - Are requirements defined for a spot arriving with a malformed or empty callsign field from the cluster? [Edge Case, Gap]
- [ ] CHK065 - Are requirements defined for a spot arriving with a frequency of 0.0 or a negative value (malformed cluster data)? [Edge Case, Gap]
- [ ] CHK066 - Are requirements defined for what happens if the zoom slider or scroll wheel is used when no contest is loaded (no band range defined)? [Edge Case, Gap]

## Dependencies & Assumptions

- [ ] CHK067 - The assumption that the DX cluster connection is managed entirely by DxClusterPanel is documented. Are requirements defined for what signal or API the band map receives when the cluster connection state changes (connect/disconnect)? [Assumption, Spec §Assumptions, Gap]
- [ ] CHK068 - The assumption that resolveSpotStatus uses ContestEngine as authoritative is documented. Is there a requirement for what ContactStatus is assigned when no contest is loaded (no multiplier context)? [Assumption, Spec §Assumptions, Gap]
- [ ] CHK069 - Are requirements or assumptions documented for how the band map behaves when it is open but minimized or hidden (dock closed) — does it continue processing incoming spots, or pause? [Assumption, Gap]

## Notes

- CHK056 (cluster-disconnect empty state vs. spots-remain-visible) is the highest-priority conflict — it must be resolved before tasks generate implementation steps
- CHK058 (spots filtered vs. cleared on band change) directly impacts data model implementation — resolve before tasks
- CHK067–CHK068 are integration-boundary gaps that could surface as implementation surprises
