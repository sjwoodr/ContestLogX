# Performance Checklist: Visual Band Map

**Purpose**: Validate completeness, clarity, and measurability of performance
requirements — repaint pipeline, expiry, status re-evaluation, and scale.
**Created**: 2026-03-21
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md)

## Requirement Completeness

- [ ] CHK027 - Are performance requirements defined for the initial band map load when switching to a band with many existing spots (e.g., all 200 cached spots rendered at once)? [Completeness, Gap]
- [ ] CHK028 - Are requirements defined for the maximum number of spots the band map is expected to handle without degradation? SC-004 mentions 30 for label readability, but plan.md mentions up to 500 at peak — is 500 a supported scale or an out-of-spec condition? [Completeness, Spec §SC-004, Plan §Scale/Scope]
- [ ] CHK029 - Are repaint performance requirements defined specifically for the zoom/pan interaction — is there a target for how quickly the viewport updates as the operator drags? [Completeness, Gap]
- [ ] CHK030 - Are requirements defined for the memory footprint of stored spots — is there a maximum number of spots that can be stored simultaneously before the oldest are evicted regardless of expiry? [Completeness, Gap]

## Requirement Clarity

- [ ] CHK031 - SC-002 specifies "within 2 seconds" for spot appearance. Is this measured from cluster message receipt to pixel on screen, or from when the user issues a refresh? The clarification session resolved this as "cluster message arrival," but SC-002 in the spec should state this explicitly. [Clarity, Spec §SC-002]
- [ ] CHK032 - The plan specifies "repaint debounced to ≤60ms during cluster bursts" — is this a requirement or an implementation target? If it's a requirement, it should appear in the spec's non-functional requirements. [Clarity, Plan §Technical Context, Gap in Spec]
- [ ] CHK033 - The expiry timer fires every 60 seconds — is this interval a requirement (must be ≤60s) or an implementation choice? If operators expect near-real-time expiry accuracy, a tighter requirement may be needed. [Clarity, Plan §Spot Expiry, Gap]
- [ ] CHK034 - Is "within 1 second" for QSY (SC-003) measured from click to radio arriving at frequency, or from click to QSY command sent? These differ when flrig has latency. [Clarity, Spec §SC-003]

## Requirement Consistency

- [ ] CHK035 - SC-004 states the map should be "readable with up to 30 simultaneously active spots." The plan states scale up to 500 spots. Are requirements consistent — does the system degrade gracefully above 30, or is 30 a hard cap? [Consistency, Spec §SC-004, Plan §Scale/Scope]
- [ ] CHK036 - FR-005 requires contact status colors to update "in real time" on log changes. Is "real time" consistent with the 2-second arrival latency in SC-002 — does status re-evaluation have its own latency budget? [Consistency, Spec §FR-005, §SC-002]
- [ ] CHK037 - The plan states status re-evaluation is triggered on every log change and iterates all visible spots. For 500 spots, is this O(n) scan acceptable within the "real time" requirement? Is there a requirement that status updates complete within a defined time budget? [Consistency, Plan §Contact Status Query, Gap]

## Acceptance Criteria Quality

- [ ] CHK038 - Is SC-002 ("within 2 seconds") measurable in automated testing, or does it require manual timing with a stopwatch? If manual, is this gatable before release? [Measurability, Spec §SC-002]
- [ ] CHK039 - Is SC-003 ("within 1 second for QSY") measurable given that radio tuning time depends on the specific radio hardware and flrig round-trip latency — factors outside the application's control? Should the requirement bound only the software's contribution? [Measurability, Spec §SC-003]
- [ ] CHK040 - Are performance requirements defined for the expiry operation itself — is there a maximum time the expiry scan may take before the UI blocks? [Measurability, Gap]

## Scenario Coverage

- [ ] CHK041 - Are performance requirements defined for the "cluster burst" scenario — when 50+ spots arrive within a few seconds (typical on band opening)? SC-002's 2-second target should still hold during a burst. [Coverage, Spec §SC-002, Gap]
- [ ] CHK042 - Are performance requirements defined for status re-evaluation when a QSO is deleted and multiplier state changes for many spots simultaneously? [Coverage, Spec §FR-005, Gap]
- [ ] CHK043 - Are requirements defined for behavior when the application is running on a low-spec machine where repaint may exceed the ≤60ms target? Is graceful degradation specified? [Coverage, Gap]

## Non-Functional Requirements

- [ ] CHK044 - Are CPU usage requirements specified for the band map in idle state (no new spots, no interaction)? A continuously running QTimer and potential polling should not impose measurable CPU overhead. [Non-Functional, Gap]
- [ ] CHK045 - Are requirements defined for the band map's behavior during a contest session of 24–48 hours — does spot storage or repaint performance degrade over time? [Non-Functional, Gap]

## Notes

- CHK032 (60ms repaint debounce) and CHK033 (60s expiry interval) are plan-level details that should be promoted to spec requirements if they are testable gates
- CHK035 (30 spots readable vs 500 spots supported) is the highest-priority consistency issue — resolve before tasks
