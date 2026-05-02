# Changelog

All notable changes to ContestLogX are documented in this file.

## [0.7.33]

## [0.7.32]

### Other Changes and Bugfixes
- Fixed CW Decoder failing to emit characters when the operator left the squelch slider at 0% — the Schmitt off-threshold expression `qMin(squelch * 0.9, …)` collapsed to zero when squelch was zero, so any non-zero magnitude held the tone "on" forever and elements never closed (no key-up edge → no dits/dashes → no characters), regardless of how strong the CW signal actually was. Now the squelch ceiling falls back to 1.0 when squelch=0 so the floor-lifted and peak-relative branches still produce a sane off-threshold; the on-threshold also degrades to a noise-floor-lifted value so noise doesn't constantly trigger key-down. Net result: leaving squelch at zero now works for clean CW signals, instead of being a silent foot-gun
- Added an audio-flow heartbeat to AudioCapture — when CW Decoder Debug is enabled, log a single line every 500 chunks (~5 s at 10 ms/chunk) confirming the QAudioSource is still delivering data. Catches the failure mode where Windows WASAPI delivers an initial buffered burst (~300 ms) and then silently stops calling `readyRead`, which otherwise looks identical to "decoder is broken" from the UI side. The first three chunks still log unconditionally for fast triage; the heartbeat only kicks in afterwards when the toggle is on so normal operation stays quiet

## [0.7.31]

### Other Changes and Bugfixes
- Added debug-gated diagnostic logging to the CW Decoder pipeline to triage "decoder shows nothing" reports when the audio path is confirmed working but no characters appear. Enable Debug → CW Decoder Debug to get: a one-line decoder configuration summary at capture start (passband Hz range, every bin's center frequency, WPM bounds, squelch threshold); a per-bin magnitude snapshot every 5 seconds (each bin's current normalized Goertzel magnitude with a `*` marker on bins currently in tone-active state) so it's instantly visible whether the operator's CW pitch lands in any configured bin with usable signal level; and a per-character emission log so it's clear whether characters are being decoded at all (vs not being shown in the UI). All three are gated by the existing toggle so they don't bloat the log in normal use

## [0.7.30]

### Other Changes and Bugfixes
- Fixed CW Decoder receiving only ~300 ms of buffered audio on Windows and then going silent on the same USB CODEC where 0.7.29 finally got the format/channel handling right — the AudioCapture object was being created on the main thread and handed to the worker thread via `setParent()`, but `setParent()` does not move QObject thread affinity. As a result the QAudioSource and its WASAPI polling timer lived on the main thread while the worker thread drove `readyRead` handlers, generating a flood of `QObject::startTimer / killTimer: Timers cannot be (started/stopped) from another thread` warnings on every audio block and (apparently) corrupting WASAPI's polling state so only the initial buffered chunk delivered real samples. The worker now explicitly `moveToThread()`s the capture before assigning the parent, so the audio pipeline lives entirely on the worker thread
- Fixed Debug Log Viewer ("Could not open log file") on Windows — the debug log path defaulted to the relative filename `clx_debug.log`, which resolved against whatever working directory CLX inherited at launch. On Windows that was typically `C:\Program Files\ContestLogX\` (not writable for normal users; UAC virtualization redirected the actual write to `%LOCALAPPDATA%\VirtualStore\…`), and the in-app log viewer then tried to read from the original install-dir path and failed. Default path is now an absolute, per-user, writable location via `QStandardPaths::AppLocalDataLocation`: `~/.local/share/ContestLogX/clx_debug.log` on Linux, `~/Library/Application Support/ContestLogX/clx_debug.log` on macOS, `C:\Users\<user>\AppData\Local\ContestLogX\clx_debug.log` on Windows. `--debug-log <path>` on the command line still overrides as before

## [0.7.29]

### Other Changes and Bugfixes
- Fixed CW Decoder receiving zero audio on Windows even when Microphone privacy settings show ContestLogX as "Currently in use" — the WASAPI backend was reporting `isFormatSupported(monoInt16)` as true and the stream opened cleanly, but the driver only actually delivered samples in its native format (typically stereo Float on USB CODEC devices), so every readyRead callback returned zero-filled buffers. AudioCapture now uses each device's preferred format unchanged and converts to mono int16 in software (handles every Qt sample format and arbitrary channel counts already)
- CW Decoder mono mixdown now averages all input channels instead of taking only channel 0, so USB CODECs that route the rig audio to the right channel (or any channel other than the first) capture the signal correctly
- Added a 3-second silence watchdog to AudioCapture — if a real-device capture stream is open but every sample so far is zero, a clear warning dialog is shown to the operator (with platform-specific causes: muted level, wrong endpoint, exclusive-mode conflict, OS privacy block) instead of the decoder silently showing nothing. AudioCapture now also logs the negotiated format and the first three audio chunks (size, frames, peak amplitude) unconditionally so "decoder shows nothing" reports can be triaged from the debug log without flipping per-component debug toggles first
- Added an OS microphone-permission check to the CW Decoder — the widget logs the current permission state (Granted / Denied / Undetermined) for each radio at construction (visible in the debug log alongside the audio device enumeration), and re-checks before starting a real-device capture. On Denied, a platform-specific dialog explains where to grant access (Windows: Privacy & security → Microphone → Let desktop apps access your microphone; macOS: System Settings → Privacy & Security → Microphone) instead of letting the decoder come up silent. On Undetermined, the OS is asked for a decision and decoding auto-resumes if granted. Skipped for the practice virtual sources since they synthesize audio internally

## [0.7.28]

### Other Changes and Bugfixes
- Fixed Linux AppImage failing to launch with `dlopen(): error loading libfuse.so.2` on Ubuntu 22.04+ / Debian 12+ and other modern distros that no longer ship `libfuse2` by default — the AppImage now uses the modern type2 runtime (supports both fuse2 and fuse3) instead of the fuse2-only runtime that AppImageKit's `appimagetool` bundles by default. Regression introduced in 0.7.26 when AppImage packaging switched from `linuxdeploy --output appimage` to `appimagetool` to fix CW Decoder GStreamer plugin bundling

## [0.7.27]

### Contest Updates
- Fixed Florida QSO Party (FQP) Cabrillo CONTEST header — now emits `FCG-FQP` (the contest sponsor's required identifier) instead of `FQP`

### Other Changes and Bugfixes
- CW memory buttons (F1–F8) in the CW console now show the full memory text as a tooltip on hover, so you can see exactly what each one sends without opening the memories editor; empty slots show no tooltip
- Fixed SSB memory phonetic playback for digits — "9" is now spoken as "nine" (matches FCC / ARRL Operating Manual phonetics) instead of "niner" (NATO / ICAO aviation usage); affects `{mycall}` and any other callsign-phonetic expansion

## [0.7.26]

### Other Changes and Bugfixes
- Added Start/Stop button to the CW Decoder widget toolbar (next to Clear) — defaults to "Stop" while decoding; click to halt the decoder and silence any practice-audio output, label flips to "Start", click again to resume against the currently-selected audio device. Disabled when the device is "(none)"
- Added "View Debug Log" entry at the bottom of the Debug menu — opens a standalone (non-dockable) window that tails the active `clx_debug.log` file (~1.5s poll), with Clear, Copy (sends the visible buffer to the clipboard), and Close buttons and an Auto-scroll checkbox so you can scroll back to inspect older lines without the view jumping to the bottom; runs as an independent top-level window so you can leave it open and switch back to CLX
- CW Decoder now logs every detected audio input device (description, ID, default flag, preferred sample rate / channels / format) plus the saved per-radio device selection on widget construction — emitted unconditionally (not gated by the CW Decoder Debug toggle) so "decoder shows nothing" reports can be triaged without the operator having to enable per-component debug first

## [0.7.25]

### Other Changes and Bugfixes
- Practice — Contest Exchange now adds two QRM stations (one above and one below the primary tone at asymmetric offsets, ~12–14 dB quieter, slightly different WPM) so the decoder and the operator experience realistic contest-band conditions with adjacent-channel interference; Rag Chew remains a single clean signal for pure head-copy practice
- Added CW Decoder entry to Preferences → Fonts so the font family and size of the decoded-text rows (and the surrounding toolbar / labels) can be customized; applies at widget spawn and on bin-count changes
- Added Remote Dashboard — embedded HTTP server serves a read-only dashboard on your LAN (score, rate, recent QSOs, rig state, propagation). Works on phone, tablet, or another PC browser (responsive layout). Preferences → Dashboard to enable; copy the bookmarkable URL or scan the QR code with your phone camera. Token-based auth; disabled by default
- Added rig-control POST endpoints to the Remote Dashboard — /api/rig/qsy (set freq/mode), /api/rig/band (jump to 20m/40m/etc), /api/rig/run_mode (Run/S&P/Off)
- Added Rig Control card to the Remote Dashboard page — band buttons (160–6m), mode buttons (CW/USB/LSB/DIG/FM/AM), Run/S&P/Off toggle, freq-in-MHz input with Go. SO2R sessions get an L/R radio toggle at the top. Current rig state highlights the matching button; taps POST to the V2 endpoints and the display reconciles on the next poll
- Added Apply button to Preferences — saves changes and triggers post-save actions (font reapply, Remote Dashboard restart, etc.) without closing the dialog, so you can iterate and see the effect immediately (e.g. enable the Dashboard, click Apply, scan the QR code)

## [0.7.24]

### Other Changes and Bugfixes
- Added CW Decoder Practice mode — two virtual audio sources in the decoder's Audio dropdown ("Practice — CW Rag Chew" and "Practice — Contest Exchange") that synthesize CW on the default output device and feed the same tones into the decoder pipeline, so operators can head-copy random QSOs / exchanges and check their copy against what the decoder captured; WPM follows the CW console setting, so speed matches the keyer
- Practice — Contest Exchange uses the active contest's named multipliers and sends with fixed 5NN RST (entry is disabled until a contest is loaded)
- Practice audio sources reset to "(none)" on startup so CLX doesn't start generating CW on the speakers the moment it launches — operator must explicitly re-select practice each session
- Updated website with CW Decoder Practice mode — feature bullet on the index page and a dedicated subsection in the docs page
- Added macOS first-launch instructions to the download page — explains how to unlock the ad-hoc-signed bundle via System Settings → Privacy & Security or via a single Terminal command, and notes the microphone permission prompt for the CW Decoder

## [0.7.23]

### Other Changes and Bugfixes
- Fixed macOS app bundle failing Gatekeeper verification — re-sign bundle ad-hoc after macdeployqt rewrites rpaths so Sealed Resources are properly generated
- Fixed CW Decoder receiving no audio on macOS — add NSMicrophoneUsageDescription to Info.plist so macOS TCC prompts for microphone access instead of silently returning a zero-filled stream
- Built macOS app bundle as universal binary (x86_64 + arm64) so it runs natively on both Intel and Apple Silicon Macs; minimum macOS version is now 11.0 Big Sur

## [0.7.22]

### Other Changes and Bugfixes
- Fixed CW Decoder failing to build on Windows (MSVC does not define M_PI without _USE_MATH_DEFINES)

## [0.7.21]

### Other Changes and Bugfixes
- Added CW Decoder — dockable multi-channel Morse decoder per radio (Window → CW Decoder) with N parallel frequency bins, auto-adaptive WPM tracking, and clickable callsign / RST tokens
- Added per-radio Audio Input Device selector in Rig Connection Settings and the decoder widget's toolbar (supports USB audio, virtual cable, soundcard); "(none)" disables the decoder for that radio
- Added per-radio "Mute decoder on PTT" toggle — gates the decoder during transmit so own keying doesn't self-decode; covers both manual keying and F-key / CW console sends
- Decoded callsigns (blue) and RST (amber) are clickable — fill the owning radio's CALL or RSTr field and trigger SCP / call-history / dupe check / QRZ auto-lookup just like keyboard entry; cut-numbers (5NN, 55N, 5N9) normalize to digits on fill
- Callsign recognition handles standard, portable-suffix (K1ABC/P), and prefix-slash (IT9/DK6XZ, DL/K1ABC) formats
- Added CW Decoder Debug Logging toggle under the Debug menu

## [0.7.20]

### Contest Updates
- Added Michigan QSO Party (MIQP) — MI and non-MI station classes, 83 MI counties, states/provinces/DX multipliers per mode, CW and SSB only

### Other Changes and Bugfixes
- Added QSY Back (Alt+B) — tunes rig to the frequency/mode of the previous QSO; press repeatedly to walk further back through the log

## [0.7.19]

### Other Changes and Bugfixes
- Added WSJT-X UDP listener — receives QSO Logged messages and pre-fills call, exchange, RST, and frequency/mode fields for operator review before logging
- RST validation now accepts digital signal reports (e.g., -15, +05) in addition to traditional RST format

## [0.7.18]

### Contest Updates
- Added Japan International DX Contest (JIDX) — CW and SSB, JA and DX station classes, band-based scoring, prefecture and CQ Zone multipliers per band
- Added OK-OM DX Contest — CW and SSB, OK/OM and DX station classes, 165 Czech/Slovak county multipliers, DXCC+WAE entities per band
- Added New Mexico QSO Party (NMQP) — NM and non-NM station classes, 33 NM counties, states/provinces/DXCC multipliers, power multiplier (QRP/LP/HP), DC/MDC alias to MD
- Added Missouri QSO Party (MOQP) — MO and non-MO station classes, 115 MO counties, states/provinces/DX multipliers, W0MA and K0GQ bonus stations
- Added Georgia QSO Party (GAQP) — GA and non-GA station classes, 159 GA counties, states/provinces multipliers per mode, CW and SSB only
- Added North Dakota QSO Party (NDQP) — ND and non-ND station classes, 53 ND counties, states/provinces, DX points-only (no DX mult), NL→NF alias

### Contest Engine Changes
- Added direct band-based scoring (byBand without month nesting)
- Added 1:1 named mult aliases for exchange normalization (e.g., "5" → "05")
- Added per-station-class named mult display lists and labels (e.g., "CQ Zones" for JA, "Prefectures" for DX)
- Added per-station-class point overrides (byPrompt) for asymmetric scoring rules
- Added received exchange validation — `namedMultOnly` blocks invalid exchanges (e.g., serial numbers where a county code is required); `namedMultOrSerial` accepts either named mults or serial numbers; both respect namedMultAliases

### Other Changes and Bugfixes
- Fixed exchange validation in several contests

## [0.7.17]
- Fixed light theme not working when running directly on GNOME/Wayland — use stylesheet-based theming that overrides GTK platform integration
- Removed hardcoded dark colors from DX Cluster spot table so it follows the active theme
- Theme changes now apply immediately with a note that some elements may need a restart
- Added "Use X11 backend" option in Preferences > Display (Linux only, on by default) — fixes window and floating dock position restore on Wayland
- Added cross-platform rules to CLAUDE.md

## [0.7.16]
- Replaced 7-Zip SFX with Inno Setup installer for Windows — proper install wizard, Start Menu shortcut, desktop shortcut, uninstaller, and post-install launch
- Added Windows application icon — exe, installer, taskbar, and shortcuts all show the ContestLogX icon

## [0.7.15]
- Fixed Windows SFX installer failing to launch after extraction — use %%T extract path variable and fix config whitespace

## [0.7.14]
- Fixed 7zSD.sfx download failing in CI (7-zip.org blocking bare curl requests)

## [0.7.13]
- Improved Windows SFX build — fetch 7zSD.sfx (public domain) at build time for proper installer UX with default extract path to %LOCALAPPDATA%\ContestLogX

## [0.7.12]
- Fixed Windows SFX build failing due to cmd.exe path escaping issue in CI

## [0.7.11]
- Changed Windows build from zip to self-extracting exe that installs to %LOCALAPPDATA%\ContestLogX

## [0.7.10]
- Added Windows build to CI pipeline — produces a self-contained portable zip via MSVC + windeployqt
- Updated download page with Windows download link and version bumps for all platforms

## [0.7.9]
- Fixed hidden dock widgets (DX Cluster, CW Console) reappearing on restart due to startup race condition where visibilityChanged signals overwrote saved settings before they could be read

## [0.7.8]
- Added validation when activating Run or S&P mode — if required memory roles (My Call/CQ/Run Exchange/TU for Run, My Call/S&P Exchange for S&P) are not assigned, an error dialog is shown and the CW or SSB memory editor opens automatically
- Added duplicate-role validation in the CW and SSB memory editor dialogs — cannot save if the same role is assigned to more than one memory slot
- Added "Copy from Station Memories" button in the CW and SSB memory editor dialogs, shown when viewing contest-specific memories, as a quick starting point
- Clamped fallback window geometry to 50% of the current screen, centered, so the app never opens off-screen or larger than the desktop when saved geometry can't be restored (layout minimums may stretch the actual height larger)
- Reduced minimum heights on CW Console, Score, SCP, and SSB Memories widgets so users can compress the dock rows more tightly; added layout stretch to CW Console so excess space sits at the bottom instead of inflating the history area
- Fixed uneven SSB memory button row heights (empty slots now show "F#\n---" so all 8 buttons stay the same height)
- Updated default_layout.json with a more compact dock arrangement

## [0.7.7]
- Fixed CW sending completely broken (F-key memories silently did nothing) caused by CWWindow being constructed before the rig client existed
- Fixed window geometry and panel state not saving when exiting via File > Exit (only saved when closing via window X button)

## [0.7.6]
- Fixed dock widget visibility (DX Cluster, CW Console) not persisting across restarts
- Made MainWindow and CWWindow debug logging always enabled for easier issue triage

## [0.7.5]
- Added call history import (File > Import Call History) supporting !!Order!! format files with field mapping dialog

## [0.7.4]
- Added Louisiana QSO Party (LAQP) contest definition with 64 parishes, multsPerBandAndMode scoring, and N5LCC bonus station
- Added Contest Calendar menu item under Contest menu (opens contestcalendar.com weekly contest listing)
- Added Mississippi QSO Party (MSQP) contest definition with 82 counties, multsOnce scoring, CW/SSB/RTTY/FT8 modes

## [0.7.3]
- Added SP DX Contest (SPDX) definition with support for both SP and DX station perspectives
- Added contest engine support for userPrompt-based stationClassMultipliers and invalidPartners
- Added Polish callsign generation support (-p flag) to callsign generator script

## [0.7.2]
- Added SO2R (Single Operator, 2 Radio) support with dual independent QSO entry dock widgets
- Added independent rig connections per radio (Radio L / Radio R) with separate backend, host, port settings
- Added backtick (`) keyboard shortcut to switch active radio, with visual indicator on active panel
- Added independent Run/S&P/Off mode per radio for typical SO2R operation
- Added SO2R Mode toggle in Rig menu and Rig Connection Settings dialog
- Added mocked rig backend for SO2R testing and practice without a second radio
- Added tabbed Rig Connection Settings dialog (Radio L / Radio R tabs when SO2R enabled)
- Added NOAA space weather propagation data (SFI, A-index, K-index) fetched automatically
- Added clickable status bar labels: rig status opens connection dialog, propagation opens NOAA
- DX cluster spots and CW/SSB keying route to the active radio
- RST defaults update automatically on mode change (599 for CW/RTTY, 59 for SSB)
- Fixed multiple SO2R focus and input routing issues with CW memories, freq/mode, and field clearing

## [0.7.1]

## [0.7.0]
- Added Hamlib (rigctld) as an alternative rig control backend alongside flrig
- Added rig backend selector in Rig Connection Settings dialog with per-backend configuration
- Added RigInterface abstraction layer for supporting multiple rig control backends
- Added online score publishing to contestonlinescore.com (SPEC-003), disabled by default
- Added "Post Now (Test)" button in Preferences to verify online scoring credentials
- Added Online Scoring tab in Preferences for credentials and posting interval
- Added Contest menu toggle for online score publishing with field validation
- Added contestOnlineScore metadata to 10 contest definitions
- Added CQ Zone, ITU Zone, ARRL Section fields to station info dialog
- Added auto-population of CQ/ITU zone from DXCC database when callsign is entered
- Fixed test-only mode corrupting user settings when run in parallel (race condition)
- Added band/mode QSO breakdown table to summary sheet (matches 3830scores.com format)
- Removed unnecessary asterisk prefix from multiplier listings in summary sheet
- Fixed operating time calculation when offTimeGapMinutes is 0 (now uses 15-minute floor)

## [0.6.14]
- Added CHANGELOG.md with CI-driven release notes from changelog entries
- Fixed make version inserting duplicate changelog sections
- Removed unnecessary cty.dat download from macOS CI build
- Removed qtserialport from CI Qt module installs
- Rewrote README for public-facing audience
- Fixed band map showing "No contest loaded" when rig is disconnected
- Fixed band map losing spots on band change
- Fixed band map not showing existing cluster spots when switching bands
- Added new screenshot to website
- Added Russian DX Contest (RDXC) contest definition
- Generalized contest engine prefix-based scoring rules (replaces hardcoded euCountry)
- Added visibleWhen support for conditional qsoFields and userPrompts based on prompt values
- Added scoringRuleConditions for station-type-dependent scoring rules
- Added {sn_sent} and {sn_rcvd} Cabrillo template substitutions
- Added --russian flag to generate_callsigns.py for Russian callsign generation
- Added RDXC automated test logs (DX and Russian station perspectives)
- Added SPEC-002 specification for stationClasses to userPrompts migration
- Fixed visibleWhen not checked in new log userPrompt collection path

## [0.6.13]
- Changed license to MIT
- Added versioned terms acceptance — dialog re-appears when terms change
- Added link to MIT license text in terms dialog
- Removed unused Qt6::SerialPort dependency
- Removed stale include path from build

## [0.6.12]
- Added prompt to refresh stale data files (cty.dat, master.scp) on startup
- Fixed TTS SSB voice memory leaving radio stuck in data mode
- Removed redundant DX Cluster label from cluster panel header

## [0.6.11]
- Added visual band map with DX cluster spot overlay
- Added band map zoom control and frequency axis labels

## [0.6.10]
- Added memory type indicator and toggle button to status bar

## [0.6.9]
- Added toggleMemoryType keyboard shortcut
- Fixed keyboard shortcut handling from floating dock widgets

## [0.6.8]
- Added VAQP (Virginia QSO Party) contest support
- Added rate widget
- Fixed crash on updateRunSPButtons at startup

## [0.6.7]
- Added Run/S&P/Off operating modes with Enter-key memory sequencing
- Added ESC halt support for TTS/SSB voice keying
