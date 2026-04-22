# ContestLogX Developer Notes

**Copyright (c) 2025-2026, by Steve Woodruff, N9OH**

## Running and Debugging

**IMPORTANT:** 
- After rebuilding, you MUST restart the application to pick up changes!
- **Always run the app from the source directory** (not the build directory) so it can find the `contests/` directory:
  ```bash
  cd /home/steve/src/other/ContestLogX
  ./build/ContestLogX
  ```

Debug information is written to:
- `contestlogx_debug.log` in the current working directory (truncated on startup, max 5MB when rotated)
- **Component-based debug logging controls:** Debug menu with granular toggles (persisted in settings)
  - **Flrig Debug Logging** (default: OFF) - flrig polling, XML-RPC messages, rig communication
  - **MainWindow Debug Logging** (default: OFF) - main window operations, UI updates, file operations
  - **ContestEngine Debug Logging** (default: OFF) - contest logic, scoring, validation, multipliers
  - **CWWindow Debug Logging** (default: OFF) - CW keying, WPM changes, memory button operations
  - **DxccDatabase Debug Logging** (default: OFF) - DXCC entity lookups, cty.dat parsing
- All log messages are prefixed with component name (e.g., `[2025-12-15 08:53:47.141] CWWindow: ...`)
- Enable specific components only when debugging that area to reduce log noise
- Debug settings are loaded BEFORE any components initialize, ensuring clean startup logs

## Recent Changes (2026 — 0.7.x)

### CW Decoder ✅

**Feature branch: `004-cw-decoder`** — dockable multi-channel CW decoder, one per radio that has an audio input device configured. See `specs/004-cw-decoder/` for the full SpecKit artifacts (spec, plan, research, data model, contracts, tasks, checklists).

**Architecture**:
- `src/audio/` (new directory) — DSP + capture subsystem. Pure C++ where possible; Qt types only at signal boundaries.
- `include/audio/audioTypes.h` — shared enums, defaults, `blockSamplesForRate()` helper
- `include/audio/morseTable.h` — compile-time Morse lookup (~55 entries, letters/digits/common prosigns)
- `include/audio/spscRingBuffer.h` — lock-free SPSC ring buffer (std::atomic head/tail)
- `include/audio/binChannel.{h,cpp}` — one Goertzel tone detector + dot/dash classifier + rolling-median WPM estimator + Morse decoder per bin
- `include/audio/cwDecoder.{h,cpp}` — owns a vector of `BinChannel` and dispatches audio blocks to each
- `include/audio/audioCapture.{h,cpp}` — `QAudioSource` wrapper; captures at device's native sample rate (no internal resampling — earlier nearest-neighbor decimation aliased above-Nyquist noise into the CW band); accepts int16 / int32 / float / uint8 input, takes channel 0 from stereo
- `include/audio/cwDecoderWorker.{h,cpp}` — `QObject` on a `QThread`; drains the ring buffer, processes blocks via `CwDecoder`, emits `charDecoded` / `wpmUpdated` / `binLayoutChanged` / `muteStateChanged` over queued connections
- `include/cwDecoderWidget.h` + `src/ui/cwDecoderWidget.cpp` — `QDockWidget` with stacked scrolling bin rows, operator-click token fill, adaptive visual highlighting for callsign and RST tokens

**DSP pipeline**:
1. `QAudioSource` → int16 mono at device's native sample rate (typically 44100 or 48000 Hz)
2. SPSC ring buffer (capture-thread producer, worker-thread consumer)
3. 10 ms blocks (e.g., 480 samples at 48 kHz) processed by the decoder worker
4. Per bin: Goertzel single-frequency tone detection → Schmitt trigger with three-way adaptive off-threshold (`max(0.7 × squelch, 1.3 × estimated noise floor, 0.3 × current tone peak)`, capped at `0.9 × squelch`) → dot/dash classifier → rolling-window 25th-percentile dot-length estimator for live WPM → Morse table lookup on character boundary → per-bin text buffer
5. Clickable token detection per row: callsign regex (standard + slash notation) and RST regex (strict `[1-5][1-9N][1-9N]` whitespace-bounded, with N/T cut-number normalization applied on click)
6. Stuck-Schmitt safety release: if the detector has been ON for longer than 6× current dot estimate (= 2× a dash), force-close the element and reset the magnitude smoothing window so the next cycle starts cold

**PTT mute** — two independent paths, both gated by a per-radio "Mute decoder on PTT" setting:
- Rig-backend path: `RigInterface::pttStateChanged(bool)` signal (added to the base class) → `CwDecoderWorker::setPttMute(bool)`
- Internal-send path: `MainWindow::notifyInternalCwSend(side, textChars, sendWpm)` fired from the `CWWindow::aboutToSendCw` signal at every `rigClient->sendCW()` invocation → decoder mutes for estimated send duration + grace window (default 250 ms)

**Rig integration**:
- Adds `Qt6::Multimedia` to CMake's `find_package`
- Adds `pttStateChanged(bool)` signal to `RigInterface` base class; `FlrigClient` / `HamlibClient` / `MockedRigClient` each emit on state transitions
- Audio input device + "Mute on PTT" + "PTT grace ms" are per-radio rig settings, stored alongside backend/host/port in `Settings` and configured in the Rig Connection Settings dialog

**Docking**: widget goes in `Qt::TopDockWidgetArea`; `setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea)` is applied in `setupUi()` and re-applied after every `restoreState()` call so the top dock area is bounded by the right-side docks (doesn't extend over DX Cluster / Band Map / SCP). When floating, `setFixedHeight()` pins vertical size to the natural size-hint (unpins on re-dock), so the operator can grow the window horizontally only.

**Debug logging**: Debug menu → "Enable CW &Decoder Debug Logging". All decoder log sites gated by `DebugLogger::instance().isCwDecoderDebugEnabled()`.

**Known limitations**:
- WPM readout is typically biased ~5-15% low due to 10 ms block-size quantization on the tone-off transition; decoding accuracy is unaffected because the dot:dash ratio is preserved. A 5 ms block size or sub-block transition interpolation would reduce the bias further.
- Off-center signals (signal between bin centers) detect on multiple bins with reduced magnitude; operator should tune the passband/bin count so a bin lands near the target tone frequency for best copy.
- No DSP unit tests yet (`tests/test_goertzel.cpp` / `test_binChannel.cpp` / `test_cwDecoder.cpp` are planned per `specs/004-cw-decoder/tasks.md` but not yet written).

### Remote Dashboard ✅

**TODO roadmap item 3** — embedded HTTP server that serves a read-only dashboard of the current session on the LAN. V1 scope (read-only) shipped in 0.7.25; V2 (minimal rig-control writes) deferred.

**Architecture**:
- `include/net/clxSnapshot.{h,cpp}` — thread-safe point-in-time view (score, recent QSOs, rig L+R, rate, propagation, worked named mults) protected by `QReadWriteLock`. Single writer (`MainWindow`) on score/QSO/rig state changes; multiple readers (HTTP handlers) take a cheap full `Copy` under the read lock and serialize JSON without holding the lock.
- `include/net/httpServer.{h,cpp}` — minimal HTTP/1.1 server on top of `QTcpServer`. Close-after-response (no keep-alive). Route dispatch via method+path exact match in a `QHash<RouteKey, HttpHandler>`. Per-socket read buffer for fragmented requests.
- `resources/dashboard.html` — single-file static dashboard served at `/` (and `/dashboard.html`). Vanilla HTML/CSS/JS — no framework. Polls the 7 `/api/*` endpoints every 5 s in parallel via `Promise.all`. Responsive CSS grid: one column on phones, three on desktop, Score hero + Recent QSOs table span full width. Dark theme matching the app.
- `src/ui/preferencesDialog.cpp` — "Dashboard" tab with enable toggle, port, bind mode (LAN / Localhost / Any), read-only token field with Rotate button, and a read-only "Phone URL" line with Copy-to-Clipboard. Changes apply on OK; MainWindow stops+starts the server.

**Why not `Qt6::HttpServer`**: the module isn't in Qt 6.2 (the Ubuntu 22.04 / AppImage build base). Keeping the portability floor lets the AppImage run on older distros; rolling a small HTTP server on `QTcpServer` is ~300 LOC of straightforward wire-format handling.

**Auth**: single bearer token, auto-generated (UUID-derived) on first enable. Accepted either as `Authorization: Bearer <token>` or `?token=<token>` query param — query form is what makes the URL bookmarkable on a phone home screen. No rate limiting in V1 (LAN-only scope makes it moot).

**Endpoints (V1, all GET, all token-gated)**:
- `GET /api/status` — running, contestName/File, so2rEnabled, startedAt, version
- `GET /api/score` — score snapshot with band/mode breakdown
- `GET /api/rate` — currentHourlyRate (last 10 min × 6), lastHourRate, sessionAverageRate
- `GET /api/qsos?limit=N&offset=N` — newest-first paginated; default limit 20, max 200
- `GET /api/rig` — per-radio freq/mode/band/connected/runSpMode
- `GET /api/mults` — worked named multipliers
- `GET /api/propagation` — NOAA SFI/A/K with fetch timestamp
- `GET /` and `GET /dashboard.html` — static dashboard page from Qt resources

**Snapshot update wiring in MainWindow**:
- `onRecalculateScore` and the three on-load/onLogQso paths each call `updateSnapshotScore()` + `updateSnapshotQsos()` — so the dashboard reflects changes in the same frame the score widget does
- `loadContestDefinition` calls `updateSnapshotStatus()` + `updateSnapshotScore()` + `updateSnapshotQsos()` + `updateSnapshotMults()`
- NOAA propagation fetch callback pushes into the snapshot on completion
- A 2-second `QTimer` refreshes rig freq/mode (via `RigInterface::getFrequency()` / `getMode()`) and rate numbers; these don't have obvious push hooks

**Discovery — QR code in Preferences** (shipped in 0.7.25): `QrCodeWidget` + `third_party/qrcodegen/` (nayuki, MIT, single translation unit ~28KB + header) render the full `http://<lan-ip>:<port>/?token=<t>` URL as a QR. Operator scans with phone camera; URL opens with token pre-filled. Chosen over mDNS because a full responder is ~400 LOC of DNS wire-format handling and Bonjour-for-Windows is a real user-install burden. QR works offline, cross-platform, zero-config.

**V2 rig-control writes (shipped 0.7.25)**:
- `POST /api/rig/qsy {radio, freq_hz, mode}` — set frequency and/or mode on the named radio
- `POST /api/rig/band {radio, band}` — jump to the low edge of a named band (`160m`..`2m`)
- `POST /api/rig/run_mode {radio, mode: Run|S&P|Off}` — toggle operating mode
- Handlers run on the Qt main thread (consistent with UI-driven rig calls); flrig's synchronous XML-RPC can briefly stall the UI on a misbehaving rig but no worse than clicking the same control in-app
- Run-mode skips the modal "missing memory roles" validation that UI buttons do — a phone request shouldn't pop a dialog on the shack PC. F-key sends just silently no-op if roles aren't assigned, same as other headless invocations

**Known limitations**:
- `RigInterface::getFrequency()` on `FlrigClient` is synchronous with a 2-second timeout — if flrig drops, the 2s timer poll could briefly stall the main thread. Not observed in practice; would fix by moving rig polling to a background thread (already the case for `HamlibClient`).
- Dashboard polls; no WebSocket push. Fine for a glance view at 5 s latency; will revisit for Multi-Multi where inter-station push latency matters (TODO item 4).

## Recent Changes (2026 — 0.6.x)

### Virginia QSO Party (VAQP) ✅
- Full VAQP contest module added (`contests/vaqp.json`)
- 95 counties + 38 independent cities as named multipliers (`multsOnce`)
- Mobile station scoring: `/M` and `/R` suffix calls score 3 points
- Bonus station groups: "Virginia is for Lovers" (K4L/K4O/K4V/K4E/K4R/K4S, +50 pts each) and "ARRL Year of the Club" (W4MYA/W4VA/K4LRG/N4FRS/W4OVH/W4NPS/W4RKC/K4XY, +20 pts each)
- `receivedExchangeFilter` restricts out-of-state/DX stations to in-state county/city exchanges only
- `bonusStations` JSON field supports `type`: `bonusOnce`, `bonusPerBand`, `bonusPerMode`, `bonusPerBandAndMode`
- Summary sheet includes BONUS STATION DETAILS section showing worked/missed stations per group

### Florida QSO Party (FQP) ✅
- Full FQP contest module added (`contests/fqp.json`)
- Both in-state (FL) and out-of-state (W/VE) station class support
- `multsPerMode` multiplier type

### Rate Widget ✅
- New dock widget showing QSO rate (QSOs/hour) over recent intervals
- Available from **Window → Rate** menu

### Run / S&P / Off Operating Modes ✅
- Three-state operating mode: **Run**, **S&P**, **Off**
- Buttons in QSO entry bar; also togglable via **Ctrl+M** (configurable)
- Run mode: Enter sequences CQ → Exchange → TU+Log
- S&P mode: Enter sequences My Call → Exchange+Log
- Off mode: Enter logs directly (legacy behaviour)
- Memory roles (`CQ`, `My Call`, `Run Exch`, `S&P Exch`, `TU`) control which memory fires at each step

### Contest / Station Memory Type Toggle ✅
- Both CW and SSB memory editors now support two sets: **Station Memories** (global) and **Contest-Specific Memories**
- Active type is shown as a clickable button in the status bar ("Station Memories" / "Contest Memories")
- **Ctrl+T** keyboard shortcut (configurable) toggles between types instantly
- Selected type persists in the CLX file — restored on log reload
- When contest mode is active, empty contest slots do nothing (no silent fallback to station memories)

### DX Cluster Band Filter ✅
- Band filter dropdown added to the DX Cluster panel, to the right of the Auto-scroll checkbox
- Shows **ALL** plus each band the current contest supports
- Filtering hides/shows spots instantly; new spots arriving while a filter is active are filtered on arrival
- Band list updates automatically when a new contest is loaded

### Keyboard Handling from Floating Dock Windows ✅
- F1–F8 CW/SSB memory keys now work when the QSO Entry dock is floating (undocked)
- All other keyboard shortcuts (Ctrl+W, Ctrl+S, Ctrl+F, Ctrl+M, Ctrl+T, etc.) also work from floating docks
- **Root cause fix:** `QWidget::isAncestorOf()` fails across window boundaries; now uses QObject parent-chain walk instead
- `qApp->installEventFilter(mainWindow)` intercepts key events app-wide; events are routed only when the target is a descendant of MainWindow in the QObject tree

### Escape to Halt CW/SSB Sending ✅
- Pressing **Escape** immediately halts any in-progress CW sending
- Also cancels TTS (SSB) voice keying in progress
- Works from any widget in the main window and from floating docks

### CW Memory SN Options ✅
- CW Memories editor now includes `{SN}` serial number formatting options:
  - **Minimum digits**: 1 (e.g. `7`), 2 (e.g. `07`), or 3 (e.g. `007`)
  - **Cut numbers**: maps `0→T`, `9→N`, `1→A` for traditional CW abbreviations

### Bonus Station Scoring (Contest Engine) ✅
- `bonusStations` array in contest JSON defines station groups with point values
- `type` field: `bonusOnce` | `bonusPerBand` | `bonusPerMode` | `bonusPerBandAndMode`
- Bonus points are auto-calculated by the contest engine (no manual entry required)
- Backward-compatible: old `oneTimeOnly: true/false` boolean maps to `bonusOnce`/`bonusPerBandAndMode`

### Mobile Station Scoring (Contest Engine) ✅
- `mobilePoints` and `mobileSuffixes` JSON fields in `scoring` section
- When a worked station's callsign ends with any listed suffix (e.g. `/M`, `/R`), `mobilePoints` overrides the normal mode/relationship-based point calculation
- Evaluated before mode-based scoring; used in VAQP

## Recent Changes (2025-12-15)

### DXCC Database Integration ✅

**Full DXCC Support Using cty.dat:**
- Integrated comprehensive DXCC database using AD1C's cty.dat format (CTY Version 9)
- New `DxccDatabase` class parses cty.dat using **colon-delimited format** (NOT fixed-width):
  - Field 1: Country Name (delimiter: `:`)
  - Field 2: CQ Zone (delimiter: `:`)
  - Field 3: ITU Zone (delimiter: `:`)
  - Field 4: Continent (2-letter abbreviation, delimiter: `:`)
  - Field 5: Latitude in degrees (+ for North, delimiter: `:`)
  - Field 6: Longitude in degrees (+ for West, delimiter: `:`)
  - Field 7: Local GMT offset (delimiter: `:`)
  - Field 8+: Primary DXCC Prefix and alias prefixes
  - Alias prefixes: comma-separated on same or consecutive lines, terminated with semicolon
  - Special prefix modifiers:
    - `=` prefix: exact match required (e.g., `=4U1VIC`)
    - `(#)` after prefix: override CQ zone
    - `[#]` after prefix: override ITU zone
    - `<#/#>` after prefix: override latitude/longitude
    - `{aa}` after prefix: override continent
    - `~#~` after prefix: override GMT offset
- Handles special prefix modifiers:
  - `=` prefix indicates exact match required
  - `(#)` CQ zone override
  - `[#]` ITU zone override
  - `<#/#>` latitude/longitude override
  - `{aa}` continent override
  - `~#~` GMT offset override
- Automatically loads cty.dat from `~/.local/share/ContestLogX/cty.dat` on startup
- File menu option: "Download DXCC Database (cty.dat)" downloads latest from country-files.com
- Downloaded file is stored in user data directory (AppDataLocation), not project data/ directory
- Provides accurate country, continent, CQ zone, ITU zone, and lat/long data
- Smart callsign matching: checks exact matches first, then longest prefix match

**ITU Region Mapping:**
- ITU Zones (1-75 from cty.dat) are mapped to ITU Regions (1, 2, 3) for contest multipliers
- Standard mapping:
  - Region 1: Zones 18–30, 32–45, 48, 49 (Europe, Africa, Middle East)
  - Region 2: Zones 7–13, 15–17, 31, 46, 47 (North/South America, Greenland, Caribbean)
  - Region 3: Zones 50–75, zone 34 (Asia-Pacific)
- **Special case overrides** for territories politically assigned to different regions:
  - **U.S. Pacific territories (Region 2):**
    - Alaska (KL): ITU zones 1/2/3 → Region 2
    - Hawaii (KH6): ITU zone 61 → Region 2
    - Guam (KH2): ITU zone 64 → Region 2
    - Northern Mariana Islands (KH0): ITU zone 64 → Region 2
    - American Samoa (KH8): ITU zone 62 → Region 2
    - Wake Island (KH9): ITU zone 65 → Region 2
  - **French Caribbean territories (Region 2):**
    - Guadeloupe (FG): ITU zones 11/12 → Region 2
    - Martinique (FM): ITU zones 11/12 → Region 2
    - French Guiana (FY): ITU zones 11/12 → Region 2

**Portable/Temporary Operation:**
- Callsigns with portable operation suffixes are handled specially:
  - Ignorable suffixes: `/M` (mobile), `/P` (portable), `/MM` (maritime mobile), `/AG`, `/AE`
  - These are stripped before DXCC/ITU lookup (the call operates "as home")
  - Geographic suffixes: `/W1-W9`, `/VE1-VE4`, `/KL`, `/KH0-KH9`, `/VY0-VY2`, etc.
  - These ARE used for DXCC/ITU lookup (e.g., N9OH/VE3 operates from Canada as VE3, not K)
  - Special suffixes like `/VP2` are recognized as DXCC prefixes and processed as such
- Debug logging shows portable location extraction and final DXCC/ITU determination

**Enhanced Points Calculation:**
- Points now calculated using actual DXCC data for both stations
- Properly detects same-country, same-continent, different-continent relationships
- Supports contest-specific scoring rules from JSON definitions
- Debug logging shows DXCC lookup results for each QSO
- Format: `My: <country>/<continent>/<dxcc> | Their: <country>/<continent>/<dxcc>`

**Contest Engine Updates:**
- `calculatePoints()` now takes station callsign parameter
- Uses DXCC database to determine geographic relationships
- Supports all scoring rule types:
  - `sameDxccEntity`: Points for contacts within same DXCC entity
  - `differentDxccEntity`: Points for contacts with different DXCC entity
  - `sameCountry`: Points for contacts within same DXCC entity (backward compatibility alias)
  - `differentCountry`: Points for contacts with different DXCC entity (backward compatibility alias)
  - `sameContinent`: Points for same continent (different DXCC entity)
  - `differentContinent`: Points for different continents
- **Configurable precedence**: Contest JSON can specify `scoring.precedence` array to control evaluation order
- Default precedence: `["sameDxccEntity", "sameCountry", "differentDxccEntity", "differentCountry", "sameContinent", "differentContinent"]`
- First matching rule in precedence order is used
- Mode normalization: SSB/USB/LSB/FM → SSB, RTTY/PSK/FT8/FT4 → DIGITAL

**Data Directory Structure:**
- **Static data** (`./data/` in project): Read-only bundled files (default_layout.json)
- **User data** (`~/.local/share/ContestLogX/`): Writable files (cty.dat, master.scp, history.json)
  - Creates directory automatically if it doesn't exist
  - Downloaded databases stored here for per-user access
  - Platform-specific via QStandardPaths::AppDataLocation
- **Configuration** (`~/.config/ContestLogX/`): Settings (ContestLogX.json)
  - Platform-specific via QStandardPaths::ConfigLocation

### Previous Updates: Station Class Persistence & Points Calculation ✅

**Station Class Persistence:**
- Station class now saved in .clx files under `contest.categories.station_class`
- When loading saved log, station class dialog defaults to saved value
- No need to re-select station class when reopening a contest log

### .clx File Format Cleanup ✅

**Removed Duplicate Exchange Fields:**
- Eliminated duplicate `exchange_sent` and `exchange_received` string fields from .clx format
- All exchange data now stored only in structured `exchange_fields` object
- Cleaner file format with no redundancy
- Fixed loading logic to properly restore all exchange fields from `exchange_fields`

**Multiplier Tracking Fix:**
- Fixed multiplier counting to only increment for new/unique multipliers
- Previously was counting every QSO, now correctly tracks unique states/provinces

### Log File Format Standardization ✅

**Standardized QSO Log Columns**

All logs now include these standard columns regardless of contest:
- **DATE**: QSO date in UTC (YYYY-MM-DD format)
- **TIME**: QSO time in UTC (HH:MM:SS format)  
- **CALL**: Station callsign (uppercase)
- **FREQ**: Operating frequency in kHz
- **MODE**: Operating mode (CW, USB, LSB, RTTY, etc.)
- **RSTs**: RST sent (auto-populated: 59 for SSB, 599 for CW/RTTY, +0 for Digi)
- **RSTr**: RST received (from exchange)
- **EXCHs**: Exchange sent (auto-populated from station config or serial)
- **EXCHr**: Exchange received (from QSO entry)
- **Nr**: QSO serial number (renamed from "Serial")
- **Dupe**: Duplicate flag ("Y" if dupe, blank otherwise)
- **M**: Cumulative multiplier count at time of QSO
- **C**: Cumulative DXCC entity count at time of QSO
- **P**: Points for this QSO
- **COMMENT**: Optional comment field

**File Save/Load Improvements:**
- `.clx` extension automatically added if no extension specified
- All fields (RSTs, RSTr, EXCHs, EXCHr, Nr, M, C, P) properly saved and restored
- Serial numbers, multiplier counts, DXCC counts, and points now preserved in .clx files
- QSO model handles both "Serial" and "Nr" column names for compatibility

### Previous: Contest Support - Station Classes ✅

**Added Station Class Selection and Exchange Handling**

1. **Station Class System**
   - Contests can now define multiple station classes (e.g., W/VE/XE vs DX)
   - `StationClassDialog` prompts user to select class when loading contest
   - Station class stored in ContestEngine and saved in .clx file
   - Exchange sent (EXCHs) auto-populated based on station class:
     - W/VE/XE stations send state/province from station setup
     - DX stations send serial number

2. **QSO Entry Field Improvements**
   - Only received exchange fields shown in entry panel (per contest UI definition)
   - RST fields auto-populated: 599 (CW/RTTY), 59 (SSB), +0 (DIGI)
   - All fields force uppercase input
   - Enter key in any field logs the QSO
   - Fields properly cleared after logging with RST defaults restored

3. **Contest Column Headers**
   - Now read directly from contest JSON `ui.logColumns`
   - ARRL 10M: TIME, CALL, FREQ, MODE, RSTs, RSTr, EXCHs, EXCHr, Nr, M, C, COMMENT
   - "Serial" renamed to "Nr" for consistency

4. **Contest Definition Updates**
   - `arrl_10m.json` updated with station class definitions
   - Exchange fields simplified to "RST" and "EXCH"
   - `stationClasses` section defines class-specific exchange behavior

### Previous: QSO Table Column Overhaul ✅

**Newest Addition: Dynamic QSO Table Columns**

1. **QsoRecord Enhanced with Exchange Field Storage**
   - Added individual exchange field storage with `QMap<QString, QString> m_exchangeFields`
   - New accessors: `getRstSent()`, `getRstReceived()`, `getExchangeSent()`, `getExchangeReceived()`
   - New mutators: `setRstSent()`, `setRstReceived()`, `setExchangeSent()`, `setExchangeReceived()`
   - Added multiplier/DXCC tracking: `getMultiplierCount()`, `getDxccCount()`, `setMultiplierCount()`, `setDxccCount()`
   - Comment field support: `getComment()`, `setComment()`
   - Generic exchange field access: `getExchangeField(key)`, `setExchangeField(key, value)`

2. **QsoListModel Refactored for Dynamic Columns**
   - Removed hard-coded `COL_*` enums
   - Column headers now dynamically configured via `setColumnHeaders(QStringList)`
   - Default columns: Time, Call, Frequency, Mode, RSTs, RSTr, EXCHs, EXCHr, Serial, M, C, Comment
   - `data()` method maps column headers to QsoRecord fields dynamically
   - Contest-specific columns automatically added based on exchange fields

3. **MainWindow QSO Logging Updates**
   - `onLogQso()` now populates individual exchange fields in QsoRecord
   - Maps dynamic input fields to proper QsoRecord setters
   - Auto-fills RST sent based on mode (59 for SSB, 599 for CW/RTTY, +0 for digital)
   - Properly stores RSTs, RSTr, EXCHs, EXCHr separately for column display
   - Placeholder multiplier/DXCC counting (TODO: implement proper tracking)

4. **Contest Column Configuration**
   - `updateLogHeaders()` builds column list from contest definition
   - Always includes: Time, Call, Frequency, Mode, RSTs, RSTr
   - Adds exchange fields from contest (avoiding RST duplicates)
   - Adds EXCHs/EXCHr if not present
   - Ends with: Serial, M (multipliers), C (DXCC), Comment
   - Updates model with `m_qsoModel->setColumnHeaders(fullHeaders)`

5. **QSO Entry Field Improvements**
   - Call field limited to 10 characters width
   - All fields enforce UPPERCASE input
   - Tab order: Call → RST fields → Exchange fields → Log QSO button
   - Enter key in any field triggers Log QSO
   - Contest name shown in status bar before QSO count

**Result:** QSO log table now displays proper columns per contest with separate fields for sent/received RST and exchange data!

### Previous: UI Improvements, CW Console, DX Cluster, Station Setup ✅

**Major Additions:**

1. **CW Console Integration**
   - CW window now embedded in main window (lower right panel)
   - Toggleable via Window menu
   - CW memory buttons (F1-F8) with customizable messages
   - Edit CW Memories dialog under Rig menu
   - WPM speed control with up/down buttons
   - CW sending via flrig CAT commands (rig.cwio_text)
   - Halt button to stop CW transmission
   - History display of sent messages
   - CW speed persisted in settings

2. **DX Cluster Panel**
   - Embedded DX cluster in main window (upper right panel)
   - Toggleable via Window menu
   - Two tabs: Spots and Console
   - Spots table with Time, Callsign, Frequency, Mode, Spotter, Comment
   - Click spot to change rig frequency/mode and populate callsign
   - Console tab for raw cluster commands
   - Auto-login using station callsign
   - Periodic WWV propagation updates (every 15 minutes)
   - SFI/A/K values displayed in status bar
   - Connection settings persisted

3. **Station Setup Dialog**
   - New "Station Setup" under File menu
   - Configure: Callsign, Name, Address, Grid Square
   - Settings persisted in JSON config
   - Used for DX cluster login

4. **Contest System**
   - Contest definitions in JSON format (contests/ directory)
   - Sample ARRL 10m contest definition created
   - Contest selection dialog (File -> New Log)
   - Dynamic QSO entry fields based on contest
   - Contest-specific table columns
   - Fixed contest dialog to properly save selected file

5. **UI Enhancements**
   - Window geometry saved/restored on startup
   - Default geometry: 1566x905+370+194
   - Column widths in QSO table persisted
   - Splitter positions saved for main/right panels
   - Status bar shows: QSOs | Rig | WPM | SFI/A/K
   - Improved DX cluster table styling (larger font, better colors)

6. **Settings Migration**
   - Changed from QSettings (INI) to JSON format
   - Settings file: ~/.config/.clx/.clxQt.json
   - Stores: geometry, flrig connection, CW speed, CW memories, station info, DX cluster, column widths

7. **Bug Fixes**
   - Fixed flrig auto-reconnect on startup
   - Fixed CW text encoding issues
   - Fixed CW buffer clearing between messages
   - Stripped leading spaces from DX cluster spot data
   - Fixed Comment field showing extra timestamp

### Previous: Complete Contest Engine Implementation ✅

**FULLY IMPLEMENTED** contest support with scoring, validation, and dupe checking:

1. **ContestEngine Class** (`contestengine.h/cpp`)
   - Core contest logic engine
   - Validation: band/mode/exchange fields per contest rules
   - Dupe checking: configurable scopes (overall, per-band, per-mode, per-band-mode)
   - Scoring: points per mode (CW=2, Phone=1, Digital=2)
   - Multipliers: state/province tracking
   - Total score calculation: QSOs × Multipliers

2. **Dynamic QSO Entry Fields**
   - Exchange fields created dynamically from contest definition
   - Field labels, max length, type from contest metadata
   - Added to QSO entry panel at runtime
   - Generic exchange field hidden when specific fields present

3. **Validation & Dupe Checking**
   - `onLogQso()` now validates before logging
   - Checks band/mode validity for contest
   - Validates exchange format (serial, RST, state/province)
   - Dupe check with warning dialog (can log anyway if desired)
   - Points calculated per QSO

4. **File Format Integration**
   - `saveClxWithContest()` includes contest metadata
   - Contest name/type saved in .clx files
   - Defaults to "General DXCC Logging" if no contest

5. **Documentation**
   - Created `CONTEST_ENGINE_IMPLEMENTATION.md` with full docs
   - API reference, usage examples, contest format spec
   - Example: ARRL 10 Meter contest fully defined

### Contest Selection Dialog Implementation

1. **Contest Selection on New Log**
   - File -> New Log now prompts user to select a contest
   - `ContestSelectDialog` lists all available contests from `contests/` directory
   - Reads contest JSON files and displays contest name with description tooltip
   - Double-click or OK button selects contest and loads its definition
   - Contest definition is stored in `m_contestDefinition` (QJsonObject)
   - Loaded into ContestEngine for validation/scoring

### Contest Definition System & DX Cluster Enhancements

1. **Contest Definition Files (JSON Format)**
   - Created JSON-based contest definition format in `contests/` directory
   - Implemented `arrl_10m.json` as reference example for ARRL 10 Meter Contest
   - Contest files define:
     * Contest metadata (name, sponsor, dates, duration)
     * Valid frequencies, bands, and modes
     * Exchange fields (sent and received)
     * QSO fields displayed in log
     * Scoring rules (points per mode, multipliers, final score calculation)
     * Dupe checking rules (overall, per-band, per-mode, per-band-and-mode)
     * Validation rules for exchanges
     * UI configuration (log columns, entry fields, band map settings)
   - **Exchange Validation Logic**:
     * `type`: "namedMultOrSerial" - indicates stations can send either
     * `multipliers`: Array of all valid state/province codes
     * `serialNumberFormat`: Regex pattern for serial numbers
     * `logic`: Describes acceptance criteria (multiplier OR serial format)
   - Future contests can be added by creating new JSON files following this schema
   - See `contests/arrl_10m.json` for complete reference implementation

2. **DX Spot Click Handling**
   - Clicking any row in DX cluster spot table changes rig frequency and mode
   - Callsign from spot is automatically filled into QSO entry field
   - Callsign is uppercased and field receives focus for immediate data entry
   - Frequency passed to flrig in Hz (converted from kHz)
   - Mode set to the calculated mode from band plan
   - Debug logging added for spot click events

3. **Comment Field Cleanup**
   - Trailing timestamps (format: NNNNZ) stripped from comment field
   - Comments now show cleaner without time suffix
   - Example: "KY 1659Z" becomes "KY"

4. **Table Row Alignment Fix**
   - Fixed padding inconsistency between alternating rows in DX cluster table
   - All cell text trimmed in parsing to remove leading/trailing spaces
   - Dark rows (#1a1a1a) should now align properly with gray rows
   - Simplified stylesheet to use consistent padding without per-row overrides

### Earlier Today: Periodic Propagation Updates

1. **Automatic Propagation Data Refresh**
   - Timer set to request propagation data every 15 minutes
   - Command `sh/wwv` sent to DX cluster automatically every 15 minutes
   - Timer starts after successful login to cluster
   - Timer stops when disconnected from cluster
   - Initial propagation data requested 3 seconds after login
   - Status bar updates with latest SFI, A, and K index values

### Window Menu & Panel Toggles

1. **New Window Menu**
   - Window -> DX Cluster (toggles DX cluster panel visibility)
   - Window -> CW Console (toggles CW console panel visibility)
   - Panel visibility state persisted in settings
   - Menu checkboxes reflect current panel visibility

2. **Band Plan Mode Calculation**
   - New utility class: `BandPlan` in `src/utils/bandplan.h/cpp`
   - Calculates expected mode from frequency based on IARU band plans
   - DX cluster spots now show calculated MODE in the Mode column
   - Covers 160m through 2m bands with CW, SSB (LSB/USB), and RTTY segments
   - Default to USB for unknown frequencies

3. **Status Bar Improvements**
   - Added pipe separators between status elements: ` | `
   - Format: "QSOs: 0 | Rig: Connected | WPM: 28 | SFI 122 A 14 K 1"
   - More readable at-a-glance information

4. **Settings Enhancements**
   - New methods: getDxClusterVisible(), setDxClusterVisible()
   - New methods: getCwConsoleVisible(), setCwConsoleVisible()
   - New methods: getMainSplitterState(), setMainSplitterState()
   - New methods: getRightPanelSplitterState(), setRightPanelSplitterState()
   - Splitter states saved as base64-encoded byte arrays
   - Panel visibility defaults to true (both visible on first run)

5. **Splitter State Persistence**
   - Both main splitter and right panel splitter positions saved
   - Restored on application startup
   - Saved whenever panels are toggled or splitters moved

### Station Setup & Propagation Data

1. **Station Setup Dialog**
   - New menu: File -> Station Setup
   - Configure: Callsign, Operator Name, Grid Square, State/Province
   - Data stored in `~/.config/.clx/.clxQt.json` under "station" key
   - Callsign automatically used for DX cluster login
   - New Settings methods: getGridSquare(), setGridSquare(), getState(), setState()

2. **DX Cluster Auto-Login**
   - If callsign configured in Station Setup, auto-logs into DX cluster
   - No login dialog needed when callsign is set
   - After auto-login, requests propagation data (sh/wm) and spots (sh/dx)
   - 1 second delay before sending callsign, 2 seconds before commands

3. **Propagation Data Display**
   - Parses WWV/WM data from DX cluster output
   - Extracts SFI (Solar Flux Index), A-Index, K-Index
   - Displays in status bar: "SFI 122  A 14  K 1"
   - Updates when connecting to cluster or on periodic updates
   - Signal: propagationDataReceived(int sfi, int aIndex, int kIndex)

4. **DX Cluster Login Flow (fallback)**
   - Auto-sends newline on connect to trigger server response
   - If no callsign configured, shows login dialog
   - Login dialog displays console output and callsign input
   - After login, automatically sends `sh/wm` and `sh/dx` commands
   - Server persisted to settings file

### Major UI Redesign - QLog-style Layout

1. **Embedded DX Cluster Panel**
   - DX cluster support added in upper right panel
   - TCP connection to DX cluster servers (e.g., dxc.nc7j.com:7300)
   - Parses and displays spots in table format
   - Stacked above CW console
   - Two views: Spots (table) and Console (raw text)
   - Can send commands to cluster via command line

2. **Embedded CW Console (Always Visible)**
   - CW console now embedded in lower right (not floating window)
   - Always visible when app is running
   - Contains F1-F8 memory buttons with editable macros
   - CW history, input line, WPM control, Halt/Clear buttons

3. **CW Memory System**
   - F1-F8 buttons for instant CW macro transmission
   - Each button shows F-key number + 5-char abbreviation
   - "Edit CW Memories" dialog under Rig menu
   - Memories stored in settings JSON with abbreviation + full text
   - Click button to immediately send that macro via CW

4. **Splitter Layout**
   - Main window uses horizontal QSplitter
   - Left side (70%): Log table + QSO entry panel
   - Right side (30%): DX Cluster panel + CW Console stacked vertically
   - User can resize splits as needed
   - Split positions persisted to settings

5. **Settings Enhancements**
   - CW memories persisted as JSON array in settings
   - Each memory has "abbreviation" (5 chars) and "text" (full CW)
   - All settings in `~/.config/.clx/.clxQt.json`
   - DX cluster server and callsign persisted

### Previous: Cleanup and Enhanced flrig Support

1. **Removed testCWSend() Self-Test**
   - Self-test function removed entirely from codebase
   - CW testing should be done manually via CW Window
   - Eliminates startup delay and potential buffer confusion

2. **Enhanced flrig Method Support**
   - Added PTT (Push To Talk) control: `getPTT()`, `setPTT(bool)`
   - Added Power level control: `getPower()`, `setPower(int watts)`
   - Added Bandwidth control: `getBandwidth()`, `setBandwidth(int hz)`
   - Added VFO control: `getVFO()`, `setVFO(QString)` - Returns/sets "A" or "B"
   - Based on QLog open source reference implementation
   - All methods include debug logging
   - Complete flrig XML-RPC API now supported for future features

3. **flrig Methods Now Available**
   - Frequency: `getFrequency()`, `setFrequency(double hz)`
   - Mode: `getMode()`, `setMode(QString)`
   - CW: `sendCW(QString)`, `setCWSpeed(int wpm)`, `stopCW()`
   - PTT: `getPTT()`, `setPTT(bool)`
   - Power: `getPower()`, `setPower(int watts)`
   - Bandwidth: `getBandwidth()`, `setBandwidth(int hz)`
   - VFO: `getVFO()`, `setVFO(QString)`
   - Rig info: `getRigName()`

**Status:** flrig support is now feature-complete for common amateur radio operations.

### Earlier: CW Sending Self-Test and Debugging

**CW Sending Debugging Status**:
- Self-test added: Automatically sends "VVV TEST" 3 seconds after startup (if in CW mode)
- XML-RPC communication verified working - sends `rig.cwio_text` with proper parameters
- flrig responds with HTTP 200 (success)
- Debug logs show complete request/response cycle

**Debug Log Analysis**:
```
>>> sendCW ENTRY: text= "VVV TEST" length= 8
>>> sendCW: About to send CW text: "VVV TEST"
>>> sendCW: XML-RPC request: "<?xml version=\"1.0\"?>\r\n<methodCall>\r\n  <methodName>rig.cwio_text</methodName>\r\n  <params>\r\n    <param><value><string>VVV TEST</string></value></param>\r\n  </params>\r\n</methodCall>\r\n"
>>> sendCW: Writing to socket
>>> sendCW: DONE, returning true
```

**Next Steps for User**:
1. Verify if radio actually transmitted CW during self-test
2. If no transmission:
   - Check flrig CW configuration (Config → CW → Text → Enable CAT CW)
   - Verify rig model supports CAT CW keying
   - Check flrig debug log for errors
3. Try manual CW send from CW Window (Rig menu → CW Window)

### UI Layout and Panel Persistence

1. **Main Window Layout**
   - Horizontal splitter divides: Left (QSO log/entry) | Right (DX Cluster + CW)
   - Right side has vertical splitter: Top (DX Cluster panel) | Bottom (CW console)
   - Both splitters are resizable and their sizes persist across app restarts
   - Splitter positions saved to settings JSON file automatically on resize
   - Settings stored in: `~/.config/.clx/.clxQt.json`

2. **DX Cluster Panel** 
   - Tabbed interface with "Spots" and "Console" views
   - Spots table shows: Time, Callsign, Frequency, Mode, Spotter, Comment
   - Console view for raw cluster commands and responses
   - Connection controls: server:port input, Connect button, view selector
   - Minimum height: 200px
   - Proportional sizing with CW panel via vertical splitter

3. **CW Console Panel**
   - Always visible at bottom right of main window
   - Contains: CW history (read-only), input line, F1-F8 memory buttons
   - WPM control, Halt button, Clear button
   - Minimum height: 250px
   - Memory buttons show: F-key label + 5-char abbreviation

### CW WPM Control, Halt Button, and Status Bar Display

1. **CW Speed (WPM) Control**
   - CW Window now includes WPM control with spinbox (5-60 WPM range, default 28)
   - Up/down arrows adjust CW speed
   - WPM setting is applied before each CW send operation
   - Uses `rig.set_cwioWPM` XML-RPC method to set speed on rig
   - Uses `rig.get_cwioWPM` XML-RPC method to get current speed
   - WPM spinbox initialized with current rig speed on window open
   - Real-time updates when spinbox value changes
   - **NOTE:** CW sending speed via CAT may be independent of rig's keyer speed
     - We set the speed before sending, but rig may ignore for CAT CW
     - Focus is on sending CW text at specified WPM via cwio

2. **CW Halt Button**
   - New "Halt" button stops CW sending in progress
   - Sends `^C` (break signal) to abort current CW transmission
   - Clears input buffer
   - Located between WPM control and Clear button

3. **Status Bar Enhancements**
   - Main window status bar now displays: "QSOs: N  Rig: Connected  WPM: 28"
   - WPM value synced between CW window and main window
   - When CW window WPM changes, main window status bar updates immediately
   - Main window WPM initialized from CW window's value when opened
   - WPM label shows "--" when rig disconnected or before CW window opened

4. **CW History Window Fixed**
   - Sent text now properly appends to history pane with timestamp
   - Format: `[HH:mm:ss] text`
   - Input line clears immediately before sending (for better UX)
   - Only appends to history if send was successful
   - Debug logging shows WPM and text for each send
   - History scrolls automatically to show latest sent text

### Earlier: CW Sending via CAT Commands

1. **CW Sending Implementation - Using rig.cwio_text** ✅ WORKING
   - Uses flrig's `rig.cwio_text` XML-RPC method to send CW via CAT
   - Text sent as-is (uppercase converted automatically for readability)
   - NO special bracket formatting needed (previous implementation removed)
   - **WPM speed is LOCAL only** - flrig doesn't support `rig.set_cwioWPM` or `rig.get_cwioWPM`
   - WPM setting stored locally, could be used for character timing in future
   - Uses main socket connection to flrig (no separate socket)
   - Based on QLog open source implementation reference
   - CW text properly sent to rig via CAT commands through flrig
   - Debug logging shows all CW operations
   - Sent text appears in history panel

2. **Column Width Persistence**
   - QSO table column widths now saved when resized
   - Widths stored in `.clxQt.json` settings file
   - Restored on next startup
   - Per-column settings stored as JSON object
   - Uses QHeaderView::sectionResized signal

### Earlier: Debug Logging, Settings Path, CW Window Polish

1. **Unified Debug Logging System** (Updated 2025-12-15)
   - New `DebugLogger` utility class for structured logging
   - All debug output goes to `clx_debug.log`
   - Log file truncated on each startup
   - Auto-truncates when reaching 5MB to prevent excessive disk usage
   - Thread-safe logging with QMutex
   - Custom message handler in main.cpp routes all qDebug() to log file
   - Includes timestamp, component name, and message
   - Available throughout application via `#include "debuglogger.h"`
   
   **Per-Component Debug Controls** (NEW):
   - New "Debug" menu in menu bar with three toggleable options:
     - **Enable Flrig Debug Logging** (default: OFF)
     - **Enable MainWindow Debug Logging** (default: ON)
     - **Enable ContestEngine Debug Logging** (default: ON)
   - Each component's debug state persisted in settings JSON
   - When disabled, that component's log messages are filtered out
   - Status bar shows current toggle state when changed
   
   **Log Message Format**:
   ```
   [2025-12-15 08:45:22.749] Component: Message
   ```
   Where Component is: Flrig, MainWindow, ContestEngine, or INFO

2. **Settings File Path Fixed**
   - Settings now stored at `~/.config/.clx/.clxQt.json`
   - Previously was `~/.config/.clx Contesting Software/.clx Qt/.clxQt.json`
   - JSON format for cross-platform compatibility
   - Settings class singleton pattern
   - Auto-saves on changes

3. **CW Window Implementation**
   - Rig menu → "CW Window" opens CW sending interface
   - Only opens when rig is in CW mode
   - Top panel: Multi-line read-only history (4 lines, 400px wide)
   - Bottom panel: Single-line input (400px wide)
   - Press Enter to send CW text via flrig `rig.cwio_text` with CAT commands
   - Text automatically formatted with brackets for flrig
   - Sent text appended to history panel immediately
   - Clear button clears both panels
   - Window auto-closes if mode changes from CW
   - Status bar shows "CW Window disabled in mode <X>" if not CW
   - CW send uses separate socket to avoid interfering with polling

4. **Window Geometry Persistence**
   - Window size and position saved on exit via Settings
   - Restored on next startup
   - Default geometry: 1566x905+370+194 (if not previously saved)
   - Maximized state also preserved
   - Settings saved in JSON format

5. **UI Polish**
   - Call field: Auto-uppercase on entry
   - Call column in table: Displays uppercase
   - Contest name defaulted to "General DXCC Logging" if unknown
   - Frequency/Mode button positioned left of QSO entry fields
   - Clicking freq/mode button opens manual entry dialog

### Earlier: Frequency/Mode Dialog & File Format Fixes

1. **Frequency/Mode Button Click Behavior Changed**
   - Clicking freq/mode button now opens manual entry dialog (not flrig connect)
   - Dialog allows manual frequency entry in kHz
   - Dialog allows mode selection
   - Changes are sent to rig if connected
   - Frequency properly converted from kHz to Hz for flrig (multiply by 1000)

2. **File Format: .clx Now Default**
   - File->Save now defaults to .clx format (JSON)
   - File->Save As defaults to .clx in file dialog
   - FileHandler properly routes to ClxFile class
   - ADIF and CSV still supported for import/export
   - Old .wl binary format NOT supported (users must export to ADIF first)

3. **Rig Menu Separation**
   - "Rig" menu for flrig connection configuration
   - Freq/Mode button for manual frequency/mode changes
   - Both update rig if connected

### Phase 2 UI Improvements

1. **Removed Frequency Field from Entry Form**
   - Frequency now only shown in freq/mode button (left side of entry panel)
   - Frequency automatically captured from rig when logging QSO
   - Stored in `m_lastFrequency` member variable

2. **Mode Control Restructured**
   - Mode dropdown removed from main window
   - Mode now set via freq/mode dialog
   - Mode changes sent to rig via flrig

3. **flrig Auto-Connect Fixed**
   - Connection state now persisted immediately on successful connect
   - Auto-connect flag saved to settings on connect
   - Auto-connect flag cleared on manual disconnect
   - Settings saved via `Settings::instance().setFlrigAutoConnect()`
   - Auto-reconnect working on startup

4. **UI Layout Improvements**
   - Freq/Mode button on left side of entry panel
   - Entry fields (Call, Exchange, Log button) to the right
   - Table view fills remaining vertical space
   - Entry panel fixed height at bottom (like Windows .clx)

### flrig Integration Status

- Full XML-RPC communication working
- Frequency and mode polling every 500ms (configurable to 2000ms)
- XML parser fixed to handle untyped values (flrig sends `<value>28009720</value>` not `<value><double>28009720</double></value>`)
- Frequency conversion: kHz in UI, Hz for flrig
- Debug logging to `wl_flrig_debug.log` (truncated on startup)
- Auto-reconnect on startup if previously connected

## File Format Decision: .clx JSON Format

### Current Status (2024-12-14)

**DECISION:** .clx Qt will use `.clx` JSON format, NOT binary `.wl` format.

**See:** `WLQT_FORMAT_SPEC.md` for complete specification

## .clx .WL File Format - NOT IMPLEMENTED

The .wl file format support in .clxQt is **STUB CODE ONLY** and does not work.

### Why It Doesn't Work

1. **Complex Binary Format:**
   - Windows-specific binary format
   - Pointers in structures (not portable)
   - Variable-length records
   - OLE objects embedded
   - Network synchronization data

2. **Current Implementation:**
   - Only checks file signature
   - Guesses at structure offsets
   - Cannot parse variable-length sections
   - No understanding of exchange fields
   - No OLE object support

3. **Real Format (from Condefs.h):**
   ```c
   struct qso_stru {
       long freq;              // 10 Hz units
       long rfreq;             // receive freq
       unsigned long serial;
       VarPart_t var_part;     // POINTER - not portable!
       unsigned char band;
       unsigned char DupeSheet;
       char stn[14];           // callsign
       char dupe;
       char parfl;
       char mode;              // '1'=LSB, '2'=USB, '3'=CW
       FILETIME m_time;        // Windows FILETIME
       // ... plus internal fields
   };
   ```

4. **Variable Part:**
   - Contains exchange data
   - Structure varies by contest
   - Length not known in advance
   - Must parse contest-specific format

### Recommendation

**DO NOT use .wl format with .clxQt!**

**Instead:**

1. **Export from Windows .clx:**
   - File → Export → ADIF
   - Use ADIF for interchange

2. **Or use CSV:**
   - .clx can export CSV
   - .clxQt fully supports CSV

3. **Keep Windows .clx for .wl files:**
   - .wl format tied to Windows .clx
   - No spec available for full format
   - Reverse engineering would take weeks

### Future Work

To properly implement .wl format would require:

1. Complete reverse engineering of format
2. Understanding all contest module variations  
3. Portable pointer handling
4. OLE object parsing (Windows-specific)
5. Testing with hundreds of real files
6. Estimated effort: **2-4 weeks full-time**

**Priority:** LOW - ADIF works fine for interchange

### Workaround for Users

If you have .wl files and want to use .clxQt:

1. Open file in Windows .clx
2. File → Export → ADIF
3. Open the .adi file in .clxQt
4. Save as .csv or .adi from .clxQt

**This is the supported workflow.**


## Latest Updates (Phase 2 Continued)

### UI Refinements - CW Console and DX Cluster Integration

**Date**: December 14, 2025

#### CW Console Improvements
1. **Compact Layout**
   - CW history at top (80px height, monospace font)
   - Input line with inline controls (WPM, Halt, Clear)
   - F-key memory buttons moved to bottom in single row
   - Smaller buttons (35px height, 9pt font) for compact display

2. **CW Memories Dialog Redesigned**
   - Fixed size: 700x350px
   - Grid layout with headers: Key | Title | Message
   - Compact single-line entries per F-key
   - Title field: 80px wide, 5 char max
   - Message field: Multi-line text edit, 28px height

#### DX Cluster Panel Enhancements
1. **Header Controls**
   - Title label, server input (200px), Connect button (80px)
   - View toggle: Spots / Console dropdown (100px)
   
2. **Dual View Modes**
   - **Spots View**: Table with Time, Callsign, Frequency, Mode, Spotter, Comment
   - **Console View**: Raw text output in monospace font (9pt)
   - Both views receive cluster data, toggle switches display

3. **Command Interface**
   - Command input line with cluster command prompt
   - "Spot Last QSO" button (120px width)
   - Send command on Enter key

#### Layout Structure
- Main window split: 70% log/entry, 30% right panels
- Right panels stacked vertically:
  - DX Cluster (top, expandable)
  - CW Console (bottom, fixed ~250px)

All styling uses 9-10pt fonts for compact professional appearance matching QLog reference.


### Bug Fixes - December 14, 2025 (Evening)

**Multiplier Tracking Fixed**
- Multipliers are now tracked correctly across QSOs
- The "M" column now only increments when a NEW multiplier is worked
- Previously worked multipliers don't increment the count
- Uses QSet to track unique multipliers worked

**.clx File Extension Auto-append**
- When saving files, if no extension is provided, `.clx` is automatically appended
- Ensures consistency in file naming

**Complete QSO Data in .clx Files**
- Fixed `.clx` file format to include ALL QSO fields:
  - RST Sent (RSTs)
  - RST Received (RSTr)
  - Exchange Sent (EXCHs)
  - Exchange Received (EXCHr)
  - Serial Number (Nr/Serial)
  - Multiplier Count (M)
  - DXCC Count (C)
  - Comment
  - All exchange fields as a JSON object

**Station Class Persistence**
- Station class (W_VE_XE vs DX) is now saved in `.clx` files
- Stored in contest.categories.station_class
- Will be loaded when reopening contest logs

**DATE Column Added**
- QSO log now includes DATE column with UTC date
- Both DATE and TIME use UTC timezone, not local time
- Contest definition updated to include DATE in log columns

