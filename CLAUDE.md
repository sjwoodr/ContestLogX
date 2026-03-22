# ContestLogX

A C++ Qt6-based amateur radio contest logging application.

- **Language:** C++17
- **Framework:** Qt6
- **Build system:** CMake
- **Version:** 0.0.9

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
| `src/`       | Source code organized by module (`ui/`, `core/`, `database/`, `engine/`) |
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

## Active Technologies
- C++17 + Qt6 (Core, Widgets) — no new dependencies (001-band-map)
- In-memory `QHash<QString, SpotData>` per `BandMapWidget` instance; (001-band-map)

## Recent Changes
- 001-band-map: Added C++17 + Qt6 (Core, Widgets) — no new dependencies
