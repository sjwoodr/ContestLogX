# Quickstart: CW Decoder Development & Testing

**Phase 1 output** · 2026-04-21 · Feature branch `004-cw-decoder`

This guide gets a developer set up to build, run, and manually verify the CW decoder on Linux, macOS, and Windows. Unit tests (`make test`) require no audio hardware; manual verification uses a virtual audio cable or a web-based CW sender.

---

## 1. Install build prerequisites

### Linux (Ubuntu 22.04+ / Debian testing)

```bash
sudo apt install cmake \
                 qt6-base-dev \
                 libqt6serialport6-dev \
                 libqt6xml6-dev \
                 qt6-multimedia-dev \
                 libqt6multimedia6 \
                 pipewire-pulse   # or: pulseaudio
```

### macOS

```bash
brew install qt6 cmake
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)
```

Qt6::Multimedia is included in the Homebrew `qt6` bottle. On Apple Silicon no extra setup is needed.

### Windows (MSVC)

- Install the Qt6 offline installer with **MSVC 2022 64-bit** and the **Multimedia** component selected.
- Install **Visual Studio 2022 Build Tools** (Desktop development with C++).
- Ensure `cmake` is on `PATH`.

---

## 2. Build

From the repo root:

```bash
make        # or: cmake -B build && cmake --build build -j
```

`make` MUST succeed with zero warnings on GCC (constitution Principle II).

On first build after pulling this branch, CMake will fetch `Qt6::Multimedia`. If CMake fails with "Could not find Qt6::Multimedia", re-check that the dev packages above are installed.

---

## 3. Run unit tests (no audio hardware required)

```bash
make test
```

New tests added by this feature:

| Test binary | What it exercises |
|---|---|
| `tests/test_goertzel` | Goertzel correctness against synthetic sine input at each default bin center frequency (400, 500, 600, 700, 800, 900 Hz). |
| `tests/test_binChannel` | Dot/dash classifier, rolling-median WPM estimator, Morse-table lookup against generated dot/dash patterns. |
| `tests/test_cwDecoder` | End-to-end: synthetic audio block → decoded character stream. Two concurrent signals at different tones decoded independently (SC-011). |

These tests use no `QAudioSource` - they invoke `CwDecoder::processBlock()` directly with pre-generated `int16_t` arrays.

---

## 4. Manual verification - routing CW audio into ContestLogX

You need a way to feed CW audio into a system audio input device that `ContestLogX` can see in its Rig Connection Settings → Audio Input Device dropdown. Options below, by platform.

### Option A - Virtual audio cable (any platform)

Create a loopback so that audio from one app (fldigi, a web Morse sender, a YouTube CW recording) is routed to a virtual input device that ContestLogX captures.

#### Linux - PipeWire loopback (recommended)

```bash
pw-loopback --capture-props='media.name=clx-cw-src' \
            --playback-props='media.name=clx-cw-sink node.name=clx_cw_virtual_input media.class=Audio/Source'
```

This creates a virtual source named `clx_cw_virtual_input` that picks up any audio routed to the paired sink. In `pavucontrol` (or `helvum`), route your CW-producing app's output to `clx-cw-src`. In ContestLogX's Rig Connection Settings, select `clx_cw_virtual_input` as the Radio L audio device.

#### Linux - PulseAudio module-null-sink (older systems)

```bash
pactl load-module module-null-sink sink_name=clx_cw \
        sink_properties=device.description=ContestLogX_CW_Virtual_Sink
pactl load-module module-loopback source=clx_cw.monitor
```

Select `Monitor of ContestLogX_CW_Virtual_Sink` in ContestLogX.

#### macOS - BlackHole (free)

```bash
brew install blackhole-2ch
```

Create a Multi-Output Device in Audio MIDI Setup combining your speakers + BlackHole 2ch. Set it as the system output. In ContestLogX, select **BlackHole 2ch** as the audio input device.

#### Windows - VB-CABLE (free)

Download and install [VB-CABLE](https://vb-audio.com/Cable/). Set **CABLE Input** as the playback device for the CW-producing app; in ContestLogX select **CABLE Output** as the audio input device.

### Option B - Direct from a radio's USB audio

If you have an Elecraft K4, Icom IC-7300, IC-7610, Yaesu FT-710, or similar radio with a USB audio endpoint:

1. Connect the radio's USB cable.
2. In ContestLogX's Rig Connection Settings → Audio Input Device, select the radio's USB audio device (often named `USB Audio CODEC` or similar).
3. Tune the radio to a CW signal (contest band during activity, W1AW CW bulletins, or a CWT).

---

## 5. Produce a test CW signal

If you don't have a live signal on the air, any of these work:

- **fldigi** - Menu → Op Mode → CW. Type into the transmit pane; fldigi sends via the audio output device selected in fldigi's config.
- **MRP40** - ships with a CW sender tool.
- **[rfzero.net Morse Sender](https://morsecode.world/international/translator.html)** - web-based; paste text, press play, captures system audio.
- **Pre-recorded WAV** - play any CW recording through your normal audio output; the virtual cable routes it into ContestLogX.

Send at a known speed (e.g., 25 WPM) and verify:

1. ContestLogX shows a decoder panel for Radio L (assuming you configured the audio input there).
2. The panel shows 6 rows labeled with their center frequencies (400, 500, 600, 700, 800, 900 Hz by default).
3. The row corresponding to your sender's tone frequency scrolls the decoded text in real time (within 200 ms perceptible).
4. The row's live WPM readout shows ~25.
5. Decoded callsigns appear as clickable tokens (underlined or colored). Click one - it fills the CALL field in Radio L's QSO entry panel without stealing keyboard focus.
6. Decoded RST tokens (`599`, `5NN`) are also clickable and fill RSTr.

---

## 6. Test PTT mute (FR-019a and FR-019c)

### Rig-backend PTT mute (path 1)

1. Ensure "Mute decoder on PTT" is checked for Radio L in Rig Connection Settings.
2. Start a CW signal on the audio input (see step 5 above).
3. Key the radio manually (mic PTT or paddle - anything that causes the rig backend to report PTT active).
4. Observe: all decoder rows for Radio L freeze while PTT is active. No new characters appear.
5. Release PTT. Decoding resumes on the active bin.

If your backend is `MockedRigClient`, you can simulate PTT via the developer menu (or by setting `rig.mock.pttActive=true` in settings during development).

### Internal-send mute (path 2)

1. Configure a CW F-key memory with a long string (e.g., your CQ macro).
2. With an unrelated CW signal playing on the audio input, press the F-key.
3. Observe: Radio L's decoder goes silent for the duration of the estimated send (text_chars × 60 / (send_wpm × 5) × 1000 + 250 ms grace).
4. After the send completes, decoding resumes.

---

## 7. Test SO2R independence (SC-011, FR-019a/b/c/d per radio)

1. In Rig Connection Settings, enable SO2R. Configure Radio L's audio device (e.g., BlackHole 2ch) and Radio R's audio device (e.g., CABLE Output, a second instance of BlackHole, or a real radio's USB audio).
2. Route one CW source to Radio L's audio device and a different CW source to Radio R's audio device.
3. Observe: two decoder widgets appear (one per radio), each decoding only its own audio.
4. Set keyboard focus to Radio L's entry (backtick toggle). Click a callsign in Radio R's decoder - verify:
   - Radio R's CALL field is filled.
   - Keyboard focus remains on Radio L's entry (no focus steal).
   - SCP / call-history lookup fires for Radio R's field (identical to keyboard entry per FR-024a).

---

## 8. Run the test-log regression (sanity, not required)

```bash
make test-logs-headless
```

The CW decoder does not change `ContestEngine`; `make test-logs` is not required per Constitution Check. But running it confirms no accidental collateral damage.

---

## 9. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Decoder panel does not appear | No audio device configured for the radio, OR "(none)" selected | Rig Connection Settings → select an audio device for Radio L (and Radio R for SO2R) |
| Rows are empty despite CW audible | Squelch too high | Lower the squelch slider; also check that the source signal lands within the configured passband (default 400-1000 Hz) |
| Garbled decode | Speed outside WPM hint range; or audio too distorted | Adjust WPM Min/Max in decoder settings; verify audio level is not clipping |
| WPM readout stuck on " - " | No lock - signal too weak or speed out of bounds | Increase audio level; widen WPM Min/Max |
| Own keying appears in decoder | Mute on PTT is OFF, or rig backend does not report PTT state | Re-enable Mute on PTT for that radio. If using `MockedRigClient`, the fallback log entry appears once per session |
| CMake error "Could not find Qt6::Multimedia" | Missing dev package | See step 1 for your platform |
| macOS build prompts for microphone access every run | `NSMicrophoneUsageDescription` missing from `Info.plist` | Add the key to the app bundle's plist; after one grant, subsequent runs are silent |

---

## 10. What to expect in CI

- **Linux (Ubuntu 24.04)**: `make && make test` passes; no audio hardware needed in CI.
- **macOS (latest)**: same.
- **Windows (MSVC)**: same; no CI-level audio device required.

The CI builds do NOT run the manual audio-path verification. Those steps are for local dev validation before merging.
