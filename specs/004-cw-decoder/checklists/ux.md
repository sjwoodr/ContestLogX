# UX Checklist: CW Decoder

**Purpose**: Validate that the CW Decoder spec's UX requirements are complete, clear, consistent, and measurable — before implementation begins. Items test the REQUIREMENTS, not the implementation.
**Created**: 2026-04-21
**Feature**: [spec.md](../spec.md)

---

## Requirement Completeness

- [ ] CHK001 Are visual layout requirements for the stacked bin rows (row height, inter-row spacing, label position) quantified with measurable criteria? [Completeness, Gap]
- [ ] CHK002 Are requirements specified for where the bin's center-frequency label appears relative to its scrolling text (left-margin label? fixed-width gutter? column width in pixels or em units)? [Completeness, Gap]
- [ ] CHK003 Are requirements defined for how decoded callsign and RST tokens are *visually distinguished* from surrounding plain decoded text (underline, color, weight, or another treatment)? [Completeness, Gap]
- [ ] CHK004 Are requirements defined for the *visual feedback* the operator receives at click time (flash, highlight, brief tooltip, or field-side confirmation) when a token successfully fills an entry field? [Completeness, Spec §FR-020, FR-021]
- [ ] CHK005 Are requirements defined for the spotlight row's *visual treatment* (larger font? highlighted background? sticky-at-top position? border?) beyond the generic "visual emphasis" phrasing? [Completeness, Ambiguity, Spec §FR-009]
- [ ] CHK006 Are keyboard-accessibility requirements specified for every interactive control in the widget (squelch slider, bin-config inputs, spotlight selector, clear button, spotlight row selector) per Principle III? [Completeness, Coverage, Gap]
- [ ] CHK007 Are requirements defined for Tab-navigation order through the decoder widget so that the widget doesn't trap focus or disrupt the QSO-entry Tab sequence? [Completeness, Gap]
- [ ] CHK008 Are requirements defined for the widget's Window-menu entry (label text, enable/disable state when no audio device is configured, keyboard shortcut if any)? [Completeness, Gap]
- [ ] CHK009 Are requirements defined for how the operator *initiates* and *exits* spotlight mode on a row (click a row header? dedicated selector? context menu)? [Completeness, Spec §FR-009]
- [ ] CHK010 Are requirements defined for a Clear button's *scope* explicitly enough to distinguish "clear all rows" from "clear spotlight row only" from "clear per-row button"? [Completeness, Spec §FR-012]

## Requirement Clarity

- [ ] CHK011 Is "visual emphasis" (spotlight row) quantified with specific, objectively verifiable visual properties rather than a subjective description? [Clarity, Ambiguity, Spec §FR-009]
- [ ] CHK012 Is the dim/gate *visual treatment* when the decoder is muted (FR-019a/FR-019c) specified with enough detail that an operator can clearly distinguish "muted (I'm transmitting)" from "no signal detected" without ambiguity? [Clarity, Spec §FR-019a, §FR-019c]
- [ ] CHK013 Is the exact label format for each row's center frequency specified (e.g., "500 Hz" vs "0.5 kHz" vs "500"), and is the precision (integer vs one decimal) defined? [Clarity, Gap]
- [ ] CHK014 Is the live WPM readout's display format specified (e.g., "25 WPM", "25", "WPM: 25"), including the "no lock" state representation (dashes, blank, "—", "NO LOCK")? [Clarity, Spec §FR-008, Assumption "No lock state"]
- [ ] CHK015 Are the passband-edge input controls specified with respect to type (numeric spin box? text field? slider?) and units (Hz, kHz)? [Clarity, Gap, Spec §FR-011]
- [ ] CHK016 Is "stacked rows" disambiguated between fixed-height rows and operator-resizable rows? [Clarity, Ambiguity]
- [ ] CHK017 Is the squelch slider's range, step granularity, and display format (normalized 0.0–1.0? percentage? dB?) specified? [Clarity, Gap, Spec §FR-010]

## Requirement Consistency

- [ ] CHK018 Do the SO2R two-widget requirements (FR-005, FR-006, US8) consistently state that each widget is bound to exactly one radio and is independently configurable without cross-widget coupling? [Consistency, Spec §FR-006, §US8]
- [ ] CHK019 Are the "no focus steal" requirements (FR-022) consistently reflected across CALL click (FR-020), RST click (FR-021), and the "identical to keyboard entry" side-effect requirement (FR-024a)? [Consistency]
- [ ] CHK020 Do the "widget hidden entirely when no audio device is configured" assumption and FR-006 ("decoder panel MUST exist for every radio that has an audio device configured and MUST NOT exist for radios with '(none)'") align exactly, or does "MUST NOT exist" leave room for a disabled-but-visible panel? [Consistency, Ambiguity]
- [ ] CHK021 Are the click-to-fill requirements consistent across all entry side effects — SCP, call-history, dupe check, name/QTH auto-fill — as specified identically in FR-024a and Assumption "Click-to-fill side effects (all)"? [Consistency]
- [ ] CHK022 Do the clickable-token requirements (FR-024: only callsign- and RST-shaped tokens are interactive) align with the Assumption entry on "Callsign token recognition" including slash notation? [Consistency]

## Acceptance Criteria Quality

- [ ] CHK023 Can the "click target size" requirement be objectively measured — is a minimum hit-target dimension (e.g., pixels or device-independent units) specified for tokens on HiDPI displays? [Measurability, Gap]
- [ ] CHK024 Can the spotlight row's "visual emphasis" be objectively verified by an independent tester using only the spec? [Measurability, Spec §FR-009]
- [ ] CHK025 Can the "clear muted vs. no-signal distinction" requirement be objectively verified without reviewing implementation code? [Measurability, Spec §FR-019a]
- [ ] CHK026 Are the "readable labels at all zoom levels" expectations quantified — e.g., a minimum font size or a relative-size rule (row height × fraction)? [Measurability, Gap]

## Scenario Coverage

- [ ] CHK027 Are requirements specified for the UX when the widget is resized very narrow (controls stack? hide? scroll horizontally?) — or is the minimum supported dock width defined? [Coverage, Gap]
- [ ] CHK028 Are requirements specified for the UX when the widget is very short (fewer rows visible than configured? vertical scroll? clip)? [Coverage, Gap]
- [ ] CHK029 Are requirements specified for the UX with 16 bins active (crowded — can all rows still fit in a typical dock? does the widget grow? do rows shrink below readability)? [Coverage, Gap]
- [ ] CHK030 Are requirements specified for the UX when all rows are simultaneously active (six concurrent decodes — scrolling all six at once is potentially visually overwhelming)? [Coverage]
- [ ] CHK031 Are requirements specified for the empty state "device configured but no signal" (does the widget show idle rows? a "no signal detected" placeholder? or just empty rows)? [Coverage, Assumption]
- [ ] CHK032 Are requirements specified for the transient state when the decoder is restarting after a bin-config change (FR-018) — is there a loading/transition indicator? [Coverage, Gap]
- [ ] CHK033 Are requirements specified for the SO2R UX when one decoder is muted (TX) and the other is actively decoding — are the two widgets visually distinct in this state? [Coverage, Spec §US8 scenario 5]

## Edge Case Coverage

- [ ] CHK034 Are UI requirements specified for the case where the operator clicks a token whose underlying text has already scrolled off-screen due to buffer trimming? [Edge Case, Gap]
- [ ] CHK035 Are UI requirements specified for a token that spans a line wrap or would be cut in half by the row boundary? [Edge Case, Gap]
- [ ] CHK036 Are UI requirements specified for two or more clickable tokens of different types overlapping in character range (e.g., a string that could match both a callsign and an RST shape)? [Edge Case, Gap]
- [ ] CHK037 Is the behavior defined when the operator repeatedly clicks the same token (debounce? allow each click to refill? visual indication of "already filled")? [Edge Case, Assumption "Same callsign token clicked repeatedly"]
- [ ] CHK038 Are requirements defined for the visual treatment of a row that is in "no lock" state (FR-016 + Assumption) — is there a visible indicator that the row cannot currently produce decode? [Edge Case, Gap]
- [ ] CHK039 Is the behavior defined for a spotlighted row when its bin count changes (e.g., operator was spotlighting row 5, then reduces bin count to 4 — does spotlight move, clear, or pin to the last row)? [Edge Case, Gap]

## Non-Functional UX Requirements

- [ ] CHK040 Are accessibility requirements (screen reader compatibility, high-contrast theme support) specified for the decoder widget? [Completeness, Non-Functional, Gap]
- [ ] CHK041 Are color requirements specified with accessibility-safe choices for operators with color-vision deficiency (don't rely on color alone to indicate muted state, no-lock state, or token type)? [Non-Functional, Gap]
- [ ] CHK042 Are requirements specified for how the widget respects the existing ContestLogX theme (light/dark) configuration? [Consistency, Gap]
- [ ] CHK043 Are requirements specified for the widget's behavior under rapid decoder state changes (e.g., token flashing on click — maximum flash rate to avoid photosensitive-triggering effects)? [Non-Functional, Gap]

## Dependencies & Assumptions

- [ ] CHK044 Is the dependency on the existing SO2R QSO-entry panel's field-update handler (the "same path as keyboard entry" in FR-024a) documented as an assumption that must hold for click-fill to work? [Dependency, Assumption]
- [ ] CHK045 Is the assumption that Qt6::Multimedia's device enumeration returns human-readable names consistently across platforms documented alongside the UX-level "device disappeared" empty state? [Dependency, Assumption, Research R5]

## Ambiguities & Conflicts

- [ ] CHK046 The spec says the spotlight row is "pinned-at-top" OR "larger font" OR "highlighted background" in the Assumptions — is the final chosen visual treatment resolved to ONE definition before implementation, or does the spec leave this genuinely undecided? [Ambiguity, Spec §Assumptions]
- [ ] CHK047 The spec says "row labels reflect the new bin center frequencies" (US4 scenario 1) and "each row labeled with the bin's center frequency" (FR-007) — is the label refresh behavior during the reconfigure transition specified (flicker-free? briefly empty? previous label held)? [Ambiguity, Gap]
- [ ] CHK048 Is there a potential conflict between FR-009 ("spotlight a single row for visual emphasis without suppressing decode in the other rows") and the spotlight assumption listing "pinned-at-top position" — if the spotlight row is re-ordered, does the row label remain its original frequency or does the row's identity shift? [Conflict, Ambiguity]

---

## Notes

- Check items off as completed: `[x]`
- Add comments or findings inline when an item resolves to "requirement missing" — these become candidate spec edits before `/speckit.tasks`
- Items tagged `[Gap]` indicate a requirement that is currently absent and likely needs a spec edit
- Items tagged `[Ambiguity]` indicate a requirement that is present but needs tightening
- Items tagged `[Conflict]` indicate two requirements that appear to contradict and need reconciliation
- Checklist total: 48 items across 7 categories
