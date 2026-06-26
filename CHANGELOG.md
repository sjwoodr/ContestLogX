# Changelog

All notable changes to ContestLogX are documented in this file.

## [0.9.6]

## [0.9.5]

### CW Keying (WinKeyer)
- The WinKeyer serial connection now has configurable Baud and Stop bits in Rig Control -> CW keyer. Defaults stay at 1200 baud / 1 stop bit (8N1), which matches the classic WinKey spec and the AtomKey test device, so existing setups are unchanged. USB keyers that expect different framing (a real K1EL WKmini wants 2 stop bits / 8N2) can now be set to match. The Detect keyer probe uses the selected values, so you can test a setting without restarting
- Added diagnostic logging to the WinKeyer debug category: the Refresh button now logs every enumerated serial port with its location, description, manufacturer and USB VID:PID, and a failed host-open now logs whether the port could not be opened (busy/permissions) versus opened but got no handshake response (wrong baud/stop bits, or not a WinKeyer). This makes it possible to tell from the debug log why a keyer was not detected

### Contest Updates
- RAC Canada Day: Sable Island (CY0) and St. Paul Island (CY9) stations now score 10 points as Canadian stations

## [0.9.4]

### Contest Updates
- Added the IARU HF World Championship contest definition

### Contest Engine Changes
- Added an optional `scoring.exchangeRules` block that lets a scoring rule match on the received exchange itself. A rule can apply when the received exchange equals one of the operator's own `userPrompts` answers (`matchesPrompt`, compared numerically so `08` matches `8`) or when the received exchange contains letters (`exchangeIsAlpha`). The IARU HF World Championship uses both, `matchesPrompt` to detect a same-ITU-zone contact, and `exchangeIsAlpha` to detect HQ/official stations (which send a letter abbreviation rather than a numeric zone). Documented in `docs/contest-module-format.md` and `contests/README.md`

## [0.9.3]

### Other Changes and Bugfixes
- Updated FileLu affiliate link in docs

## [0.9.2]

### Other Changes and Bugfixes
- Added cloud backup for contest logs via FileLu or Amazon S3. Logs are always stored locally; when a provider is configured, every save is also mirrored to the cloud in the background (with status shown in the bottom bar). Configure providers under Preferences -> Cloud Storage (endpoint, region, bucket or bucket/folder, access/secret keys, with a Test connection button). Open lets you pull a log from the cloud (you choose where to save the local copy).
- ContestLogX users can get a free FileLu account via our affiliate link: https://filelu.com/ref495255525748.html 

## [0.9.1]

### Other Changes and Bugfixes
- Fixed the Windows CI build, which broke when the build runner upgraded to a newer compiler incompatible with the bundled Qt version

## [0.9.0]

### Other Changes and Bugfixes
- Fixed the CI build, which broke after WinKeyer support added a required Qt SerialPort dependency

## [0.8.4]

### Contest Updates
- Added the Alabama QSO Party (ALQP)

### Contest Engine Changes
- Extended `automaticMultipliers` to work under `multsPerMode`, not just `multsOnce` and `multsPerBand`. Per-mode semantics: each automatic multiplier is credited on every mode where the operator worked a `requiresWorkedFrom` entry. Used by the Alabama QSO Party so an AL station counts its own state, `AL`, as a state multiplier on each mode where they work any AL station. The summary sheet's per-band and per-mode multiplier breakdown now also surfaces automatic-credit entries so the printed multiplier list matches the scoring summary count

### Other Changes and Bugfixes
- Added CW keying via K1EL WinKeyer (and WinKey-compatible) keyers over a serial port, set per radio in Rig Control with a "Detect keyer" button to confirm the connection. The keyer runs on its own serial link, independent of the flrig/hamlib CAT backend, for lower-latency CW than keying through flrig
- Fixed window position and size not being restored on launch under X11

## [0.8.3]

### Contest Updates
- Added the Atlantic Canada QSO Party (ACQP) - first Sunday in June, 11-hour event. CW/SSB only, 80-10m. Cabrillo CONTEST tag: `AC-QSO-PARTY`
- Added the RAC Canada Day Contest - July 1 (Canada Day), 24-hour event. CW/SSB, 160-2m. Cabrillo CONTEST tag: `RAC-CANADA-DAY`

### Contest Engine Changes
- Extended `automaticMultipliers` to work under `multsPerBand`, not just `multsOnce`. Per-band semantics: each automatic multiplier is credited on every band where the operator worked a `requiresWorkedFrom` entry. No shipped contest uses this yet - available for custom contest modules. Documented in `docs/contest-module-format.md`
- Extended scoring-rule matchers with `<ruleName>Callsigns` arrays. Mirrors the existing `<ruleName>Prefixes` mechanism but matches on the worked station's full callsign instead of its DXCC prefix. Used by the RAC Canada Day Contest to give the 15 RAC official stations (VE1RAC, VE3RHQ, etc.) the 20-pt-per-QSO score that the rules call for. Documented in `docs/contest-module-format.md`
- Per-rule point lookups in `scoring.points` now accept mode keys in either case (`"cw"` / `"CW"`, `"phone"` / `"SSB"`), with `"phone"` recognized as an alias for `"SSB"` to match the convention used by the top-level `scoring.points` fallback. Existing contests that use `"CW"`/`"SSB"` (CQ WPX, etc.) continue to work unchanged

## [0.8.2]

### Contest Updates
- Added ARRL Field Day - the fourth full weekend of June, the annual emergency-preparedness operating exercise. CW/SSB/FM/Digital, 160-10m HF (no WARC bands) plus all bands 50 MHz and above. Exchange is operating class + ARRL/RAC section (e.g. `3A CT`). Phone QSO = 1 point, CW and Digital QSO = 2 points. Field Day has no QSO multipliers - final score is QSO points times the power multiplier (×5 for 5 W or less on natural power, ×2 for 100 W or less, ×1 for over 100 W). The Rule 7.3 bonus points (emergency power, GOTA, media publicity, satellite, youth participation, etc.) are proof-based claims submitted on the official ARRL Field Day entry and are not auto-computed. Cabrillo CONTEST tag: `ARRL-FIELD-DAY`
- Added the Kentucky QSO Party (KYQP) - first Saturday in June, 12-hour event (1300Z-0100Z). CW/SSB only, 80-10m plus 6m and 2m. Kentucky stations work everyone; non-Kentucky stations work Kentucky stations only. Phone QSO = 1 point, CW QSO = 2 points; multipliers (120 KY counties, US states + DC, Canadian provinces) count once each, DX is points-only. Final score is multiplied by power level - High Power ×1, Low Power ×2, QRP ×3. The K4KCG bonus is added by the KYQP committee after log submission, so it is not tracked in the app. Cabrillo CONTEST tag: `KYQP`
- Added the West Virginia QSO Party (WVQP) - Saturday on or before June 20 (statehood day), 12-hour event (1600Z-0400Z). CW/SSB/Digital, 80-10m, no WARC bands. West Virginia stations work everyone; out-of-state stations work WV stations only. Phone QSO = 1 point, CW and Digital QSO = 2 points; multipliers (55 WV counties, US states, Canadian provinces, and - for WV stations - DXCC entities) count once each. WV stations also count their own state, WV, as a state multiplier, credited once they work any WV station. Bonus station W8WVA is worth 100 points per band/mode QSO. The WV-mobile bonus (100 points per county activated) is a self-activation bonus that is not auto-tracked. Cabrillo CONTEST tag: `WVQP`

### Contest Engine Changes
- Added an optional `automaticMultipliers` block to a contest's `scoring.multipliers`. It credits a named multiplier the operator earns without an explicit exchange, keyed by a `userPrompts` answer. The West Virginia QSO Party uses it so a WV station counts its own state, `WV`, as a state multiplier - a WV station always sends a county and would otherwise never see `WV` in an exchange. The optional `requiresWorkedFrom` gate withholds the credit until the operator has worked a multiplier from a named list (WVQP gates on `inStateMults`, so `WV` is earned by working a WV station rather than handed out unconditionally). Supported with `multsOnce`. Documented in `docs/contest-module-format.md` and `contests/README.md`

## [0.8.1]

### Contest Updates
- Added the Canadian Prairies QSO Party (CPQP) - second weekend of May. CW/SSB only, 40-10m. Cabrillo CONTEST tag: `CPQP`
- Added the K1USN Slow Speed Test (SST) - every Monday 0000-0100 UTC and every Friday 2000-2100 UTC. CW only, 20 WPM upper limit, 160-10m. Cabrillo CONTEST tag: `K1USN-SST`
- Added the Arkansas QSO Party (ARQP) - third Saturday in May. CW/SSB/Digital, 160-2m. Cabrillo CONTEST tag: `ARQP`
- Added His Majesty The King of Spain CW (KOS-CW) - third full weekend of May, 1200Z Saturday to 1159Z Sunday. CW only, 160-10m. Cabrillo CONTEST tag: `EA-MAJESTAD-CW`
- Added the UN DX Contest - 16 May 0600-2059 UTC. CW/SSB, 80-10m. 212 KDA district codes as multipliers alongside DXCC entities. Cabrillo CONTEST tag: `UN-DX`
- Added the RSGB 3.5MHz Club Championship - short evening contests on 80m, Mondays/Wednesdays/Thursdays Feb-Jul. CW/SSB/PSK63/RTTY, 1.5 hours per session. RSGB special station (G6XX or regional variant) is worth 5 points per band+mode QSO. Cabrillo CONTEST tag: `RSGB-80M-CC`
- Added the CQ World-Wide WPX Contest - 48-hour worldwide event, two separate weekends scored as separate contests (CW: last full weekend of May; SSB: last full weekend of March). Single contest definition with a CW/SSB prompt at setup. 160-10m, RST + serial exchange, prefix multipliers counted once across the contest. Same-DXCC = 1 pt; intra-NA different-country = 2/4 pts (HF/LF); other same-continent = 1/2 pts; inter-continent = 3/6 pts. Cabrillo CONTEST tag: `CQ-WPX-CW` or `CQ-WPX-SSB` based on the selected weekend

### Other Changes and Bugfixes
- Added a positional argument form to `make version` - `make version 0.8.1` updates to 0.8.1 directly without the interactive prompt. The no-arg form still prompts as before
- Fixed the bonus-station details line in the summary sheet showing the wrong total when a contest uses `bonusPerBand`/`bonusPerMode`/`bonusPerBandAndMode` and the same bonus station was worked on multiple band/mode combinations. The line was computing `unique callsigns × pointsEach`, but the correct formula matches the engine's dedup logic - `unique (call, band, mode) keys × pointsEach`. Internal score calculation was already correct; only the displayed bonus subtotal was wrong (mismatched with the final score line directly above it). Surfaces in the new RSGB 3.5MHz Club Championship and would also apply to ARQP if the WR5P bonus station is worked on more than one band+mode combination
- Fixed `DxccDatabase::stripPortableSuffixes` short-circuiting on the base call's prefix before checking the portable designator, so calls like `W1AAA/KH6` were returning USA (continent NA) instead of Hawaii (OC). The reordered check is: license-class suffix → digit-only call-area indicator → exact full-prefix match on the designator → partial-prefix match → base-call prefix fallback. Affects DXCC-driven scoring in every contest; CQ WPX surfaced the bug because its byBand HF/LF point tier amplifies any wrong continent classification
- Added `eadx100` as a new multiplier category, backed by a bundled `data/eadx100.json` reference file containing the 344 active EADX-100 entities (URE-curated, ≈superset of ARRL DXCC plus Sicily, Bear Island, Shetlands, etc.) and 62 historical/cancelled entities. Used by URE-sponsored contests as their entity multiplier source. Optional `eadx100Excludes` list in the contest definition lets a contest suppress specific entity prefixes from this category - used by King of Spain to skip EA/EA6/EA8/EA9, since Spanish stations contribute the more granular province multiplier and counting via both categories would double-credit. Entries flow into the same per-QSO entity bucket as `dxcc` (a contest uses one or the other, not both - they cover ≈99% the same set), and the score widget and summary sheet relabel "DXCC" → "EADX-100" when the eadx100 category is active
- Extended `multAliases` with a new `sourceValues` field - an inline array of exact-match values that lets one alias rule target a specific subset of values without requiring a named list. Used by CPQP to map each prairie province's Federal Electoral District codes to its 2-letter province code in three rules (one per province). Backward-compatible - every existing contest definition still works unchanged
- Added `wpxPrefix` as a new multiplier category - extracts the CQ WPX prefix dynamically from each callsign per the official rules (portable designators like `PA/N8BJQ → PA0` and `W1AW/KH6 → KH6`, call-area changes like `W1AW/4 → W4`, license-class suffix stripping for `/M`/`/MM`/`/A`/`/E`/`/J`/`/P`/`/QRP`, and `0`-padding for callsigns without numbers like `XEFTJW → XE0`). Distinct from `namedCallPrefixes`, which works against a static prefix list (e.g. YBDX). Counts flow into the same accounting bucket as `namedCallPrefixes`, so `multsOnce` / `multsPerBand` / `multsPerMode` / `multsPerBandAndMode` all work with `wpxPrefix` unchanged
- Added `byBand` keys inside scoring rule definitions - lets a relationship rule tier its point values by band (e.g. CQ WPX same-continent QSOs earn 1 pt on 28/21/14 MHz and 2 pts on 7/3.5/1.8 MHz). Falls through to the existing per-mode and per-prompt lookups when not specified
- Added `bothInXX` scoring rules - fire when both stations are located in continent `XX` (e.g. `bothInNA` for the CQ WPX North-America-only point exception). The 2-letter continent code is appended to the rule name; falls back through the existing precedence chain when the rule does not match


## [0.8.0]

### Contest Updates
- Added the New England QSO Party (NEQP) - first full weekend of May. CW/SSB/Digital, 80-10m. Cabrillo CONTEST tag: `NEQP`
- Added the Delaware QSO Party (DEQP) - first full weekend of May. CW/SSB/RTTY/PSK, 160-10m. Cabrillo CONTEST tag: `DEQP`

## [0.7.36]

### Contest Updates
- Added the 7th Call Area QSO Party (7QP) contest definition (`contests/7qp.json`). 18-hour event held the first Saturday in May, 1300Z-0700Z. 7th-area stations (AZ/ID/MT/NV/OR/UT/WA/WY) work everyone; non-7th-area stations work 7th-area stations only. Exchange is RST + 5-letter `<state><county>` for 7th-area stations or 2-letter state/province/`DX` for non-7th-area. Scoring: CW=3, SSB=2, Digital=4 points per QSO. Multiplier handling: non-7th-area operators count 7th-area counties (max 259); 7th-area operators count states + provinces + DXCC. Bands: 160/80/40/20/15/10m. WSJT modes (FT8/FT4/MSK144/JT65) excluded per sponsor rules - RTTY/PSK/AMTOR/etc. allowed. Cabrillo CONTEST tag: `7QP`
- Added the Indiana QSO Party (INQP) contest definition (`contests/inqp.json`). 12-hour event held the first full weekend of May, 1500Z Saturday to 0259Z Sunday. Indiana stations work everyone; non-Indiana stations work Indiana stations only. Exchange is RST + 5-letter `IN<county>` for Indiana stations or 2-letter state/province for non-Indiana W/VE (DC counts as MD via `namedMultAliases`); DX otherwise. Scoring: 2 points per QSO (CW or SSB) - matches the 2026 rule change. Multiplier handling is `multsPerMode` so each mult counts once on CW and once on SSB. Non-Indiana operators count the 92 IN counties only; Indiana operators count counties + the other 49 US states + 13 Canadian provinces (plus DC via the alias). Indiana stations may work DX for QSO point credit but DX is not a multiplier. CW and Phone only - no digital modes per sponsor rules. Cabrillo CONTEST tag: `INQP`

### Contest Engine Changes
- Extended the `multAliases` mechanism in the contest engine with two new fields. `promptValueIn: ["VAL1", "VAL2", …]` is an any-of trigger that fires when the operator's `userPrompts` answer is in the list (overrides the legacy single-value `promptValue` when present). `mapByPrefix: N` extracts the first N characters of the received exchange as the multiplier (e.g., `WYALB` → `WY` with `mapByPrefix: 2`), wins over the legacy `mapsTo` static replacement when both are present. Used by 7QP to prefix-extract 5-letter `<state><county>` codes to 2-letter state mults for 7th-area operators, while non-7th-area operators see each county as a separate mult per the existing `receivedExchangeFilter`. The new fields are fully backward-compatible - every existing contest definition that uses `multAliases` (FQP, etc.) is unaffected. Documented in `docs/contest-module-format.md`, `docs/contest-engine-overview.md`, and the public docs page

### Other Changes and Bugfixes
- Fixed the `multsPerMode` multiplier-details summary printer treating LSB and USB as separate mode buckets, even though the scoring engine correctly normalizes both to "SSB" for credit. The same SSB-mode multiplier could be listed under both an "LSB" and a "USB" section in the printout - confusing for the operator and double-counted by the automated test runner's parser. The printer now uses the same {CW, SSB, DIGITAL} mode-category normalization the engine uses, so the printout matches the score
- Added a `--config-dir <path>` CLI flag for sandbox-isolated CLX sessions. When set, both the JSON settings (`ContestLogX.json`) and the QSettings INI (used by Debug toggles) read and write within `<path>` instead of the platform default. If `<path>` doesn't already contain a `ContestLogX.json`, the user's real config is copied in as a starting point so the sandbox session has full state (callsign, CW memories, station info, terms-accepted version, saved layout, etc.). Used for local smoke testing without stomping the user's real config - and now also used by the automated log-test runner (`scripts/run_log_tests.py`) which creates a fresh per-test sandbox via `tempfile.mkdtemp` so parallel test workers can't race on writes to the user's real config dir
- Updated `CLAUDE.md` with an architectural overview of the CW Decoder pipeline (AudioCapture → BinChannel → CwDecoder → CwDecoderWorker → CwDecoderWidget) plus three new Common Pitfalls entries: the moveToThread thread-affinity rule (must be on the source thread, not the destination - silently breaks Windows WASAPI otherwise), the `device.preferredFormat()`-unchanged + per-frame max-abs channel mixdown rule (any of the intuitive alternatives - request mono Int16, take channel 0, average channels - silently fails on at least one platform), and the `clx_debug.log` per-platform path
- Updated `web/src/pages/docs.astro` with the new CW Decoder Word Gap control description, Auto squelch checkbox description, and an updated "WPM Tracking" section that no longer references the removed WPM range spinboxes (gone since 0.7.33). Softened the `TUK4RO`-style fused-token entry under Known Limitations to point at the Word Gap knob as a mitigation

## [0.7.35]

### Other Changes and Bugfixes
- Added an Auto checkbox next to the CW Decoder squelch slider - when checked, the worker recomputes the squelch threshold every second from per-bin noise-peak estimates (BinChannel's 90th-percentile of recent magnitudes) using the **median across bins** rather than max. Most bins are noise-only at any given moment, so the median lands on a noise bin almost always; this avoids the "max would catch the signal bin and gate the decoder" failure mode. Margin is `medianPeak × 1.8 + 0.02`, clamped to `[0.08, 0.6]`. The slider is disabled while Auto is on but visually tracks the auto-detected value so the operator can see the level the decoder picked. Toggling Auto off takes manual control back at the last auto value. State persists per-radio (Radio L / Radio R). **Known limitation:** when chasing a weak station whose signal magnitude is barely above the noise floor, Auto's safety margin can gate the weak signal out - uncheck Auto and slide manually for weak-signal copy (the checkbox tooltip notes this)

## [0.7.34]

### Other Changes and Bugfixes
- Added `scripts/clx-radio-usb` - a developer helper for the QEMU/KVM Windows VM workflow. Attaches and detaches the rig's two USB devices (Silicon Labs CP2102 for IC-7300 CAT, and the TI PCM2901 audio codec for receive audio) to/from a libvirt-managed Windows VM via `virsh attach-device` / `virsh detach-device`, so the host's PulseAudio and `/dev/ttyUSB*` get the devices back automatically when the VM doesn't need them. Subcommands: `attach`, `detach`, `toggle`, `status`. VM name defaults to `win11`; override with `CLX_VM_NAME`. Useful when iterating on Windows-specific changes (CW decoder audio path, MSVC-only behavior) without cutting a release each time

## [0.7.33]

### Other Changes and Bugfixes
- Added a Word Gap control to the CW Decoder widget toolbar - a multiplier (× dot length) that controls how aggressive the decoder is about inserting spaces between words. Default 4.0 is a contest-friendly compromise between textbook 7× spacing and tightly-sent QRQ contest CW that runs ~3×. Lower the value (e.g. 3.0-3.5) when listening to compressed contest sending where callsigns and exchanges run together; raise it (e.g. 6.0-7.0) for textbook timing. Range 3.0-8.0 in 0.5 steps; persists per-radio (Radio L / Radio R)
- Removed the WPM range spinboxes from the CW Decoder widget - they consumed two slots of toolbar space but the operator-relevant range (5-60 WPM) covers every CW signal on the bands, so per-operator tuning had no real value. Range is now hard-coded to 5-60. Toolbar real estate reclaimed for the Word Gap control above
- Restored CW Decoder mono-mixdown signal level - 0.7.29 changed the multi-channel-to-mono path from "take channel 0" to "average all channels," which captured signal regardless of whether the rig was wired to the left or right channel of a stereo USB CODEC, but also attenuated a typical mono-on-stereo signal by ~6 dB and added the noise floor of the empty channel into the working signal. The user-visible symptom was the decoder catching real CW tokens (callsigns and RST exchanges) but also a steady stream of single-dit "E" / single-dah "T" false-positives - noise breaching the on-threshold once SNR was no longer matched to its hysteresis. Now picks the channel with the larger absolute value per frame: full signal level whether the rig is wired to L or R, no 6 dB hit on stereo CODECs, no false positives from an empty-channel noise floor
- Fixed CW Decoder audio path stalling on Windows after only 2-3 audio chunks (~20-30 ms of samples) so the decoder appeared "dead" even though the QAudioSource opened cleanly and Windows showed ContestLogX as actively using the microphone - root cause was AudioCapture being moved to the worker thread from the *destination* thread (inside the worker's startCapture slot) instead of the *source* thread (in the widget, where AudioCapture was constructed). Qt's contract requires moveToThread to be called from the object's current thread; calling it from the destination is technically forbidden and on Windows left Qt's WASAPI plugin uncertain about which thread owned its internal polling timer, manifesting as a silent stall after the initial buffered burst. CwDecoderWidget::beginDecoding now does the move from the main thread before invokeMethod hands the pointer off, and the worker side asserts the move already happened
- Added always-on QAudioSource state-change logging - every Active / Idle / Suspended / Stopped transition is logged with the current QAudio::Error value, so future "decoder shows nothing" reports surface the failure mode (e.g. "Active -> Idle (NoError)" = waiting for data, "Active -> Stopped (IOError)" = driver failure) instead of failing silently. Always-on, not gated by CW Decoder Debug, because state transitions are rare and exactly the smoking gun needed for triage
- Updated `docs/BUILD.md` with full Windows local-build instructions (Visual Studio + Qt + scripts/build-windows.ps1), a brief macOS section, and a Strawberry-Perl/MinGW shadowing-MSVC gotcha that produces "undefined reference to `__imp__ZN5QFontD1Ev`" link errors when an x86_64 MinGW toolchain ends up ahead of MSVC on PATH. Linux section unchanged. Also fixed the runtime data section's claim that `clx_debug.log` lives in cwd - since 0.7.30 it lands in `QStandardPaths::AppLocalDataLocation`

## [0.7.32]

### Other Changes and Bugfixes
- Fixed CW Decoder failing to emit characters when the operator left the squelch slider at 0% - the Schmitt off-threshold expression `qMin(squelch * 0.9, …)` collapsed to zero when squelch was zero, so any non-zero magnitude held the tone "on" forever and elements never closed (no key-up edge → no dits/dashes → no characters), regardless of how strong the CW signal actually was. Now the squelch ceiling falls back to 1.0 when squelch=0 so the floor-lifted and peak-relative branches still produce a sane off-threshold; the on-threshold also degrades to a noise-floor-lifted value so noise doesn't constantly trigger key-down. Net result: leaving squelch at zero now works for clean CW signals, instead of being a silent foot-gun
- Added an audio-flow heartbeat to AudioCapture - when CW Decoder Debug is enabled, log a single line every 500 chunks (~5 s at 10 ms/chunk) confirming the QAudioSource is still delivering data. Catches the failure mode where Windows WASAPI delivers an initial buffered burst (~300 ms) and then silently stops calling `readyRead`, which otherwise looks identical to "decoder is broken" from the UI side. The first three chunks still log unconditionally for fast triage; the heartbeat only kicks in afterwards when the toggle is on so normal operation stays quiet

## [0.7.31]

### Other Changes and Bugfixes
- Added debug-gated diagnostic logging to the CW Decoder pipeline to triage "decoder shows nothing" reports when the audio path is confirmed working but no characters appear. Enable Debug → CW Decoder Debug to get: a one-line decoder configuration summary at capture start (passband Hz range, every bin's center frequency, WPM bounds, squelch threshold); a per-bin magnitude snapshot every 5 seconds (each bin's current normalized Goertzel magnitude with a `*` marker on bins currently in tone-active state) so it's instantly visible whether the operator's CW pitch lands in any configured bin with usable signal level; and a per-character emission log so it's clear whether characters are being decoded at all (vs not being shown in the UI). All three are gated by the existing toggle so they don't bloat the log in normal use

## [0.7.30]

### Other Changes and Bugfixes
- Fixed CW Decoder receiving only ~300 ms of buffered audio on Windows and then going silent on the same USB CODEC where 0.7.29 finally got the format/channel handling right - the AudioCapture object was being created on the main thread and handed to the worker thread via `setParent()`, but `setParent()` does not move QObject thread affinity. As a result the QAudioSource and its WASAPI polling timer lived on the main thread while the worker thread drove `readyRead` handlers, generating a flood of `QObject::startTimer / killTimer: Timers cannot be (started/stopped) from another thread` warnings on every audio block and (apparently) corrupting WASAPI's polling state so only the initial buffered chunk delivered real samples. The worker now explicitly `moveToThread()`s the capture before assigning the parent, so the audio pipeline lives entirely on the worker thread
- Fixed Debug Log Viewer ("Could not open log file") on Windows - the debug log path defaulted to the relative filename `clx_debug.log`, which resolved against whatever working directory CLX inherited at launch. On Windows that was typically `C:\Program Files\ContestLogX\` (not writable for normal users; UAC virtualization redirected the actual write to `%LOCALAPPDATA%\VirtualStore\…`), and the in-app log viewer then tried to read from the original install-dir path and failed. Default path is now an absolute, per-user, writable location via `QStandardPaths::AppLocalDataLocation`: `~/.local/share/ContestLogX/clx_debug.log` on Linux, `~/Library/Application Support/ContestLogX/clx_debug.log` on macOS, `C:\Users\<user>\AppData\Local\ContestLogX\clx_debug.log` on Windows. `--debug-log <path>` on the command line still overrides as before

## [0.7.29]

### Other Changes and Bugfixes
- Fixed CW Decoder receiving zero audio on Windows even when Microphone privacy settings show ContestLogX as "Currently in use" - the WASAPI backend was reporting `isFormatSupported(monoInt16)` as true and the stream opened cleanly, but the driver only actually delivered samples in its native format (typically stereo Float on USB CODEC devices), so every readyRead callback returned zero-filled buffers. AudioCapture now uses each device's preferred format unchanged and converts to mono int16 in software (handles every Qt sample format and arbitrary channel counts already)
- CW Decoder mono mixdown now averages all input channels instead of taking only channel 0, so USB CODECs that route the rig audio to the right channel (or any channel other than the first) capture the signal correctly
- Added a 3-second silence watchdog to AudioCapture - if a real-device capture stream is open but every sample so far is zero, a clear warning dialog is shown to the operator (with platform-specific causes: muted level, wrong endpoint, exclusive-mode conflict, OS privacy block) instead of the decoder silently showing nothing. AudioCapture now also logs the negotiated format and the first three audio chunks (size, frames, peak amplitude) unconditionally so "decoder shows nothing" reports can be triaged from the debug log without flipping per-component debug toggles first
- Added an OS microphone-permission check to the CW Decoder - the widget logs the current permission state (Granted / Denied / Undetermined) for each radio at construction (visible in the debug log alongside the audio device enumeration), and re-checks before starting a real-device capture. On Denied, a platform-specific dialog explains where to grant access (Windows: Privacy & security → Microphone → Let desktop apps access your microphone; macOS: System Settings → Privacy & Security → Microphone) instead of letting the decoder come up silent. On Undetermined, the OS is asked for a decision and decoding auto-resumes if granted. Skipped for the practice virtual sources since they synthesize audio internally

## [0.7.28]

### Other Changes and Bugfixes
- Fixed Linux AppImage failing to launch with `dlopen(): error loading libfuse.so.2` on Ubuntu 22.04+ / Debian 12+ and other modern distros that no longer ship `libfuse2` by default - the AppImage now uses the modern type2 runtime (supports both fuse2 and fuse3) instead of the fuse2-only runtime that AppImageKit's `appimagetool` bundles by default. Regression introduced in 0.7.26 when AppImage packaging switched from `linuxdeploy --output appimage` to `appimagetool` to fix CW Decoder GStreamer plugin bundling

## [0.7.27]

### Contest Updates
- Fixed Florida QSO Party (FQP) Cabrillo CONTEST header - now emits `FCG-FQP` (the contest sponsor's required identifier) instead of `FQP`

### Other Changes and Bugfixes
- CW memory buttons (F1-F8) in the CW console now show the full memory text as a tooltip on hover, so you can see exactly what each one sends without opening the memories editor; empty slots show no tooltip
- Fixed SSB memory phonetic playback for digits - "9" is now spoken as "nine" (matches FCC / ARRL Operating Manual phonetics) instead of "niner" (NATO / ICAO aviation usage); affects `{mycall}` and any other callsign-phonetic expansion

## [0.7.26]

### Other Changes and Bugfixes
- Added Start/Stop button to the CW Decoder widget toolbar (next to Clear) - defaults to "Stop" while decoding; click to halt the decoder and silence any practice-audio output, label flips to "Start", click again to resume against the currently-selected audio device. Disabled when the device is "(none)"
- Added "View Debug Log" entry at the bottom of the Debug menu - opens a standalone (non-dockable) window that tails the active `clx_debug.log` file (~1.5s poll), with Clear, Copy (sends the visible buffer to the clipboard), and Close buttons and an Auto-scroll checkbox so you can scroll back to inspect older lines without the view jumping to the bottom; runs as an independent top-level window so you can leave it open and switch back to CLX
- CW Decoder now logs every detected audio input device (description, ID, default flag, preferred sample rate / channels / format) plus the saved per-radio device selection on widget construction - emitted unconditionally (not gated by the CW Decoder Debug toggle) so "decoder shows nothing" reports can be triaged without the operator having to enable per-component debug first

## [0.7.25]

### Other Changes and Bugfixes
- Practice - Contest Exchange now adds two QRM stations (one above and one below the primary tone at asymmetric offsets, ~12-14 dB quieter, slightly different WPM) so the decoder and the operator experience realistic contest-band conditions with adjacent-channel interference; Rag Chew remains a single clean signal for pure head-copy practice
- Added CW Decoder entry to Preferences → Fonts so the font family and size of the decoded-text rows (and the surrounding toolbar / labels) can be customized; applies at widget spawn and on bin-count changes
- Added Remote Dashboard - embedded HTTP server serves a read-only dashboard on your LAN (score, rate, recent QSOs, rig state, propagation). Works on phone, tablet, or another PC browser (responsive layout). Preferences → Dashboard to enable; copy the bookmarkable URL or scan the QR code with your phone camera. Token-based auth; disabled by default
- Added rig-control POST endpoints to the Remote Dashboard - /api/rig/qsy (set freq/mode), /api/rig/band (jump to 20m/40m/etc), /api/rig/run_mode (Run/S&P/Off)
- Added Rig Control card to the Remote Dashboard page - band buttons (160-6m), mode buttons (CW/USB/LSB/DIG/FM/AM), Run/S&P/Off toggle, freq-in-MHz input with Go. SO2R sessions get an L/R radio toggle at the top. Current rig state highlights the matching button; taps POST to the V2 endpoints and the display reconciles on the next poll
- Added Apply button to Preferences - saves changes and triggers post-save actions (font reapply, Remote Dashboard restart, etc.) without closing the dialog, so you can iterate and see the effect immediately (e.g. enable the Dashboard, click Apply, scan the QR code)

## [0.7.24]

### Other Changes and Bugfixes
- Added CW Decoder Practice mode - two virtual audio sources in the decoder's Audio dropdown ("Practice - CW Rag Chew" and "Practice - Contest Exchange") that synthesize CW on the default output device and feed the same tones into the decoder pipeline, so operators can head-copy random QSOs / exchanges and check their copy against what the decoder captured; WPM follows the CW console setting, so speed matches the keyer
- Practice - Contest Exchange uses the active contest's named multipliers and sends with fixed 5NN RST (entry is disabled until a contest is loaded)
- Practice audio sources reset to "(none)" on startup so CLX doesn't start generating CW on the speakers the moment it launches - operator must explicitly re-select practice each session
- Updated website with CW Decoder Practice mode - feature bullet on the index page and a dedicated subsection in the docs page
- Added macOS first-launch instructions to the download page - explains how to unlock the ad-hoc-signed bundle via System Settings → Privacy & Security or via a single Terminal command, and notes the microphone permission prompt for the CW Decoder

## [0.7.23]

### Other Changes and Bugfixes
- Fixed macOS app bundle failing Gatekeeper verification - re-sign bundle ad-hoc after macdeployqt rewrites rpaths so Sealed Resources are properly generated
- Fixed CW Decoder receiving no audio on macOS - add NSMicrophoneUsageDescription to Info.plist so macOS TCC prompts for microphone access instead of silently returning a zero-filled stream
- Built macOS app bundle as universal binary (x86_64 + arm64) so it runs natively on both Intel and Apple Silicon Macs; minimum macOS version is now 11.0 Big Sur

## [0.7.22]

### Other Changes and Bugfixes
- Fixed CW Decoder failing to build on Windows (MSVC does not define M_PI without _USE_MATH_DEFINES)

## [0.7.21]

### Other Changes and Bugfixes
- Added CW Decoder - dockable multi-channel Morse decoder per radio (Window → CW Decoder) with N parallel frequency bins, auto-adaptive WPM tracking, and clickable callsign / RST tokens
- Added per-radio Audio Input Device selector in Rig Connection Settings and the decoder widget's toolbar (supports USB audio, virtual cable, soundcard); "(none)" disables the decoder for that radio
- Added per-radio "Mute decoder on PTT" toggle - gates the decoder during transmit so own keying doesn't self-decode; covers both manual keying and F-key / CW console sends
- Decoded callsigns (blue) and RST (amber) are clickable - fill the owning radio's CALL or RSTr field and trigger SCP / call-history / dupe check / QRZ auto-lookup just like keyboard entry; cut-numbers (5NN, 55N, 5N9) normalize to digits on fill
- Callsign recognition handles standard, portable-suffix (K1ABC/P), and prefix-slash (IT9/DK6XZ, DL/K1ABC) formats
- Added CW Decoder Debug Logging toggle under the Debug menu

## [0.7.20]

### Contest Updates
- Added Michigan QSO Party (MIQP) - MI and non-MI station classes, 83 MI counties, states/provinces/DX multipliers per mode, CW and SSB only

### Other Changes and Bugfixes
- Added QSY Back (Alt+B) - tunes rig to the frequency/mode of the previous QSO; press repeatedly to walk further back through the log

## [0.7.19]

### Other Changes and Bugfixes
- Added WSJT-X UDP listener - receives QSO Logged messages and pre-fills call, exchange, RST, and frequency/mode fields for operator review before logging
- RST validation now accepts digital signal reports (e.g., -15, +05) in addition to traditional RST format

## [0.7.18]

### Contest Updates
- Added Japan International DX Contest (JIDX) - CW and SSB, JA and DX station classes, band-based scoring, prefecture and CQ Zone multipliers per band
- Added OK-OM DX Contest - CW and SSB, OK/OM and DX station classes, 165 Czech/Slovak county multipliers, DXCC+WAE entities per band
- Added New Mexico QSO Party (NMQP) - NM and non-NM station classes, 33 NM counties, states/provinces/DXCC multipliers, power multiplier (QRP/LP/HP), DC/MDC alias to MD
- Added Missouri QSO Party (MOQP) - MO and non-MO station classes, 115 MO counties, states/provinces/DX multipliers, W0MA and K0GQ bonus stations
- Added Georgia QSO Party (GAQP) - GA and non-GA station classes, 159 GA counties, states/provinces multipliers per mode, CW and SSB only
- Added North Dakota QSO Party (NDQP) - ND and non-ND station classes, 53 ND counties, states/provinces, DX points-only (no DX mult), NL→NF alias

### Contest Engine Changes
- Added direct band-based scoring (byBand without month nesting)
- Added 1:1 named mult aliases for exchange normalization (e.g., "5" → "05")
- Added per-station-class named mult display lists and labels (e.g., "CQ Zones" for JA, "Prefectures" for DX)
- Added per-station-class point overrides (byPrompt) for asymmetric scoring rules
- Added received exchange validation - `namedMultOnly` blocks invalid exchanges (e.g., serial numbers where a county code is required); `namedMultOrSerial` accepts either named mults or serial numbers; both respect namedMultAliases

### Other Changes and Bugfixes
- Fixed exchange validation in several contests

## [0.7.17]
- Fixed light theme not working when running directly on GNOME/Wayland - use stylesheet-based theming that overrides GTK platform integration
- Removed hardcoded dark colors from DX Cluster spot table so it follows the active theme
- Theme changes now apply immediately with a note that some elements may need a restart
- Added "Use X11 backend" option in Preferences > Display (Linux only, on by default) - fixes window and floating dock position restore on Wayland
- Added cross-platform rules to CLAUDE.md

## [0.7.16]
- Replaced 7-Zip SFX with Inno Setup installer for Windows - proper install wizard, Start Menu shortcut, desktop shortcut, uninstaller, and post-install launch
- Added Windows application icon - exe, installer, taskbar, and shortcuts all show the ContestLogX icon

## [0.7.15]
- Fixed Windows SFX installer failing to launch after extraction - use %%T extract path variable and fix config whitespace

## [0.7.14]
- Fixed 7zSD.sfx download failing in CI (7-zip.org blocking bare curl requests)

## [0.7.13]
- Improved Windows SFX build - fetch 7zSD.sfx (public domain) at build time for proper installer UX with default extract path to %LOCALAPPDATA%\ContestLogX

## [0.7.12]
- Fixed Windows SFX build failing due to cmd.exe path escaping issue in CI

## [0.7.11]
- Changed Windows build from zip to self-extracting exe that installs to %LOCALAPPDATA%\ContestLogX

## [0.7.10]
- Added Windows build to CI pipeline - produces a self-contained portable zip via MSVC + windeployqt
- Updated download page with Windows download link and version bumps for all platforms

## [0.7.9]
- Fixed hidden dock widgets (DX Cluster, CW Console) reappearing on restart due to startup race condition where visibilityChanged signals overwrote saved settings before they could be read

## [0.7.8]
- Added validation when activating Run or S&P mode - if required memory roles (My Call/CQ/Run Exchange/TU for Run, My Call/S&P Exchange for S&P) are not assigned, an error dialog is shown and the CW or SSB memory editor opens automatically
- Added duplicate-role validation in the CW and SSB memory editor dialogs - cannot save if the same role is assigned to more than one memory slot
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
- Added versioned terms acceptance - dialog re-appears when terms change
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
