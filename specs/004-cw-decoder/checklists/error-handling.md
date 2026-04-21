# Error-Handling Checklist: CW Decoder

**Purpose**: Validate that the CW Decoder spec's error-handling requirements are complete, clear, consistent, and measurable — before implementation begins. Items test the REQUIREMENTS, not the implementation.
**Created**: 2026-04-21
**Feature**: [spec.md](../spec.md)

---

## Requirement Completeness — Audio Path

- [ ] CHK090 Are requirements specified for the case where `QMediaDevices::audioInputs()` returns an empty list (no audio input devices available on the system)? [Completeness, Spec §Edge Case "No audio device available"]
- [ ] CHK091 Are requirements specified for the case where a previously-selected audio device no longer exists on application startup (device name persisted but not found)? [Completeness, Research §R5]
- [ ] CHK092 Are requirements specified for the case where the audio device disappears mid-session (USB unplug, virtual cable process exits, Bluetooth disconnect)? [Completeness, Spec §Edge Case "Configured audio device disappears mid-session"]
- [ ] CHK093 Are requirements specified for the case where the audio device reappears after previously disappearing — is auto-resumption required, or does the operator need to manually re-enable? [Completeness, Assumption "Audio device reconnection"]
- [ ] CHK094 Are requirements specified for the case where the audio device enumerates but rejects the decoder's preferred format (8 kHz / 16-bit / mono)? [Completeness, Spec §Edge Case "Audio device returns an unsupported format"]
- [ ] CHK095 Are requirements specified for the case where `QAudioSource::start()` returns a failure status (device busy, permission denied, format invalid)? [Completeness, Gap]
- [ ] CHK096 Are requirements specified for the case where audio samples arrive but the stream contains only silence (zero-amplitude samples, not background noise)? [Completeness, Gap]
- [ ] CHK097 Are requirements specified for the macOS microphone-permission-denied case (user denied the `NSMicrophoneUsageDescription` prompt)? [Completeness, Research §R7]

## Requirement Completeness — Rig Backend

- [ ] CHK098 Are requirements specified for the case where a rig backend NEVER emits `pttStateChanged` (e.g., MockedRigClient, or a backend that doesn't support PTT polling)? [Completeness, Spec §FR-019b, Research §R4]
- [ ] CHK099 Are requirements specified for the case where the rig backend emits `pttStateChanged(true)` but never follows with `pttStateChanged(false)` (stuck PTT state — operator's decoder would stay muted indefinitely)? [Completeness, Gap]
- [ ] CHK100 Are requirements specified for the case where the rig backend disconnects entirely (flrig crashed, network to rigctld dropped) while the decoder is running? [Completeness, Gap]
- [ ] CHK101 Are requirements specified for the case where the rig backend lags — PTT state reported with > 500 ms delay? [Completeness, Spec §Edge Case "Rig backend reports PTT state late or intermittently"]

## Requirement Completeness — Decoder Internal

- [ ] CHK102 Are requirements specified for the case where the decoder worker thread crashes or throws an exception (e.g., Goertzel hits numerical instability)? [Completeness, Gap]
- [ ] CHK103 Are requirements specified for the ring-buffer-full case (worker can't keep up with capture) — drop samples? block capture? log warning? [Completeness, Gap, Research §R6]
- [ ] CHK104 Are requirements specified for the case where `muteForInternalSend(durationMs)` is called twice in rapid succession (overlapping internal sends — second mute may be shorter than needed)? [Completeness, Gap, Research §R9]
- [ ] CHK105 Are requirements specified for the case where the internal-send duration estimate UNDER-estimates actual send time (flrig tail hasn't finished) — does the grace window fully cover it, or should the estimate be adjustable? [Completeness, Spec §Edge Case "Internal send duration underestimate"]
- [ ] CHK106 Are requirements specified for the case where `MainWindow::notifyInternalCwSend` is NOT called from a CW-send code path that should be calling it (a regression in an F-key handler not being updated)? [Completeness, Gap]
- [ ] CHK107 Are requirements specified for the WPM "no lock" recovery — once a bin enters "no lock", what event or condition transitions it back to "locked" state? [Completeness, Assumption "No lock state"]

## Requirement Completeness — UI / Click-Fill

- [ ] CHK108 Are requirements specified for a click-fill attempt when no contest is loaded (QSO-entry fields exist per Edge Cases, but what about FR-024a side effects like SCP that may require a contest)? [Completeness, Spec §Edge Case "Contest not loaded"]
- [ ] CHK109 Are requirements specified for a click-fill on a field that is read-only or grayed-out for some contest-specific reason (e.g., RST not used in that contest)? [Completeness, Gap]
- [ ] CHK110 Are requirements specified for a click-fill when the owning radio's entry panel is hidden (e.g., only Left entry visible, click on Right decoder's token)? [Completeness, Gap]

## Requirement Clarity

- [ ] CHK111 Is "falls back to always-active behavior and logs a one-time notice" quantified — what specifically is in the notice (text, where it appears, how long is "one-time" — session, run, forever)? [Clarity, Ambiguity, Spec §FR-019b]
- [ ] CHK112 Is the "Audio device unavailable" state (for a radio whose audio device disappeared) disambiguated from the normal "no decoder for this radio" state (no device ever configured)? [Clarity, Spec §Edge Case "Configured audio device disappears mid-session"]
- [ ] CHK113 Is "gracefully" (e.g., "decoder fails gracefully when...") quantified with specific observable behaviors (widget remains responsive? user message appears? app does not crash)? [Clarity, Ambiguity]
- [ ] CHK114 Is "cleanly gate on PTT assert and un-gate on PTT drop without corrupting any bin's internal timing state" specified concretely enough to test — what constitutes "corrupted internal timing state"? [Clarity, Spec §Edge Case "Rapid PTT toggle during a run"]
- [ ] CHK115 Is the logging destination specified — does "logs a one-time notice in the application debug log" mean the existing `DebugLogger` in the codebase (per CLAUDE.md)? [Clarity, Ambiguity, Spec §FR-019b]

## Requirement Consistency

- [ ] CHK116 Does the "hidden widget when no audio device" requirement (US1 acceptance scenario, Assumption) remain consistent with the "device disappeared mid-session" requirement (Edge Case) — does the widget hide mid-session or remain visible with an error state? [Consistency, Ambiguity]
- [ ] CHK117 Are the two PTT mute paths (FR-019a rig-backend and FR-019c internal-send) consistent under the failure modes — if rig-backend path fails (no PTT signal), does internal-send path still work reliably? [Consistency, Spec §FR-019a, §FR-019c, §FR-019b]
- [ ] CHK118 Does the "Mute on PTT" setting being OFF (FR-019d) consistently disable BOTH mute paths (not just one)? [Consistency, Spec §FR-019d]

## Acceptance Criteria Quality

- [ ] CHK119 Can "no crash" (from the edge-cases block) be objectively verified — is there an expected test procedure for each edge case (unplug device, kill rig backend, etc.)? [Measurability, Gap]
- [ ] CHK120 Can "no silent failure" be objectively defined — what specifically qualifies as a silent failure (logged message required? UI indication? both)? [Measurability, Ambiguity, Spec §FR-019b]
- [ ] CHK121 Are the recovery criteria for each failure mode measurable — e.g., "on device reconnect, first decoded character appears within N seconds"? [Measurability, Gap]

## Scenario Coverage — Exception & Recovery

- [ ] CHK122 Are requirements specified for recovery from decoder worker thread crash — restart worker? destroy session and require operator to re-enable? [Coverage, Exception Flow, Gap]
- [ ] CHK123 Are requirements specified for recovery from Qt6::Multimedia errors (`QAudioSource::error()`)? [Coverage, Exception Flow, Gap]
- [ ] CHK124 Are requirements specified for partial failure — e.g., Radio L's decoder is functional but Radio R's fails? [Coverage, Partial Failure, Gap]
- [ ] CHK125 Are requirements specified for the migration case — an operator upgrades from a previous ContestLogX version (no decoder settings exist) and first opens Rig Connection Settings? [Coverage, Recovery, Gap]
- [ ] CHK126 Are requirements specified for the "settings corrupted" case — e.g., a hand-edited QSettings file with a `binCount` of 999 or a negative passband? [Coverage, Defensive, Gap]

## Edge Case Coverage

- [ ] CHK127 Is the edge case specified for audio device index changing between sessions (OS assigns a different device index after reboot, but the description matches)? [Edge Case, Research §R5]
- [ ] CHK128 Is the edge case specified for the audio capture thread being unable to acquire the audio device at startup (race condition with another app holding exclusive access)? [Edge Case, Gap]
- [ ] CHK129 Is the edge case specified for audio format negotiation failure on the FALLBACK format too (i.e., both preferred and default formats fail)? [Edge Case, Gap, Spec §Edge Case "Unsupported format"]
- [ ] CHK130 Is the edge case specified for an audio device that produces data at a wildly different sample rate than requested (e.g., the OS silently resamples from 44.1 kHz — does Goertzel tolerate it)? [Edge Case, Gap]
- [ ] CHK131 Is the edge case specified for PTT state oscillation (rig emits `pttStateChanged(true), (false), (true), (false)` within 50 ms — possibly backend polling artifact)? [Edge Case, Gap]
- [ ] CHK132 Is the edge case specified for internal-send signalling overlapping rig-backend PTT (operator uses F-key to send CW, rig backend reports PTT mid-send — does the composite mute state behave correctly)? [Edge Case, Spec §Data Model "MuteState"]

## Dependencies & Assumptions

- [ ] CHK133 Is the dependency on the existing `DebugLogger` (per CLAUDE.md Core Modules) for error logging documented? [Dependency, Assumption]
- [ ] CHK134 Is the dependency on the rig backend emitting `pttStateChanged` (newly added per Research §R4) documented as a prerequisite that MUST land before the decoder's PTT wiring can be tested? [Dependency, Research §R4]
- [ ] CHK135 Is the assumption that Qt6::Multimedia delivers a stable `QAudioSource::error()` signal on all three platforms documented? [Assumption, Gap]

## Ambiguities & Conflicts

- [ ] CHK136 The spec has a "No lock state" assumption but no explicit error requirement for the case where ALL bins are in "no lock" for an extended period (e.g., decoder has been running for 10 minutes but no bin ever locks) — is this an error condition, or silent by design? [Ambiguity, Gap]
- [ ] CHK137 The spec's Edge Case "Rig backend reports PTT state late or intermittently" acknowledges self-decode may leak briefly, but does not specify the maximum tolerable leak (how many spurious characters is acceptable before the operator should consider the PTT-mute feature broken)? [Ambiguity, Spec §Edge Case "Rig backend reports PTT state late or intermittently"]
- [ ] CHK138 The spec says "no silent failure" but also says the mocked rig (which doesn't report PTT) results in "fallback to always-active + log" — is this considered silent (no user-visible error) or not-silent (log is sufficient)? [Ambiguity, Conflict, Spec §FR-019b]

---

## Notes

- Check items off as completed: `[x]`
- Items tagged `[Gap]` indicate a failure-mode requirement currently absent that likely needs spec edit
- Items tagged `[Ambiguity]` indicate an error-handling requirement present but needs tightening
- Items tagged `[Conflict]` indicate two requirements that appear to contradict on failure behavior
- Checklist total: 49 items across 9 categories
