# UX Checklist: Visual Band Map

**Purpose**: Validate completeness, clarity, and consistency of user experience
requirements — rendering, interaction, states, and accessibility.
**Created**: 2026-03-21
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md)

## Requirement Completeness

- [ ] CHK001 - Are the visual dimensions (height, width, minimum usable size) of the band map panel specified, or is this left entirely to the dock system? [Completeness, Gap]
- [ ] CHK002 - Are requirements defined for what the frequency axis tick marks display — frequency labels at what intervals, in what unit (MHz/kHz)? [Completeness, Gap]
- [ ] CHK003 - Are requirements defined for how the operator's own VFO frequency is represented on the map (line, indicator, or omitted entirely)? [Completeness, Gap — plan.md mentions a VFO line but spec.md does not]
- [ ] CHK004 - Are hover interaction requirements defined for all interactive elements beyond spot markers (e.g., frequency axis itself, zoom slider, empty-state area)? [Completeness, Spec §FR-002]
- [ ] CHK005 - Are requirements defined for the zoom slider's visual appearance — range labels, tick marks, position indicator? [Completeness, Spec §FR-012]
- [ ] CHK006 - Are pan interaction requirements fully specified — is click-drag the only mechanism, or is a scrollbar also required? [Completeness, Spec §FR-012, Clarifications]

## Requirement Clarity

- [ ] CHK007 - Is "readable" in SC-004 quantified? ("spot labels non-overlapping at default zoom with up to 30 spots" — is "non-overlapping" sufficient, or is a minimum label height/font size also needed?) [Clarity, Spec §SC-004]
- [ ] CHK008 - Is "informative empty state" in FR-013 specified with required content for each of the three conditions (no contest, no cluster, no spots on band)? The spec states the text for "no contest" but not the other two. [Clarity, Spec §FR-013]
- [ ] CHK009 - Is "visually distinct highlighted color" for new multipliers (FR-004) sufficient, or does the spec need to define a minimum contrast ratio to ensure distinguishability? [Clarity, Spec §FR-004]
- [ ] CHK010 - Is the ±5 pixel click tolerance for spot selection (plan.md) referenced in the spec as a requirement, or is it purely an implementation detail? If it affects usability at high zoom, it belongs in the spec. [Clarity, Plan §Click-to-QSY]
- [ ] CHK011 - Is "truncate gracefully" in the narrow-panel edge case defined with a minimum visible character count or truncation indicator (e.g., ellipsis)? [Clarity, Spec §Edge Cases]

## Requirement Consistency

- [ ] CHK012 - FR-004 requires three status colors to be distinguishable "without a legend," but SC-005 says this is "verified by informal user testing." Are these consistent — is the legend-free requirement a testable gate or a post-release aspiration? [Consistency, Spec §FR-004, §SC-005]
- [ ] CHK013 - The spec says click-to-QSY should behave "consistent with clicking a spot in the existing cluster table" (US3, scenario 3). Is the existing cluster table behavior formally documented or assumed? If assumed, is this a hidden dependency? [Consistency, Spec §US3]
- [ ] CHK014 - FR-002 specifies tooltip content as "mode, frequency, spotter callsign, and spot age." Are these consistent with the SpotData entity definition in the data model, which lists the same fields? [Consistency, Spec §FR-002, data-model.md]
- [x] CHK015 - **Resolved**: FR-015 updated — zoom/pan is not persisted; it resets to full-band default on band change. SC-007 updated accordingly. US5 and FR-015 are now consistent. [Consistency, Spec §US5, §FR-015]

## Acceptance Criteria Quality

- [ ] CHK016 - SC-004 defines readability with "up to 30 spots." Is this threshold derived from real contest conditions or arbitrary? Is it measurable without a reference dataset? [Measurability, Spec §SC-004]
- [ ] CHK017 - SC-005 ("correctly identified without a legend, verified by informal user testing") — is "informal user testing" defined enough to be a gateable criterion? Who conducts it, how many subjects, what pass rate? [Measurability, Spec §SC-005]
- [ ] CHK018 - SC-001 ("all spots visible simultaneously without scrolling") — is this achievable for all bands at all zoom levels, or only at default full-band zoom? The phrasing could imply no scrolling ever. [Clarity, Spec §SC-001]

## Scenario Coverage

- [ ] CHK019 - Are interaction requirements defined for the zoom slider when the band map is very narrow (docked)? Could the slider become unusably small? [Coverage, Spec §Edge Cases]
- [ ] CHK020 - Are requirements defined for what happens when the operator drags a spot marker versus clicking it? (Is drag-to-move a spot ever possible, or is all drag reserved for panning?) [Coverage, Gap]
- [ ] CHK021 - Are requirements defined for the tooltip when multiple spots overlap at the same pixel position after zoom-out? Does hovering show one tooltip or multiple? [Coverage, Gap]
- [ ] CHK022 - Are requirements defined for the band map appearance during the transition between bands (e.g., a brief flash of empty state before new band's spots load)? [Coverage, Gap]
- [ ] CHK023 - Are keyboard navigation requirements explicitly excluded and documented as a known limitation? The assumption states keyboard nav is "not required but must not be architecturally precluded." Is this captured as a formal out-of-scope statement? [Coverage, Spec §Assumptions]

## Edge Case Coverage

- [ ] CHK024 - The "very narrow panel" edge case specifies label truncation but not frequency axis truncation — are requirements defined for a minimum useful frequency axis width? [Edge Case, Spec §Edge Cases]
- [ ] CHK025 - Are requirements defined for the behavior when a spot's frequency falls outside the current band range (e.g., cluster sends a 21 MHz spot while on 20m)? [Edge Case, Gap]
- [ ] CHK026 - Are requirements defined for what happens to zoom/pan state when a contest is unloaded (no band range) and then reloaded? [Edge Case, Gap]

## Notes

- Items marked `[Gap]` indicate missing requirements that should be addressed in spec.md before proceeding to tasks
- Items marked `[Clarity]` indicate requirements that exist but need quantification
- CHK015 (zoom state persistence vs. per-band reset) is the highest-priority consistency issue to resolve
