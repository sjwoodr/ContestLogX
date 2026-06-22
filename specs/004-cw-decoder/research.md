# Research: CW Decoder (SPEC-005 / 004-cw-decoder)

**Phase 0 output** · 2026-04-21

This document resolves the open technical questions flagged by the Technical Context of [plan.md](plan.md) and by reviewing the existing ContestLogX codebase against the [spec](spec.md). Each entry follows the required format: **Decision → Rationale → Alternatives considered**.

---

## R1 - Audio capture API: `QAudioSource` via `Qt6::Multimedia`

**Decision**: Use `QAudioSource` for audio capture and `QMediaDevices::audioInputs()` for device enumeration. Configure the stream as **mono, 16-bit signed little-endian PCM at 8 kHz** (`QAudioFormat::Int16`, 1 channel, 8000 Hz). The audio sink is a `QIODevice*` returned by `QAudioSource::start()`, read in the Qt event loop on the capture thread and posted to a ring buffer consumed by the decoder worker.

**Rationale**:
- Cross-platform out of the box: Linux (PipeWire/PulseAudio), macOS (CoreAudio), Windows (WASAPI). No platform-specific audio code required.
- 8 kHz is more than enough bandwidth for CW: the entire audible CW range (300-2500 Hz) is well below Nyquist at 4 kHz. Lower sample rate = lower CPU for Goertzel.
- 16-bit signed PCM is the universal default; no format negotiation is required on any of the three platforms.
- Mono simplifies the DSP path; stereo would require a channel-mixing step for no decoder benefit.
- `QAudioSource::start(QIODevice*)` mode (push mode) works identically everywhere.

**Alternatives considered**:
- **PortAudio**: third-party dependency; constitution forbids (Principle II). Rejected.
- **Platform-native APIs (ALSA / CoreAudio / WASAPI directly)**: three separate code paths, more maintenance burden, no operator benefit. Rejected.
- **16 kHz sample rate**: acceptable but doubles the per-block DSP cost for no accuracy gain at CW frequencies. 8 kHz is sufficient and cheaper.
- **Float32 samples**: would simplify Goertzel math slightly but introduces unnecessary conversion overhead from the OS-level PCM. Rejected.

---

## R2 - Goertzel algorithm for tone detection

**Decision**: For each bin, implement the standard second-order Goertzel recursion over a block of N samples:

```
Given target frequency f and sample rate fs:
    k           = N * f / fs                     (normalized bin index)
    omega       = 2π * k / N
    coeff       = 2 * cos(omega)                 (precomputed once per bin)

Per block:
    s_prev      = 0
    s_prev2     = 0
    for sample x in block:
        s       = x + coeff * s_prev - s_prev2
        s_prev2 = s_prev
        s_prev  = s
    magnitude² = s_prev² + s_prev2² - coeff * s_prev * s_prev2
```

Block size: **80 samples (10 ms at 8 kHz)**. At each block boundary, magnitude² is compared against the squelch threshold and a per-bin "tone present" boolean is emitted.

**Rationale**:
- Goertzel is O(N) per bin per block - a small constant factor cheaper than FFT when the number of target bins is small relative to FFT size. At 6 bins × 80 samples = 480 multiply-adds per 10 ms, well under 0.1% of one CPU core.
- 10 ms blocks are fine enough to resolve dot lengths at 60 WPM (dot = 20 ms) with 2 samples per dot - adequate for edge detection.
- Coefficients are precomputed once per bin (recomputed only on bin-config change, per FR-018), so the hot path is pure integer/float multiply-add.
- The standard second-order recursion is numerically stable over the block size used.

**Alternatives considered**:
- **Full FFT** (e.g., Qt's QtCharts DFT or Kiss FFT): unnecessary because we only need tone presence/absence at N known frequencies, not a full spectrum. Full FFT would cost more AND produce data we do not use. Explicitly rejected in spec FR-015 and constitution V (Simplicity).
- **Sliding-DFT**: more complex, susceptible to numerical drift over long runs; Goertzel's block-reset behavior is cleaner. Rejected.
- **First-order IIR bandpass + envelope detector**: simpler per-bin but requires careful filter design per frequency and adds phase delay that complicates timing. Rejected.
- **Different block size**:
  - 5 ms (40 samples): more CPU, diminishing returns for CW timing.
  - 20 ms (160 samples): too coarse for 60 WPM dot edges.
  - 10 ms is the goldilocks.

---

## R3 - Continuously adaptive WPM via rolling-median dot-length estimator

**Decision**: Per bin, maintain a rolling window of the most recent **N=16 measured dot lengths**. On each emitted character, compute the median of the window and derive WPM = 1200 ms / median_dot_ms (PARIS standard). Feed the updated estimate back into the dot/dash classifier's threshold (dashes = ≥ 2.5× current dot estimate). Bound the estimate to the operator's `wpmMin` / `wpmMax` range - if the derived WPM falls outside, the bin enters "no lock" state and suppresses output until the estimate returns to the bounds.

**Rationale**:
- **Median** over mean: CW timing is susceptible to occasional bad samples (noise bursts, dropped pulses, QSB fades). Median ignores outliers naturally; mean would drift.
- **N=16**: empirically good tradeoff - large enough to smooth 2-3 noisy dots, small enough to track a sender's speed change within ~5 characters (matching SC-003). Window size is a plan-level default, not an operator-facing setting.
- **PARIS standard (1200 / dot_ms)**: the industry-standard formula used by fldigi, MRP40, CW Skimmer, and RBN - verified across the published Morse-code timing literature. Operator-facing numbers will match what operators see in other decoders.
- **Bounding range enforcement**: prevents a noise burst from driving the estimator to 120 WPM and making all subsequent decoding garbage.

**Alternatives considered**:
- **Rolling mean**: simpler, vulnerable to outliers. Rejected.
- **Exponential moving average (EMA)**: smooth but lags fast speed changes; a median over a short window converges faster on actual speed changes while still rejecting outliers. Rejected.
- **Measure only dot lengths (not dash lengths)**: adopted - dashes are 3× dots, so estimating dots alone is sufficient and cleaner statistically.
- **Window size 8**: too noisy at slow WPM where dots are rare. Window size 32: too slow to track speed changes, violates SC-003. N=16 is chosen.

---

## R4 - Rig backend `pttStateChanged` signal (prerequisite gap)

**Decision**: Add a new Qt signal to `RigInterface`:

```cpp
signals:
    void pttStateChanged(bool active);
```

Each concrete backend (`FlrigClient`, `HamlibClient`, `MockedRigClient`) MUST emit this signal on every PTT state transition. The existing PTT accessor `getPTT()` remains; the signal augments it for event-driven consumers.

**Rationale**:
- The decoder's PTT-mute path (FR-019a) requires signal/slot wiring, not polling - polling has latency and CPU cost. The existing `RigInterface` (`include/rigInterface.h:86-92`) exposes `connected`, `disconnected`, `error`, `frequencyChanged`, `modeChanged` signals but NOT a PTT-state signal.
- Adding the signal is a small, backward-compatible extension: existing callers of `getPTT()` continue to work; the decoder subscribes to the new signal.
- Backend responsibilities:
  - **FlrigClient**: poll flrig's XML-RPC `rig.get_ptt` periodically (already polled alongside freq/mode in the main-thread loop); emit on transition.
  - **HamlibClient**: the `HamlibWorker` already polls rig state on a background thread; extend it to track PTT and emit.
  - **MockedRigClient**: emit `pttStateChanged(false)` at startup and toggle on `setPTT()` calls so tests and the fallback path are exercised.

**Alternatives considered**:
- **Poll `getPTT()` from the decoder**: violates Qt6-native idioms (signals are the preferred pattern) and costs a timer per decoder per radio. Rejected.
- **Add PTT as an overload of existing `modeChanged`**: semantically wrong - PTT is orthogonal to mode. Rejected.
- **Do not add the signal; rely only on internal-send signalling (FR-019c)**: acceptable per Clarify #2 fallback behavior, but misses the manual-keying case. Adding the signal is cheap and closes the gap.

---

## R5 - Audio device identifier persistence

**Decision**: Store the audio device by its **human-readable name** (`QAudioDevice::description()`). On startup, resolve by matching against currently enumerated devices:
1. Exact description match → use that device.
2. No match → leave "(none)" selected for that radio and show a one-line status message in the rig status area ("Audio device '<name>' not found; decoder disabled for Radio L").
3. Operator can reselect in Rig Connection Settings.

**Rationale**:
- Device descriptions are stable across reboots on the three supported platforms for the common hardware (USB audio devices on the K4 / IC-7300 show consistent names). IDs are opaque and platform-specific; names survive OS updates better.
- If the device is genuinely gone (unplugged), falling back to "(none)" + one-line notice is better UX than silently decoding nothing.
- Consistent with how ContestLogX already persists flrig/hamlib host identifiers (string-based).

**Alternatives considered**:
- **Store device ID (`QAudioDevice::id()`)**: IDs are byte-blob on macOS, integer on Linux, GUID on Windows - opaque and can change across OS updates. Rejected as primary key; may be added as a secondary check in a future refinement.
- **Store both name and ID; prefer ID; fall back to name**: more complex, marginal benefit given target users. Deferred.

---

## R6 - Threading model: capture → ring buffer → decoder worker

**Decision**: Two threads per decoder session.

- **Capture thread** (implicit - Qt event loop on main thread calls `QIODevice::readyRead` handlers for the `QAudioSource` output device). The handler drains audio into a **lock-free SPSC ring buffer** (Qt does not ship one, so implement a small one using `std::atomic<size_t>` head/tail indices - ~40 lines of code).
- **Decoder worker thread** (`QThread`): owns a `CwDecoderWorker` QObject whose event loop wakes every ~10 ms (or on a `QSemaphore` posted by the capture side). On each wake it pulls available audio from the ring, runs the Goertzel + per-bin state machines, and emits `charDecoded(int binIdx, QChar)` / `wpmUpdated(int binIdx, int wpm)` signals via `Qt::QueuedConnection` to the widget on the main thread.

Ring buffer size: **8,000 samples (1 second at 8 kHz)** - 16 KB per decoder session. Generous headroom for momentary scheduling hiccups.

**Rationale**:
- Mirrors the existing `HamlibWorker` pattern (`src/rig/hamlibClient.cpp`) - proven idiom in this codebase.
- Audio callbacks must never block; the ring buffer decouples capture from decode.
- Lock-free SPSC ring buffer is simpler and faster than a `QMutex` + `QWaitCondition` producer/consumer on this workload.
- One worker per decoder session (per radio) means the two SO2R decoders cannot interfere with each other even on a single-core CPU.

**Alternatives considered**:
- **Single-threaded DSP on the capture callback**: risks blocking the audio callback if Goertzel spikes; failures cause audio drops. Rejected.
- **`QMutex` + `QWaitCondition`**: correctness is fine, but adds kernel-level wakeup overhead on every audio block. Lock-free atomics are lighter. Rejected.
- **Shared decoder worker for both radios**: couples the two, violates SO2R independence, harder to reason about. Rejected.

---

## R7 - Cross-platform audio device enumeration differences

**Decision**: Use `QMediaDevices::audioInputs()` uniformly on all three platforms. Document platform-specific user setup in `quickstart.md`:

| Platform | Typical user configuration |
|---|---|
| **Linux** | PipeWire presents both real devices and `module-loopback` / `pw-loopback` virtual sinks. Ensure PipeWire is running (default on Ubuntu 22.04+, Fedora 36+). For radios with USB audio (IC-7300, K4), select the radio's USB audio endpoint directly. For virtual routing, create a loopback with `pw-loopback` or configure a PulseAudio null-sink. |
| **macOS** | CoreAudio exposes all input devices including aggregate devices. For virtual routing, use BlackHole (free) or Loopback.app (paid). Qt6 on macOS requires a microphone-usage string in `Info.plist` - add `NSMicrophoneUsageDescription` during build. |
| **Windows** | WASAPI in shared mode (Qt default). Radios with USB audio appear as separate input devices (no special config). For virtual routing, use VB-CABLE or Voicemeeter. Note: Stereo Mix is only available if the OEM driver enabled it - not reliable. |

**Rationale**:
- All three OSes surface the same devices through Qt6::Multimedia; no conditional code needed in the app.
- The differences are in what the *operator* needs to configure, which belongs in docs, not source.
- The `NSMicrophoneUsageDescription` is the only build-time wrinkle; add it to the macOS bundle plist.

**Alternatives considered**:
- **Abstract over the platforms with a custom layer**: unnecessary; Qt already does this. Rejected.

---

## R8 - Token detection heuristics

**Decision**: Per-bin, maintain a rolling text buffer; after every emitted character, scan the tail-most 20 characters against two regexes:

- **Callsign**: `\b[A-Z0-9]*[A-Z][0-9][A-Z0-9]*[A-Z](?:/[A-Z0-9]+)?\b` - a simplified contest-callsign pattern. Matches `K1ABC`, `W9XYZ`, `PJ2/N9OH`, `YB1AR/2`, `VE3XYZ`, etc. Rejects pure numbers and lone letters.
- **RST**: `\b(?:5NN|4NN|3NN|[1-5][1-9][1-9]|[1-5][1-9])\b` - three-digit standard, two-digit short RST, and CW N-for-9 forms.

When a match is found, emit a token span with the bin index, byte range in the scrolling buffer, and token type. The widget renders that span as a clickable anchor. On click, `callClicked(binIdx, QString)` or `rstClicked(binIdx, QString)` fires.

**Rationale**:
- Regex-based detection is simple, deterministic, and testable.
- The tail-most 20 characters is sufficient to capture callsigns and RSTs as they are emitted - no retroactive re-scoring as more characters arrive.
- Matching at word boundaries (`\b`) prevents mid-callsign substrings from being clickable.
- The callsign regex is deliberately permissive - better to accept a rare non-callsign than to reject a legitimate portable/prefix operation.

**Alternatives considered**:
- **DXCC-database lookup on every potential callsign**: rejects garbage callsigns but adds per-character DXCC lookup cost; bounces against the ContestEngine. Defer to the keyboard-entry handler, which already does SCP + call-history, to validate semantically. Rejected at token-detection layer.
- **Callsign regex from an authoritative source (ADIF spec)**: the ADIF callsign pattern is quite loose; the simplified contest pattern above has better precision for decoded text.

---

## R9 - Internal-send duration estimation

**Decision**: When `MainWindow` initiates a CW send via `cwio_text` or CW console, it calls `decoder->muteForInternalSend(durationMs)` with:

```
durationMs = ceil( textChars * 60.0 / (sendWpm * 5.0) * 1000 ) + graceMs
```

where `sendWpm` is the current CW send WPM (a setting already in `CwSettings`), `textChars` is the length of the expanded macro text, and `graceMs` defaults to 250. The decoder tracks an absolute `muteUntil` timestamp; while `now() < muteUntil`, all bins are gated.

**Rationale**:
- The PARIS baseline is 5 characters = 50 dot-units ≈ 1 word at 5 WPM = 60 seconds / 5 WPM = 12 seconds per 5 chars → `seconds_per_char = 60 / (WPM × 5) × 1000` ms. This is the standard CW duration formula; mirrors the send timing that the CW console already uses internally.
- The 250 ms grace window absorbs flrig's spool tail and the worst-case backend RX-recovery time.
- Storing an absolute timestamp (rather than a countdown) avoids clock-drift issues if the decoder worker thread is briefly descheduled.

**Alternatives considered**:
- **Read flrig's in-progress send state**: flrig's XML-RPC does not expose this cleanly; unreliable across flrig versions. Rejected.
- **Use a fixed per-character mute**: would over-mute short sends and under-mute long ones; WPM-aware estimation is more accurate with trivial code.
- **No grace window**: tested against typical flrig tail behavior - self-decode leaks at end of transmission. Grace window mitigates; 250 ms is conservative.

---

## R10 - Constitution v1.0.1 and the `Qt6::Multimedia` module addition

**Decision**: Approve the addition of `Qt6::Multimedia` to the project's Qt6 module set at this plan gate. Do NOT bump the constitution to v1.1.0 as a formal amendment.

**Rationale**:
- Constitution v1.0.1 §Technology Stack lists Qt6 modules as "Core, Widgets, Network, SerialPort, Xml" - a non-exhaustive enumeration in practice (for example, `Qt6::Test` is used for `make test` without ever being listed). Adding one more standard Qt6 module is consistent with the spirit of Principle II (Qt6-native; no third-party UI frameworks).
- `Qt6::Multimedia` is shipped by the same Qt6 distribution, audited by the same vendor, and has no independent license or dependency footprint.
- Per prior user decision, the gate-level approval path was chosen over a formal amendment (recorded in the workflow's Decision log, 2026-04-21).

**Alternatives considered**:
- **Bump constitution to v1.1.0**: more formal, requires `CLAUDE.md` update and `.specify/templates/` audit per the governance section. Heavy for "add one standard module." Rejected in favor of gate-level approval.

---

## R11 - Non-goals reaffirmed after Phase 0

The following items, raised in clarification discussion or touched on in research, are explicitly **out of scope** for this feature and will not be implemented:

- Full FFT / waterfall / panadapter.
- Automatic callsign harvesting / RBN-style network output (this is a distinct future feature, listed in Out of Scope of the spec).
- CW transmission via the decoder (existing CW console owns keying).
- Cross-bin fusion of signals straddling bin boundaries.
- Per-bin squelch thresholds (one global squelch per session is sufficient).
- Loading pre-recorded WAV files into the decoder (test-only affordance if needed; not exposed in UI).

---

## Summary of design inputs locked at end of Phase 0

| Area | Decision |
|---|---|
| Audio API | `Qt6::Multimedia` `QAudioSource`, 8 kHz mono 16-bit PCM |
| DSP | Goertzel per bin, 10 ms blocks, N=6 default bins at 400-1000 Hz |
| WPM estimator | Rolling median over 16 dot lengths, PARIS standard |
| Threading | Two threads per session: capture (main) → SPSC ring → worker (QThread) |
| PTT signal | Add `pttStateChanged(bool)` to `RigInterface`; emit from all three backends |
| Internal-send mute | `muteForInternalSend(durationMs)`; duration = text_chars × 60 / (WPM × 5) × 1000 + 250 ms grace |
| Device persistence | By `QAudioDevice::description()`; graceful fallback to "(none)" if unresolved |
| Token detection | Regex per bin for callsign and RST shapes |
| Constitution | `Qt6::Multimedia` approved at this plan gate (no formal amendment) |

Phase 1 (data model + contracts + quickstart) may proceed.
