# Performance Checklist: CW Decoder

**Purpose**: Validate that the CW Decoder spec's performance requirements are complete, clear, consistent, and measurable - before implementation begins. Items test the REQUIREMENTS, not the implementation.
**Created**: 2026-04-21
**Feature**: [spec.md](../spec.md)

---

## Requirement Completeness

- [ ] CHK049 Are requirements specified for end-to-end decode latency (audio-in → on-screen character) for every bin, not just aggregated across bins? [Completeness, Spec §SC-004]
- [ ] CHK050 Are requirements specified for per-bin CPU cost at different bin counts (6 default, 10, 16 maximum), not only for the default configuration? [Completeness, Gap, Spec §SC-009]
- [ ] CHK051 Are requirements specified for CPU cost with BOTH radios running decoders concurrently in SO2R mode (12 bins total)? [Completeness, Gap]
- [ ] CHK052 Are memory requirements quantified - per-bin text buffer size (~10,000 chars in Assumptions), ring buffer size (8,000 samples / 16 KB in research), total memory per session? [Completeness, Spec §Assumption "Scrolling buffer size", Research R6]
- [ ] CHK053 Are requirements specified for decoder startup time - from rig-config apply to first decoded character on a clean signal? [Completeness, Gap]
- [ ] CHK054 Are requirements specified for the reconfigure (FR-018: bin/passband change) completion time - is "within one second" the only target, or are interim states defined? [Completeness, Spec §FR-018]
- [ ] CHK055 Are requirements specified for worst-case performance during the audio callback - maximum acceptable callback execution time before audio drops? [Completeness, Gap]
- [ ] CHK056 Are requirements specified for sustained-operation resource growth - does the decoder accumulate memory or CPU cost over a multi-hour contest session? [Completeness, Gap]

## Requirement Clarity

- [ ] CHK057 Is "≤ 200 ms perceptible" quantified from a specific reference point - from the audio sample's arrival at the QAudioSource callback? from the bit being keyed on the radio? from the character's Morse-end-of-element timing? [Clarity, Ambiguity, Spec §SC-004]
- [ ] CHK058 Is "< 10% of one CPU core on a modern laptop" defined with a specific baseline hardware class (e.g., "a 2020-era x86_64 laptop CPU at its base clock")? [Clarity, Ambiguity, Spec §SC-009]
- [ ] CHK059 Is "< 2% of one CPU core" during audio silence clarified with respect to whether DSP continues (Goertzel runs every block) or throttles (e.g., poll less often when no signal)? [Clarity, Spec §SC-009]
- [ ] CHK060 Is "convergence within 10 characters of the first received character on a clean signal" clarified for the per-bin case - are 10 characters counted from that bin's first character, or from any bin's first character? [Clarity, Spec §SC-002]
- [ ] CHK061 Is "keystroke latency indistinguishable from the decoder-off baseline" quantified with a specific measurement method and threshold (e.g., "≤ 5 ms additional typing latency")? [Clarity, Ambiguity, Spec §SC-005]
- [ ] CHK062 Is "modern laptop" quantified with a specific year/spec, or at least a range (e.g., "any x86_64 laptop from 2019 or later")? [Clarity, Ambiguity, Spec §SC-009]

## Requirement Consistency

- [ ] CHK063 Are the 200 ms latency (SC-004), 10-character convergence (SC-002), and 5-character re-convergence on speed change (SC-003) internally consistent - i.e., do they all hold simultaneously on the same signal? [Consistency, Spec §SC-002, §SC-003, §SC-004]
- [ ] CHK064 Are the per-bin WPM independence (FR-016) and the global squelch (FR-010) consistent with the CPU budget (SC-009) - i.e., the spec doesn't pay twice for per-bin estimation? [Consistency]
- [ ] CHK065 Is the 6-bin default consistent with the CPU target - i.e., does the < 10% CPU target (SC-009) assume 6 bins or does it hold up to 16 bins? [Consistency, Ambiguity, Spec §SC-009]
- [ ] CHK066 Are the real-time latency targets (SC-004) and the block size decision (10 ms, per research R2) consistent - is it feasible to deliver < 200 ms end-to-end with 10 ms blocks plus Qt signal dispatch plus text rendering? [Consistency, Spec §SC-004, Research §R2]

## Acceptance Criteria Quality

- [ ] CHK067 Can SC-004 (latency ≤ 200 ms) be objectively measured by an independent tester - is a measurement procedure specified? [Measurability, Spec §SC-004]
- [ ] CHK068 Can SC-005 (no perceptible keystroke regression) be objectively measured, or is "indistinguishable" too subjective for acceptance testing? [Measurability, Ambiguity, Spec §SC-005]
- [ ] CHK069 Can SC-009 (CPU usage targets) be objectively measured on all three platforms, with a specific tool or metric named (e.g., `top`, Windows Performance Monitor, macOS Activity Monitor)? [Measurability, Gap]
- [ ] CHK070 Can SC-011 (two concurrent distinct-tone signals decode independently without cross-contamination) be objectively measured - is the test method specified? [Measurability, Spec §SC-011]

## Scenario Coverage

- [ ] CHK071 Are performance requirements specified for low-signal (just-above-squelch) decode - does latency or accuracy degrade gracefully? [Coverage, Gap]
- [ ] CHK072 Are performance requirements specified for high-rate decode bursts (e.g., 40 WPM sender, six simultaneous bins all active) - is decode accuracy maintained or does it drop? [Coverage, Gap]
- [ ] CHK073 Are performance requirements specified for rapid bin-config changes (operator repeatedly resizing passband or bin count) - is there a minimum inter-change interval? [Coverage, Gap]
- [ ] CHK074 Are performance requirements specified for repeated PTT cycling (CW QSK sending) - is there a mute/un-mute overhead that accumulates? [Coverage, Spec §Edge Case "Rapid PTT toggle during a run"]
- [ ] CHK075 Are performance requirements specified for the "no CW present" case (SSB voice, noise only) - does the decoder actively suppress Goertzel output to meet the silent-audio CPU budget? [Coverage, Spec §SC-009]
- [ ] CHK076 Are performance requirements specified for resource consumption when a decoder session is running but its widget is hidden (dock closed)? [Coverage, Gap]

## Edge Case Coverage

- [ ] CHK077 Is the worst-case audio-callback behavior quantified when the worker thread is temporarily descheduled (e.g., OS context-switch spike)? Is there a ring-buffer overflow budget? [Edge Case, Gap, Research §R6]
- [ ] CHK078 Are performance requirements specified for the cold-start first-second of audio capture, which may include device warmup and format negotiation? [Edge Case, Gap]
- [ ] CHK079 Are performance requirements specified for device-format fallback (unsupported format → default format) - is latency or CPU cost higher in the fallback path? [Edge Case, Gap, Spec §Edge Case "Audio device returns an unsupported format"]
- [ ] CHK080 Are performance requirements specified for bin-config validation edge cases (e.g., operator sets 16 bins in a narrow 200 Hz passband - is this gracefully refused without a CPU spike)? [Edge Case, Spec §Edge Case "Bin count vs passband sanity"]

## Non-Functional Requirements

- [ ] CHK081 Are performance requirements specified for the SPSC ring-buffer producer-consumer pair under sustained 8 kHz audio rate - is the buffer size (1 second / 8000 samples / 16 KB) sufficient under worst-case jitter? [Non-Functional, Research §R6]
- [ ] CHK082 Are jitter requirements (not just average latency) specified - e.g., 99th-percentile latency, or maximum observed latency under sustained operation? [Non-Functional, Gap]
- [ ] CHK083 Are thermal-throttling considerations addressed - does the spec specify behavior on sustained-load thermal throttling on a laptop running decoder for 6 hours? [Non-Functional, Gap]

## Dependencies & Assumptions

- [ ] CHK084 Is the assumption that Qt6::Multimedia's `QAudioSource` delivers audio without significant platform-level latency (on the order of one block, i.e., 10 ms) documented? [Assumption, Research §R1]
- [ ] CHK085 Is the assumption that the lock-free SPSC ring buffer implementation is correct documented, with a testing requirement to verify it? [Assumption, Research §R6]
- [ ] CHK086 Is the assumption that per-bin Goertzel is truly O(1) per sample (not O(N) per sample) documented so that scaling from 6 to 16 bins grows CPU linearly? [Assumption, Research §R2]

## Ambiguities & Conflicts

- [ ] CHK087 The spec says SC-005 is "indistinguishable from the decoder-off baseline" - is the baseline defined (which keystroke paths are measured? the mainWindow entry field's text-update? the textEdited signal handler?) or is the comparison operator-perceptual only? [Ambiguity, Spec §SC-005]
- [ ] CHK088 SC-009 says "< 10% CPU when CW is absent" but doesn't explicitly state the CPU budget when 6 bins ARE all actively decoding - is the implicit target the same 10%, or higher? [Ambiguity, Spec §SC-009]
- [ ] CHK089 The research doc specifies "8 kHz × 10 ms blocks" (R2) while the spec says 200 ms end-to-end latency (SC-004) - does the end-to-end budget leave enough headroom after the block boundary (10 ms) + ring-buffer drain + worker wakeup + Qt queued-connection dispatch + widget repaint? Is this budget breakdown specified anywhere? [Ambiguity, Conflict, Research §R2, Spec §SC-004]

---

## Notes

- Check items off as completed: `[x]`
- Items tagged `[Gap]` indicate a requirement currently absent that likely needs spec or SC-level addition
- Items tagged `[Ambiguity]` indicate a performance requirement that is present but needs tightening or a measurement procedure
- Items tagged `[Conflict]` indicate two requirements that appear to compete and need reconciliation (e.g., latency vs. CPU)
- Checklist total: 41 items across 7 categories
