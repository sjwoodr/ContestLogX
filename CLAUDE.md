# ContestLogX

A C++ Qt6-based amateur radio contest logging application.

- **Language:** C++17
- **Framework:** Qt6
- **Build system:** CMake
- **Version:** 0.0.9
- **Platforms:** Linux, macOS, Windows

## Cross-Platform Rules

ContestLogX ships on Linux (AppImage), macOS (app bundle), and Windows (Inno Setup installer). All code changes must work on all three platforms:
- Use `#ifdef Q_OS_LINUX`, `Q_OS_MACOS`, `Q_OS_WIN` for platform-specific code
- Do not use platform-specific APIs without guarding them
- Test that new features don't break the CI builds for other platforms
- File paths: use `QDir`, `QStandardPaths`, and `/` separators (Qt normalizes them)
- The Windows build uses MSVC; avoid GCC/Clang-only extensions

## Dependencies

- **CMake** >= 3.16
- **Qt6** (Core, Widgets, Network, Xml)
- **C++17** compiler (GCC, Clang, or MSVC)
- **Python 3** (for test scripts)

### Linux (Ubuntu/Debian)
```bash
sudo apt install cmake qt6-base-dev libqt6serialport6-dev libqt6xml6-dev
```

### macOS
```bash
brew install qt6 cmake
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)  # if Qt6 not found automatically
```

## Build & Run

```bash
make              # build
./clx             # run
./clx --debug --log <logfile.clx>  # debug mode
make test         # unit tests
make test-logs    # automated contest log validation with multiplier verification
```

## Directory Structure

| Directory    | Contents                                                        |
|--------------|-----------------------------------------------------------------|
| `src/`       | Source code organized by module (`ui/`, `core/`, `database/`, `engine/`, `rig/`) |
| `include/`   | Header files                                                    |
| `contests/`  | Contest definition JSON files (naqp.json, arrl_10m.json, etc.)  |
| `tests/`     | Unit and integration tests                                      |
| `ui/`        | Qt UI files (.ui)                                               |
| `data/`      | Data files and default configurations                           |
| `resources/` | Images and other resources                                      |
| `docs/`      | Documentation                                                   |

## Core Modules

### ContestEngine (`src/engine/contestengine.cpp`)
Core scoring engine - loads contest definitions from JSON, validates QSO exchanges, scores QSOs with points and multipliers. Multiplier types:
- **namedMults** - exchange field values (e.g., STATE in NAQP)
- **dxccMultipliers** - DXCC entities (once per contest or per band)
- **ituRegions** - ITU regions (per band)
- **namedCallPrefixes** - call sign prefixes (e.g., YB0-YB9 in YBDX)
- **gridSquareMultipliers** - Maidenhead grid squares per band (ARRL VHF)

### DxccDatabase (`src/database/dxccdatabase.cpp`)
DXCC entity and prefix lookup. Handles slash notation: `PJ2/N9OH` resolves to PJ2 prefix. Portable suffix: `YB1AR/2` resolves to YB2 for namedCallPrefixes. Maps 346 DXCC entities with 7058 prefixes.

### MainWindow (`src/ui/mainwindow.cpp`)
Main application window - QSO data entry/logging, contest setup, user prompts, summary sheet generation, scoring worker thread management.

### QsoRecord (`src/qsorecord.cpp`)
Single QSO data structure. Fields: DATE, TIME, CALL, FREQ, MODE, RSTs/RSTr, SNs/SNr, GRIDs/GRIDr, POINTS, and contest-specific multiplier fields.

### CW Decoder (`src/audio/`, `include/audio/`, `src/ui/cwDecoderWidget.cpp`)
Multi-channel Morse decoder pipeline that listens to a per-radio audio device and displays decoded text in a dock widget. Architecture:
- **AudioCapture** (`src/audio/audioCapture.cpp`) — wraps `QAudioSource`, converts multi-channel input to mono int16, pushes into an SPSC ring buffer. See "Common Pitfalls" for the format/channel and threading rules.
- **BinChannel** (`src/audio/binChannel.cpp`) — single-frequency Goertzel detector + Schmitt trigger + dot/dash classifier + WPM estimator. One instance per UI row.
- **CwDecoder** (`src/audio/cwDecoder.cpp`) — owns N `BinChannel` instances, fans out audio blocks to all of them per processBlock call.
- **CwDecoderWorker** (`src/audio/cwDecoderWorker.cpp`) — runs on its own `QThread`; drains the ring buffer, calls `m_decoder.processBlock`, emits `charDecoded` / `wpmUpdated` / `squelchAutoUpdated` / `binLayoutChanged`.
- **CwDecoderWidget** (`src/ui/cwDecoderWidget.cpp`) — dockable `QDockWidget` per radio. Toolbar: Audio device, Center Hz, Bins, Word Gap, Squelch (manual slider + Auto checkbox), Mute indicator, Clear, Start/Stop. Two virtual "Practice" sources (rag-chew, contest exchange) live above real devices in the Audio dropdown.
- Settings persist per-radio under `cwDecoder.{left,right}`: center, bins, squelch + squelchAuto, wordGapMultiplier. WPM range is hardcoded 5–60 (the spinboxes were removed in 0.7.33 — covers every CW signal on the bands and isn't worth UI real estate).

### Rig Control (`src/rig/`, `include/rigInterface.h`)
Three rig backends behind a common `RigInterface` abstract base class:
- **FlrigClient** (`src/rig/flrigClient.cpp`) — XML-RPC client for flrig. Full feature set: freq, mode, CW keying (cwio), PTT, power, bandwidth. Synchronous I/O on main thread.
- **HamlibClient** (`src/rig/hamlibClient.cpp`) — TCP text protocol client for rigctld. Freq/mode control; CW/PTT/power depend on rig. Uses `HamlibWorker` on a background `QThread` to avoid blocking the UI. Getters return cached values from background polling; setters are fire-and-forget.
- **MockedRigClient** (`src/rig/mockedRigClient.cpp`) — Simulated rig for testing and SO2R practice. Accepts all commands, returns last-set values. Defaults to 14.200 MHz USB.
- Backend selected in Settings (`rig.backend`: `"flrig"`, `"hamlib"`, or `"mocked"`), each with independent host/port/autoConnect.

## Naming Conventions

- **Exchange fields:** 3-letter codes with `s` (sent) or `r` (received) suffix: RSTs, RSTr, SNs, SNr, EXCHs, EXCHr, GRIDs, GRIDr, NAMEs, NAMEr
- **QSO log columns:** Uppercase: DATE, TIME, CALL, FREQ, MODE, POINTS
- **Mode tracking:** Score widget uses `PH` for PHONE mode (SSB, AM, FM combined)

## Contest Definition Format (contests/*.json)

JSON files defining contest rules. Key sections:
- **metadata** - name, version, description, sponsor, bands, modes, url
- **frequencies** - band frequency allocations with mode sub-ranges
- **stationClasses** - operating categories (SOLP, SOHP, SO3B) with exchange requirements
- **exchangeFields** - sent/received field definitions
- **scoring.qsoPoints** - points based on relationship (sameDxcc, sameContinent, etc.) or byBand/month
- **scoring.multipliers** - multiplier definitions with type (multsOnce, multsPerBand, multsPerMode, multsPerBandAndMode)
- **scoring.finalScore** - formula string (e.g., `SUM(points) * (namedMults + dxccMultipliers)`)
- **validation** - rules, namedMults array, namedCallPrefixes array, gridSquareFormat
- **dupeChecking** - overall, perBand, perMode, perBandAndMode, perBandAndGridSquare
- **userPrompts** - optional inputs collected during contest setup (saved to CLX file)

## Multiplier Extraction Logic

1. Check if callsign itself is multiplier (`type == 'callsign'`) - if yes, return callsign
2. Extract from exchange field via `extractMultiplier()`
3. Check against `validation.namedMults` array
4. If not found, check DXCC lookup for non-US/VE stations
5. Handle Alaska/Hawaii special case (`alaskaAndHawaiiCountDxcc=true`)
6. Handle call prefixes for contests with namedCallPrefixes category
7. Skip WAE-only entities (cty.dat `*` prefix) unless `scoring.multipliers.includeWaeEntities=true`

## CLX File Format (JSON-based)

```
metadata:   contestFile, stationClass, version
station:    callsign, name, qth, club
categories: contest-specific metadata including userPromptValues
qsos:       array of QSO records
```

## Automated Testing

- **Runner:** `scripts/run_log_tests.py`
- **Config:** `test_logs/automated_tests.json`
- Validates scores AND multiplier details for all supported contests
- Checks multiplier counts match actual listed multipliers
- Coverage: NAQP CW/SSB/RTTY, ARRL 10M, CWops CWT, YBDX, ARRL VHF

## Test Data Generation

- **Callsign generator:** `scripts/generate_callsigns.py` — always use this script when generating callsigns for test data (do not make up callsigns manually)
- Usage: `python3 scripts/generate_callsigns.py --total 50 --seed 42` (generates a mix of US, Canadian/Mexican, and international callsigns)
- Flags: `-u` US count, `-n` NA count, `-i` international count, `-t` total with default ratios, `--seed` for reproducibility

## Common Pitfalls

- **Call slash notation:** `PJ2/N9OH` = prefix first when operating from another DXCC. `YB1AR/2` portable → counts as YB2
- **Multiplier per-band vs once:** Check `contest.multipliers[].type` field
- **Mode tracking:** SSB/AM/FM all counted as PHONE in score widget (`ssbQsos` variable)
- **Summary sheet multiplier handlers:** All multiplier type handlers (multsPerMode, multsOnce, multsPerBandAndMode) must be present in summary sheet generation - a past commit accidentally removed these causing empty MULTIPLIER DETAILS
- **CW Decoder thread affinity:** `AudioCapture` must be `moveToThread`-ed on the **source** thread (in `CwDecoderWidget::beginDecoding`, where it was constructed) BEFORE `invokeMethod` queues `startCapture` to the worker. Calling `moveToThread` from inside the worker (the destination) is forbidden by Qt and silently breaks Qt's WASAPI plugin on Windows — only ~20–30 ms of audio gets through then `readyRead` stops firing entirely. The failure looks like "decoder is broken" with no error. There's a `Q_ASSERT_X` in `CwDecoderWorker::startCapture` to catch regressions
- **CW Decoder audio format & channel mixdown:** Use `QAudioDevice::preferredFormat()` **unchanged** — do NOT override channel count or sample format. Qt's WASAPI plugin returns `isFormatSupported(monoInt16) == true` but the driver delivers a silent stream; only the device's native format actually flows on Windows. Mono mixdown is per-frame **max-abs** (preserves sign of the channel with larger magnitude) — taking only channel 0 is silent on right-channel-only CODECs, and averaging loses 6 dB on mono-on-stereo signals plus folds in the noise floor of the empty channel
- **Debug log path:** `clx_debug.log` lives at `QStandardPaths::AppLocalDataLocation` + `/clx_debug.log` (Linux `~/.local/share/ContestLogX/`, macOS `~/Library/Application Support/ContestLogX/`, Windows `%LOCALAPPDATA%\ContestLogX\`). `--debug-log <path>` overrides for tests/scripts. Pre-0.7.30 default was a relative path that broke Windows when CLX's cwd was the read-only `C:\Program Files\ContestLogX\` install dir

## Active Technologies
- C++17 + Qt6 (Core, Widgets) — no new dependencies (001-band-map)
- In-memory `QHash<QString, SpotData>` per `BandMapWidget` instance; (001-band-map)
- C++17 + Qt6 (Core, Widgets, Network, Xml) + **NEW: `Qt6::Multimedia`** — a standard Qt6 module, not a third-party library (004-cw-decoder)
- In-memory ring buffer for audio blocks (lock-free SPSC or QMutex-guarded); per-bin `QString` buffers capped at ~10,000 chars (rolling); `QSettings` for decoder runtime state (center Hz / bin count / squelch with auto checkbox / word-gap multiplier — WPM range is hardcoded 5–60); rig config for audio device name, "Mute on PTT" flag, PTT grace-window ms (004-cw-decoder)

## Recent Changes
- 001-band-map: Added C++17 + Qt6 (Core, Widgets) — no new dependencies
