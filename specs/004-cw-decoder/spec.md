# Feature Specification: CW Decoder

**Feature Branch**: `004-cw-decoder`
**Created**: 2026-04-21
**Status**: Draft
**Input**: Dockable audio-based Morse decoder with per-radio audio input, continuously adaptive WPM, and click-to-fill callsign and RST tokens routed to the owning radio's QSO entry panel

---

## Clarifications

### Session 2026-04-21

- Q: Single-channel decoder (one tone, one stream) or multi-channel skimmer-style (parallel bins across the passband, each with its own stream)? → A: Multi-channel skimmer-style. The decoder runs N parallel single-frequency detectors across the configured passband (default 6 bins at 100 Hz spacing from 400 Hz to 1000 Hz), each bin producing an independent stream of decoded characters rendered as its own horizontal row. Every bin maintains its own continuously-adaptive WPM estimate since different signals on different bins may be sent at different speeds. The "pin tone" concept from the original draft is replaced by a "spotlight row" — the operator can visually emphasize a single bin without suppressing decode in the others.
- Q: What should the decoder do when the owning radio is transmitting (to avoid decoding the operator's own keyed sidetone)? → A: Per-radio "Mute decoder on PTT" setting in Rig Connection Settings, default ON. While the owning radio is in TX (as reported by its rig backend's PTT state), every bin of that radio's decoder is gated — no new characters are added to any row, no token detection runs — until PTT drops. If the rig backend does not report PTT state (e.g., mocked rig, or a backend that does not expose PTT), the decoder falls back to always-active behavior and logs a one-time notice; the operator can still disable the "Mute on PTT" setting if needed. The setting is stored in the per-radio rig configuration alongside the audio device selection.
- Q: When the operator clicks a decoded token to fill CALL or RSTr, what side effects should fire (SCP lookup, call-history lookup, dupe check, name/QTH auto-fill)? → A: Click-to-fill is behaviorally identical to keyboard typing the same characters into the field. Every downstream action that would fire for keyboard entry — SCP lookup, call-history lookup, dupe check, name/QTH auto-fill — fires identically for click-fill. The only behavioral difference is focus: clicking does not steal focus from wherever the operator is currently typing. This keeps the operator's mental model consistent (click = type) and means future-added side effects automatically apply to click-fill without additional code paths.
- Q: How does the decoder know to mute when ContestLogX itself sends CW via F-key memories or the CW console, given that rig-backend PTT state may not reliably track cwio_text-style sends? → A: MainWindow explicitly signals the owning radio's decoder when it initiates a CW send, and the decoder mutes that radio's bins for an estimated duration (text length ÷ current send WPM, converted to time) plus a small grace window (e.g., 250 ms). This is additive to the rig-backend PTT-state path from the prior clarification — backend PTT still mutes when the operator keys manually through the radio, and internal-send signalling mutes when the operator uses ContestLogX's CW macros. Both paths respect the per-radio "Mute decoder on PTT" setting: if the setting is OFF, neither path mutes.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Per-Radio Audio Input Configuration (Priority: P1)

A contest operator opens the Rig Connection Settings dialog to configure the radio. Alongside the existing fields (backend, host, port, CAT serial port), they now see an **Audio Input Device** dropdown. They select the system audio input that their radio's receive audio is routed through (a USB audio endpoint on the radio, a soundcard line-in, or a virtual audio cable). The selection persists across sessions. In SO2R mode, this configuration is independent for Radio L and Radio R — the operator can configure an audio device for either, both, or neither radio. A radio with no audio device configured simply has no decoder — this is a legal and common setup.

**Why this priority**: Without a clear configuration model, nothing else in this feature works. This story establishes the primary data binding (audio ↔ radio) that makes SO2R click-routing deterministic. Every other user story depends on this.

**Independent Test**: Open Rig Connection Settings, select an audio device for Radio L only, save, restart the app. Verify the selection persisted. Verify that a decoder panel appears for Radio L and does not appear for Radio R. Change the selection to "(none)" for Radio L, save — the decoder panel disappears.

**Acceptance Scenarios**:

1. **Given** the operator opens Rig Connection Settings, **When** they view the Radio L tab, **Then** they see an "Audio Input Device" dropdown listing all system audio input devices plus an explicit "(none)" option.

2. **Given** an audio device is selected for Radio L and "(none)" is selected for Radio R, **When** the dialog is saved, **Then** on application restart the same selections are still present and a decoder panel is active for Radio L only.

3. **Given** SO2R is enabled with audio devices set for both radios, **When** the operator works in the app, **Then** two independent decoder instances operate concurrently without interfering.

4. **Given** SO2R is not enabled and an audio device is set for the single radio, **When** the application starts, **Then** one decoder panel is active bound to that radio.

5. **Given** "Mute decoder on PTT" is enabled for Radio L and SO2R is active, **When** Radio L goes into transmit (e.g., operator sends CW), **Then** all of Radio L's decoder rows gate immediately (no new characters, no token detection) and Radio R's decoder continues decoding normally; when PTT drops, Radio L resumes decoding on all bins.

---

### User Story 2 - Multi-Channel Real-Time CW Decoding (Priority: P1)

A contest operator has configured an audio input device and has CW signals arriving through that device. The decoder panel displays incoming Morse text from the audio across N parallel frequency bins (default 6 bins at 100 Hz spacing across the configured passband). Each bin renders as its own horizontal row of scrolling text, with the bin's center frequency labeled at the left. Multiple stations audible in the passband at different tones each decode on their own row simultaneously. The operator can read every active signal in the passband at a glance without retuning.

**Why this priority**: This is the core value of the feature. Multi-channel skimmer-style decoding is strictly more powerful than single-channel for contest use — the operator sees every active CW signal in the passband at once. Without real-time decoding nothing else matters; this is the MVP. Even without click-to-fill or SO2R routing, the ability to read six concurrent CW streams delivers immediate operator value.

**Independent Test**: Feed two known CW signals at distinct tones within the passband (e.g., 500 Hz and 800 Hz, each at 25 WPM) into the configured audio device. Verify that two different rows of the decoder panel — the 500 Hz bin and the 800 Hz bin — each decode their respective stream independently and that a silent bin (e.g., 700 Hz) produces no output.

**Acceptance Scenarios**:

1. **Given** a CW signal at 500 Hz is present on the configured audio device at 25 WPM, **When** the operator watches the decoder panel, **Then** decoded characters appear in the row labeled ~500 Hz within 200 ms of each character being sent, and other bins remain empty.

2. **Given** two CW signals at distinct tones within the passband (e.g., 500 Hz and 800 Hz) are being sent simultaneously, **When** the operator watches the decoder panel, **Then** the two signals decode independently in their respective rows with no cross-contamination.

3. **Given** the audio device is silent, **When** the operator watches the decoder panel, **Then** no spurious characters appear in any row.

4. **Given** the decoder has been running and multiple rows have accumulated text, **When** the operator clicks the "Clear" button, **Then** every row is cleared while decoding continues on all bins.

---

### User Story 3 - Continuously Adaptive Speed (WPM) (Priority: P1)

A contest operator is copying CW from one station, then tunes to a new station operating at a different speed (e.g., from 25 WPM to 35 WPM). The decoder continuously measures the sender's dot length and updates its receive-WPM estimate in real time. The operator never manually sets a WPM. A live WPM readout in the decoder panel shows the current estimate. The operator configures only a bounding range (default 5–60 WPM) that prevents noise bursts from driving the estimator to implausible values.

**Why this priority**: Contest operators encounter a wide range of speeds — 15 WPM casual CW, 40+ WPM top-of-band contesters, and everything between. A decoder that requires manual speed setting is effectively unusable in a contest environment. Continuous adaptation is mandatory, not optional.

**Independent Test**: Send CW at 20 WPM for 30 seconds, then switch to 35 WPM. Verify the decoder's live WPM readout tracks the change and that decoded text remains accurate within a small number of characters after the speed change.

**Acceptance Scenarios**:

1. **Given** CW is being sent at 25 WPM, **When** the operator views the decoder panel, **Then** the live WPM readout shows a value close to 25 (within the estimator's tolerance) and decoded text is accurate.

2. **Given** the sender changes speed from 25 WPM to 35 WPM mid-transmission, **When** the operator watches the decoder, **Then** the WPM readout converges to 35 and decoded text resumes accuracy within a small number of characters of the change.

3. **Given** the operator has configured the bounding WPM range to 15–45, **When** noise bursts suggest an implausible 80 WPM, **Then** the estimator stays within 15–45 and does not drift out of bounds.

4. **Given** the decoder is running, **When** the operator opens the panel, **Then** there is no manual WPM entry control — only the live readout and the bounding-range setting.

---

### User Story 4 - Passband, Bin Configuration, and Spotlight Row (Priority: P2)

A contest operator wants to customize the decoder's frequency coverage to match their operating style. They open the decoder panel's settings and adjust the passband edges (default 400–1000 Hz), bin count (default 6), and therefore bin spacing (default 100 Hz). They can also "spotlight" a single row — giving it visual emphasis (larger font, highlighted background, or pinned position at top) without suppressing decode on any other bin. The spotlight is a display aid, not a decode gate; every bin continues to decode in the background and can be un-spotlighted at any time.

**Why this priority**: Operators have preferences for sidetone pitch (some prefer 500 Hz, some 700 Hz) and different coverage needs (narrow for a crowded band, wide for quiet conditions). Making the passband and bin configuration tunable is important but not blocking — the defaults work for most cases. Spotlight is a quality-of-life feature for focusing on a specific station within a busy passband.

**Independent Test**: Change the passband to 300–900 Hz with 4 bins and verify the row labels shift to 300/450/600/750/900 Hz center frequencies. Spotlight the 600 Hz row and verify it is visually emphasized while the other rows continue to scroll normally.

**Acceptance Scenarios**:

1. **Given** the decoder is running with default settings, **When** the operator opens the bin-configuration control and changes the passband to 300–900 Hz with 4 bins, **Then** the decoder restarts its DSP and the row labels reflect the new bin center frequencies.

2. **Given** multiple rows are decoding simultaneously, **When** the operator spotlights one row, **Then** that row receives visual emphasis while all other rows continue to decode and scroll normally.

3. **Given** a row is spotlighted, **When** the operator un-spotlights it, **Then** the row returns to its default visual treatment and all rows remain active.

4. **Given** an invalid bin configuration is requested (e.g., 50 bins — more than the passband can cleanly support), **When** the operator applies it, **Then** the system warns and either refuses or clamps to a valid range.

---

### User Story 5 - Click-to-Fill CALL Token (Priority: P1)

A contest operator sees a decoded callsign in any of the decoder panel's rows (e.g., "CQ TEST K1ABC K1ABC" appearing in the 650 Hz bin row). They click the callsign "K1ABC" in that row. The CALL field of the QSO entry panel for the **owning radio** (the radio whose audio produced that decode, regardless of which bin the decode came from) is immediately populated with "K1ABC". In SO2R, this is deterministic: clicking a token in any row of the Radio L decoder always fills Radio L's CALL field, regardless of which radio is currently active for keyboard input. Clicking does not steal keyboard focus from wherever the operator is typing. The bin (row) the click came from is informational only — routing targets the owning *radio*, not the owning bin.

**Why this priority**: This is the primary workflow payoff of the feature. The decoder exists to reduce typing during fast-paced QSOs. Routing to the owning radio (not the active radio) is essential for SO2R correctness — otherwise clicks in the left decoder would unpredictably land in the right radio's entry.

**Independent Test**: In single-radio mode, trigger a decode containing "K1ABC", click the callsign in the scrolling text, verify CALL field is filled with "K1ABC" and keyboard focus remains where it was. Then enable SO2R: with Radio L active for keyboard, click a callsign in Radio R's decoder — verify it fills Radio R's CALL field and keyboard focus remains on Radio L's entry.

**Acceptance Scenarios**:

1. **Given** the decoder has decoded the text "CQ K1ABC K1ABC", **When** the operator clicks "K1ABC" in the scrolling text, **Then** the CALL field in the owning radio's QSO entry panel is populated with "K1ABC".

2. **Given** SO2R is active with Radio L as the currently-focused entry and a callsign appears in Radio R's decoder, **When** the operator clicks that callsign, **Then** Radio R's CALL field is filled and keyboard focus remains on Radio L's entry.

3. **Given** the CALL field already contains text, **When** the operator clicks a new callsign token, **Then** the previous contents are replaced with the clicked callsign.

4. **Given** a decoded token is ambiguous (could be a callsign or an exchange), **When** the operator clicks it, **Then** the system applies a callsign-shaped pattern check and only fills CALL if the token matches the callsign pattern.

---

### User Story 6 - Click-to-Fill RST Token (Priority: P2)

A contest operator sees a decoded signal report in any row of the decoder panel (e.g., "599 001"). They click "599". The RSTr field of the owning radio's QSO entry panel is populated with "599". The same owning-radio routing rule as CALL applies — routing is by radio, not by bin. The decoder recognizes several RST formats in use in contest CW: standard three-digit RST (e.g., `599`), CW short-form with N-for-9 (e.g., `5NN`), and two-digit signal reports used in some exchanges (e.g., `57`).

**Why this priority**: RST fill saves less time than CALL fill (operators often use a fixed "599" for CW contests), but for contests where signal reports vary or for operators who log actual reports, it is a meaningful workflow improvement.

**Independent Test**: Decode "599", click it, verify RSTr is populated with "599". Decode "5NN", click it, verify RSTr is populated with either "5NN" or the normalized "599" (consistent with how the operator keyboards it).

**Acceptance Scenarios**:

1. **Given** the decoder has decoded "599 001", **When** the operator clicks "599", **Then** the RSTr field in the owning radio's QSO entry panel is populated with "599".

2. **Given** the decoder has decoded "5NN", **When** the operator clicks "5NN", **Then** the RSTr field is populated with a valid RST value (normalization policy documented in Assumptions).

3. **Given** RST token detection is ambiguous, **When** the operator clicks, **Then** only tokens matching a recognized RST-shaped pattern fill RSTr — non-RST tokens do not.

---

### User Story 7 - Noise Squelch (Priority: P2)

A contest operator is on a noisy band. Without filtering, the decoder produces spurious characters as the DSP mistakes noise for Morse elements. The operator adjusts a squelch/threshold slider in the decoder panel. Decoded output stops when the signal strength is below the threshold and resumes when the signal is present. The threshold setting persists across sessions.

**Why this priority**: Without a squelch, the decoder is unusable on noisy bands — the scrolling view fills with garbage. But it is not the first thing the operator needs; it is a refinement on top of the core decode.

**Independent Test**: With a known-noise audio source (e.g., band noise with no CW), verify the decoder produces no output when the squelch is above the noise level and produces output when it is below. Verify the threshold setting is saved and restored after restart.

**Acceptance Scenarios**:

1. **Given** the audio contains only background noise below the squelch threshold, **When** the operator watches the decoder, **Then** no characters are decoded.

2. **Given** the operator adjusts the squelch slider while audio is streaming, **When** the threshold crosses the signal level, **Then** the decoder output starts or stops immediately in response.

3. **Given** the operator sets a squelch value and exits the application, **When** they restart, **Then** the same squelch value is restored.

---

### User Story 8 - SO2R Independent Decoding (Priority: P1)

A contest operator running SO2R has audio devices configured for both Radio L and Radio R (for example, via separate USB audio endpoints on two radios, or via a virtual audio cable split). Each radio has its own independent decoder running in real time. Decoded text from Radio L and Radio R appear in separate panels, each labeled with the owning radio. Click-to-fill from either panel targets that panel's owning radio's QSO entry, regardless of which radio is currently active for keyboard input. The operator can switch keyboard focus between radios (using the existing SO2R backtick shortcut) without affecting which decoder is feeding which entry — the audio-to-radio binding is fixed at configuration time.

**Why this priority**: SO2R is a first-class supported mode in ContestLogX. A decoder that only works cleanly for one radio would be a regression from the existing SO2R capabilities. Getting this right depends on the per-radio audio-device binding (User Story 1) being solid.

**Independent Test**: Enable SO2R with audio configured for both radios. Feed different CW signals into the two audio devices. Verify two decoder panels run concurrently and decode independently. Set keyboard focus to Radio L, click a callsign in Radio R's decoder, verify it fills Radio R's CALL field without changing keyboard focus.

**Acceptance Scenarios**:

1. **Given** SO2R is active with both audio devices configured, **When** CW is received on both radios simultaneously, **Then** each decoder panel decodes its own radio's audio independently with no cross-contamination.

2. **Given** Radio L is the active keyboard-entry radio and a callsign appears in Radio R's decoder, **When** the operator clicks that callsign, **Then** Radio R's CALL field is filled and Radio L remains the active keyboard-entry radio.

3. **Given** audio is configured only for Radio L, **When** SO2R is enabled, **Then** a decoder panel exists for Radio L only and the SO2R-active-radio toggle behaves normally for Radio R (no dead decoder UI, no crash).

4. **Given** no audio is configured for either radio, **When** SO2R is enabled, **Then** no decoder panel is shown at all, and all other SO2R functionality is unaffected.

---

### Edge Cases

- **No audio device available on the system**: When `QMediaDevices` reports no input devices, the Rig Connection Settings "Audio Input Device" dropdown shows only the "(none)" option; no decoder can be started. The app does not crash or block; other functionality is unaffected.
- **Configured audio device disappears mid-session**: If the operator unplugs the USB audio device (or the virtual cable process exits), the decoder for that radio shows a clear "Audio device unavailable" state and stops consuming CPU. On reconnection, decoding resumes automatically or on operator prompt (resumption policy documented in Assumptions).
- **Audio device enumerates but returns an unsupported format**: The decoder falls back to a platform-default format; if that also fails, it enters an error state with a message, and the rest of the app continues.
- **Contest not loaded when operator clicks a token**: The QSO entry fields exist (they are present even without a contest), so the fill still works. If contest-specific validation would normally reject the field value, that validation runs as it would for keyboard input.
- **Same callsign token clicked repeatedly**: Each click refills the CALL field with the same value; there is no accumulation or append.
- **Decoder output contains characters that are not ASCII**: Morse produces ASCII letters and numbers only; any prosigns (AR, SK, BT, etc.) are decoded as their conventional multi-character representations and are not clickable as tokens.
- **Decoder running at application exit**: Audio capture stops cleanly; no dangling threads, no crash on shutdown.
- **WPM adaptation fails to lock** (per-bin): If the signal in a bin is too weak or too noisy to reliably classify dots and dashes within the bounding range, that bin's row produces no output and its per-bin live WPM readout shows a clear "no lock" state. Other bins are unaffected.
- **Signal straddles two bins**: If a CW signal's tone frequency sits near a bin boundary, it may decode partially in both adjacent bins. The operator is expected to adjust the passband/bin configuration or simply read whichever row is cleaner; the decoder does not attempt cross-bin fusion.
- **Bin count vs passband sanity**: If the operator configures an impractically high bin count for the passband (e.g., 50 bins in a 600 Hz passband, producing 12 Hz spacing that is below the Goertzel resolvability for reasonable block sizes), the system warns and either refuses or clamps the configuration.
- **All bins idle**: When the audio signal is present but contains no CW (e.g., SSB voice), no row produces output and CPU stays low.
- **Rapid PTT toggle during a run**: The decoder must cleanly gate on PTT assert and un-gate on PTT drop without corrupting any bin's internal timing state; repeated fast keying (e.g., CW QSK) must not cause spurious characters immediately after each un-gate event.
- **Rig backend reports PTT state late or intermittently**: If PTT state lags the actual radio by a small amount, the decoder may briefly decode the operator's own keying at the start/end of a transmission. Tolerance: up to approximately one backend poll interval of leaked self-decode per TX event (typically ≤ 500 ms on flrig's default polling, corresponding to at most ~5 decoded characters at 40 WPM). If an operator consistently observes more than this, either (a) the per-radio "Mute decoder on PTT" setting is OFF, (b) the rig backend polls PTT slower than 500 ms (investigate), or (c) the operator should rely on the internal-send signalling path (FR-019c) which is independent of backend accuracy and covers all ContestLogX-initiated sends.
- **Internal send duration underestimate**: If ContestLogX's estimated send duration finishes before the radio has actually stopped transmitting (e.g., flrig is still spooling the tail of a long CW macro), the decoder may briefly un-mute while TX is still active. The grace window mitigates this; operators with chronic underestimation can raise the grace window in settings.

---

## Requirements *(mandatory)*

### Functional Requirements

**Audio Configuration**

- **FR-001**: The Rig Connection Settings dialog MUST expose an "Audio Input Device" selector for each radio (Radio L, and Radio R when SO2R is enabled), listing all system audio input devices plus an explicit "(none)" option.
- **FR-002**: The selected audio device per radio MUST persist across application sessions.
- **FR-003**: "(none)" MUST be a legal selection for any radio; a radio with "(none)" selected MUST have no decoder active.
- **FR-004**: In SO2R mode, the audio device settings for Radio L and Radio R MUST be independent — any combination (neither, L only, R only, both) is legal.
- **FR-004a**: The Rig Connection Settings dialog MUST expose a per-radio "Mute decoder on PTT" boolean setting, default ON, stored alongside the audio device selection.

**Decoder Widget**

- **FR-005**: The CW Decoder MUST be presented as a dockable, floatable, closable panel consistent with other ContestLogX dock widgets.
- **FR-006**: A decoder panel MUST exist for every radio that has an audio device configured and MUST NOT exist for radios with "(none)" selected.
- **FR-007**: Each decoder panel MUST display decoded Morse text as a stack of N horizontal rows (one per frequency bin), each row labeled with its bin's center frequency and each row independently scrolling decoded characters, with new characters appearing within 200 milliseconds of the corresponding audio arriving.
- **FR-008**: Each row MUST display its own live WPM readout updated continuously as that bin's speed estimate changes; WPM tracking is independent per bin.
- **FR-009**: The decoder panel MUST allow the operator to "spotlight" a single row for visual emphasis without suppressing decode in the other rows; the spotlighted row returns to normal appearance when un-spotlighted.
- **FR-010**: Each decoder panel MUST provide a squelch / threshold control that suppresses output when signal strength is below the set level; the squelch applies uniformly to all bins.
- **FR-011**: The decoder panel MUST allow the operator to configure the passband edges (low Hz and high Hz) and the number of bins, and MUST compute the bin center frequencies as evenly-spaced points across the passband.
- **FR-012**: Each decoder panel MUST provide a "Clear" control that wipes the on-screen scrolling text in every row without affecting the audio stream or per-bin decoder state.
- **FR-013**: The operator MUST NOT be able to set WPM manually for any bin — only the live-estimated value per bin is displayed.
- **FR-014**: The operator MUST be able to set a single bounding WPM range (min and max) that applies uniformly to every bin's estimator; the default range is 5–60 WPM.

**Decoding Behavior**

- **FR-015**: The decoder MUST perform parallel single-frequency tone detection across N bins spaced evenly within the configured passband; full-spectrum FFT / waterfall / panadapter analysis remains out of scope.
- **FR-016**: Each bin MUST continuously adapt its WPM estimate from the measured dot length of its own received signal and feed the updated estimate back into that bin's dot/dash classifier in real time. WPM estimates do not cross between bins.
- **FR-017**: WPM adaptation in every bin MUST be bounded by the operator-configured min/max range so that noise bursts cannot drive any bin's estimate out of bounds.
- **FR-018**: When bin configuration (passband or bin count) is changed, the decoder MUST restart its DSP cleanly and reflect the new bin layout in the panel within one second.
- **FR-019**: The decoder MUST suppress output in a bin whose audio signal strength is below the operator-configured squelch threshold.
- **FR-019a**: When "Mute decoder on PTT" is enabled for a radio AND that radio's rig backend reports PTT active, the decoder for that radio MUST gate all bins — no new characters added to any row, no token detection — until PTT drops. Other radios' decoders are unaffected.
- **FR-019b**: If the owning radio's rig backend does not report PTT state, the decoder MUST fall back to always-active behavior for rig-backend PTT muting and log a one-time notice in the application debug log; no crash, no silent failure. (The internal-send mute path in FR-019c remains active and independent.)
- **FR-019c**: When ContestLogX itself initiates a CW send for a radio (F-key memory, CW console, or any other internal send path), the application MUST signal the owning radio's decoder to mute its bins for the estimated send duration (character count ÷ current send WPM, converted to time) plus a configurable grace window (default 250 ms). This mute path is additive to FR-019a (rig-backend PTT mute) and is independent of backend PTT reporting accuracy.
- **FR-019d**: Both mute paths (rig-backend PTT in FR-019a and internal-send signalling in FR-019c) MUST respect the per-radio "Mute decoder on PTT" setting from FR-004a; if the setting is OFF for a radio, neither path mutes that radio's decoder.

**Click-to-Fill**

- **FR-020**: Clicking a callsign-shaped token in a decoder panel MUST populate the CALL field of that decoder panel's owning radio's QSO entry, replacing any previous CALL contents.
- **FR-021**: Clicking an RST-shaped token (recognized formats: three-digit e.g. `599`, CW short-form e.g. `5NN`, two-digit e.g. `57`) MUST populate the RSTr field of the owning radio's QSO entry.
- **FR-022**: Click-to-fill MUST NOT steal keyboard focus from wherever the operator is currently typing.
- **FR-023**: Click-to-fill MUST route to the owning radio's entry panel (the radio bound to the decoder whose text was clicked), NOT to the currently-active keyboard-entry radio — this binding is deterministic from the decoder-to-radio configuration.
- **FR-024**: Only tokens matching a recognized callsign or RST shape MUST be clickable; plain decoded text that does not match any recognized pattern MUST NOT be interactive.
- **FR-024a**: Click-to-fill MUST trigger the same downstream actions that keyboard entry of the same characters would trigger — specifically: Super Check Partial (SCP) lookup, call-history lookup, dupe check, and name/QTH auto-fill. The only behavioral difference from keyboard entry is that click-fill does not transfer keyboard focus to the populated field.

**Persistence**

- **FR-025**: The decoder's per-radio runtime settings (squelch, passband edges, bin count, spotlight-row index, bounding WPM range) MUST persist across application sessions.
- **FR-026**: Decoder panel layout state (dock position, size, floating/docked) MUST persist via the existing application state-persistence mechanism.

**Non-Interference**

- **FR-027**: QSO entry keyboard latency MUST NOT regress when one or more decoders are running.
- **FR-028**: The CW Decoder MUST NOT auto-log any QSO based on decoder output. The operator's keyboard input remains the sole authority for what gets logged.
- **FR-029**: The decoder MUST NOT require any change to flrig or existing rig-control behavior; it operates purely as an audio consumer.

**Platform**

- **FR-030**: The decoder MUST function on Linux, macOS, and Windows using system audio input devices exposed by the OS.

### Key Entities

- **Audio Input Device Binding**: The association between a specific radio (L or R) and a specific system audio input device. Attributes: radio identity, device identifier. Nullable (a radio may have no binding). Persisted in rig configuration.
- **Decoder Session**: A live decoder running against a specific Audio Input Device Binding. Attributes: owning radio, passband edges (low/high Hz), bin count, spotlight-row index, squelch threshold, bounding WPM range, and a collection of Bin Channels.
- **Bin Channel**: One of N parallel single-frequency decoders within a Decoder Session. Attributes: center frequency, current live WPM estimate, lock/no-lock state, scrolling decoded text buffer. Each Bin Channel operates independently of the others.
- **Decoded Token**: A contiguous run of decoded characters (within a single Bin Channel's buffer) that matches a recognized pattern (callsign or RST). Attributes: text value, pattern type, originating Bin Channel, byte range in that channel's buffer, clickable state.

---

## Assumptions

These are reasonable defaults used where the user description did not specify a value. Any of these may be revisited during `/speckit.clarify`.

- **Widget layout (SO2R)**: Two separate dock panels (one per radio), each labeled with its owning radio ("Radio L Decoder", "Radio R Decoder"), mirroring the existing SO2R QSO-entry-panel layout. Single-radio mode shows one unlabeled panel.
- **Decoder panel when no audio configured**: Hidden entirely, not shown as an empty/disabled panel. The operator sees UI only for radios that have audio to decode.
- **Passband and bin defaults**: 400–1000 Hz with 6 bins at 100 Hz spacing (bin centers at 400, 500, 600, 700, 800, 900 Hz, plus a 1000 Hz upper bound — exact placement finalized during implementation). Operator-configurable.
- **Squelch scope**: One global squelch value per decoder session applies uniformly to every bin. Per-bin squelch is out of scope for the first release.
- **Spotlight row**: Visual emphasis via a highlighted background color (theme-matched — a distinct, subdued accent on the spotlighted row's background while non-spotlight rows use the default panel background). No font-size change, no row re-ordering, no position change. Decode continues unchanged in every row; click-to-fill routing is unaffected.
- **Click-to-fill overwrite policy**: Clicking always replaces the current field contents, consistent with how the DX Cluster row-click fill already behaves in ContestLogX.
- **Click-to-fill side effects (all)**: Clicking any decoded token is behaviorally identical to keyboard entry of the same characters — SCP lookup, call-history lookup, dupe check, and name/QTH auto-fill all fire identically. The only difference is that keyboard focus is not stolen from wherever the operator is currently typing.
- **RST normalization**: CW short-form reports (`5NN`, `4NN`) are accepted as clicked tokens and stored as-typed; the contest engine's existing RST handling decides whether to normalize (no new normalization logic is introduced by this feature).
- **Callsign token recognition**: Matches standard international callsign patterns including slash-notation (e.g., `PJ2/N9OH`, `YB1AR/2`). Prosigns and non-callsign text are never clickable as CALL tokens.
- **Audio device reconnection**: If a configured audio device disappears mid-session and then reappears, the decoder auto-resumes without operator intervention. The "Audio device unavailable" indicator clears when capture resumes.
- **Scrolling buffer size**: The on-screen scrolling text retains the most recent ~10,000 characters; older characters are discarded to prevent unbounded memory growth. "Clear" resets the buffer.
- **"No lock" state**: When the decoder cannot establish a plausible WPM within the bounding range, the live WPM readout shows a dashed or blank value rather than a number, and no characters are emitted.

---

## Out of Scope

- Sending CW (the existing CW console and flrig own keying).
- SSB voice-to-text decoding.
- RTTY, FT8, PSK, or any digital-mode decoding (the existing WSJT-X UDP listener covers FT8).
- Full-spectrum audio analysis (waterfall, panadapter, visual spectrum display).
- Network sharing of decoded text across multi-op stations.
- Simultaneous multi-signal separation within a single passband — the operator is expected to tune such that only one signal is within the passband at a time.
- Direct audio capture from flrig (no flrig API for this exists; audio is a system-level routing concern).
- Auto-logging a QSO based on decoder output.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a clean audio signal at 25 WPM centered in a bin, that bin's decoded text matches the sent text with ≥ 95% character accuracy.
- **SC-002**: In any bin receiving a clean signal, the bin's live WPM readout converges to within ±2 WPM of the true sender speed within 10 characters of the first received character.
- **SC-003**: When the sender in a bin changes speed by ±10 WPM mid-transmission, that bin re-converges to the new speed within 5 additional characters and decode accuracy returns to the SC-001 baseline within the same window.
- **SC-004**: End-to-end latency from audio-in to decoded character appearing in the corresponding row is ≤ 200 ms perceptible at 25 WPM for every bin that has a signal.
- **SC-005**: With a decoder running at 40 WPM on a loud signal across all 6 default bins, QSO-entry keystroke latency is indistinguishable from the decoder-off baseline (no perceptible regression during a 60-second typing test).
- **SC-006**: In SO2R mode with audio configured for both radios, clicking a callsign token in any row of one radio's decoder fills that radio's CALL field in 100% of test cases regardless of which radio is active for keyboard entry, and keyboard focus remains on the active radio in 100% of test cases.
- **SC-007**: When no audio input device is configured for a radio, no decoder panel is shown for that radio and no CPU is consumed by audio capture for that radio.
- **SC-008**: The feature builds and runs on Linux, macOS, and Windows with identical user-facing behavior, and the continuous-integration build succeeds on all three platforms.
- **SC-009**: With audio present but no CW signal (e.g., SSB or noise only) across 6 default bins, the decoder consumes < 10% of one CPU core on a modern laptop; when audio is silent, CPU use is < 2%.
- **SC-010**: Spurious character output in any bin on pure background noise (no signal) is zero when the squelch is set above the noise floor.
- **SC-011**: When two distinct CW signals at different tones within the passband are sent simultaneously (e.g., 500 Hz and 800 Hz), both decode independently in their respective bin rows with no cross-contamination on a clean channel.
