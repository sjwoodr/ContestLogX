# ContestLogX - Project Summary

**Copyright (c) 2025-2026, by Steve Woodruff, N9OH**

## Project Status
[![Build](https://github.com/sjwoodr/ContestLogX/actions/workflows/ci.yml/badge.svg)](https://github.com/sjwoodr/ContestLogX/actions/workflows/ci.yml)

## What is ContestLogX?

ContestLogX is a cross-platform amateur radio contest logging application built entirely from scratch using Qt6. It was initially envisioned as a Qt port of WriteLog but evolved into a completely independent application with its own architecture, file formats, and feature set.

## Development History

### Phase 1: Foundation (Qt Migration)
- Set up Qt6 project structure
- Created basic main window with QSO entry panel
- Implemented QSO log view with table display
- Basic file I/O (CSV export)

### Phase 2: Core Features
- **Contest Engine**: JSON-based contest definitions with flexible exchange fields
- **File Format**: Created .clx (JSON) format for contest logs
- **DXCC Database**: Integrated CTY.DAT for country/continent lookup
- **Scoring**: Points calculation with same-country/continent detection
- **Multiplier Tracking**: Automatic multiplier counting (states/provinces)
- **Duplicate Detection**: Per-band, per-mode, or combined

### Phase 3: Rig Control (flrig Only)
- XML-RPC communication with flrig
- Frequency and mode polling (500ms default, 2000ms optional)
- Radio control: QSY, mode changes
- **CW Keying**: CAT-based CW sending via flrig's `cwio_text`
- WPM speed control
- Connection persistence across sessions

### Phase 4: CW Features
- CW Console window (integrated into main window)
- Real-time CW sending with text history
- 8 programmable CW memory buttons (F1-F8)
- Configurable WPM with up/down arrows
- Halt function to stop CW mid-transmission
- CW speed display in status bar

### Phase 5: DX Cluster Integration
- Telnet connection to DX cluster servers
- **Spots View**: Tabular display of spots with columns:
  - Time, Callsign, Frequency, Mode, Spotter, Comment
- **Console View**: Raw cluster output and command input
- Click spot to QSY radio to frequency/mode
- Automatic login with station callsign
- Propagation data parsing (SFI/A/K from sh/wwv)
- Status bar display of propagation conditions
- Connection persistence

### Phase 6: Station Setup
- Station configuration dialog
- Fields: Callsign, Name, Address, Grid Square
- Used for DX cluster login and exchange defaults
- Persisted in application settings

### Phase 7: UI Enhancements
- Toggleable panels (DX Cluster, CW Console)
- Window geometry persistence
- Column width persistence for QSO log
- Status bar with: Contest | QSOs | Rig Status | WPM | SFI/A/K
- Dark theme styling for DX cluster table

### Phase 8: Debug System
- Module-specific debug logging
- Toggle switches for: flrig, MainWindow, ContestEngine, CWWindow, DxccDatabase
- Debug log file with 5MB rotation
- Default: ContestEngine and DxccDatabase enabled

### Phase 9: Contest Definitions
- JSON-based contest rules
- **ARRL 10 Meter Contest** implemented
- Station class support (W/VE/XE vs. DX)
- Dynamic exchange fields based on station class
- Contest-specific scoring rules

### Phase 10: Independence & Rebranding
- Rebranded to ContestLogX
- Added copyright: "Copyright (c) 2025-2026, by Steve Woodruff, N9OH"

## Current Feature Set

### ✅ Implemented Features

#### Core Logging
- [x] QSO entry panel with dynamic exchange fields
- [x] Real-time frequency/mode from rig (via flrig)
- [x] Automatic UTC date/time stamping
- [x] Uppercase enforcement on callsigns
- [x] RST defaults (59 SSB, 599 CW, +0 Digi)
- [x] Serial number auto-increment
- [x] Tab navigation through fields
- [x] Enter to log QSO

#### Contest Support
- [x] JSON contest definitions
- [x] Dynamic column layout per contest
- [x] Exchange validation
- [x] Points calculation
- [x] Multiplier tracking
- [x] Dupe detection
- [x] Station class support
- [x] ARRL 10 Meter Contest implemented

#### Rig Control (via flrig)
- [x] Frequency polling
- [x] Mode polling
- [x] QSY via spot click or manual entry
- [x] Mode change dialog
- [x] CW keying via CAT (cwio_text)
- [x] WPM speed control
- [x] Connection status in UI

#### CW Features
- [x] CW console with send/receive
- [x] 8 memory buttons (F1-F8)
- [x] CW memory editor dialog
- [x] WPM display and control
- [x] Halt function
- [x] Text history display

#### DX Cluster
- [x] Telnet connection
- [x] Spots table with clickable entries
- [x] Console view with command input
- [x] Auto-login with callsign
- [x] Propagation data (SFI/A/K)
- [x] Periodic sh/wwv updates (15 min)
- [x] Click spot to QSY + populate callsign

#### DXCC Database
- [x] CTY.DAT parser
- [x] Country lookup by callsign
- [x] Continent detection
- [x] Same-country/continent logic
- [x] Menu option to download latest CTY.DAT
- [x] Hot-reload after download

#### File Formats
- [x] .clx (JSON) save/load
- [x] Contest metadata in log file
- [x] Station class persistence
- [x] ADIF export
- [x] CSV export

#### UI/UX
- [x] Professional Qt interface
- [x] Toggleable panels (CW, DX Cluster)
- [x] Window geometry persistence
- [x] Column width persistence
- [x] Status bar with multi-info display
- [x] Dark theme for tables
- [x] Menu system (File, Rig, Window, Help, Debug)

#### Configuration
- [x] Station setup dialog
- [x] Rig connection settings
- [x] CW memory editor
- [x] Debug logging toggles
- [x] JSON settings file

### ❌ Not Implemented (Future)

- [ ] Band map
- [ ] Multiplier checklist window
- [ ] Rate meter / statistics
- [ ] Partial callsign checking (Super Check Partial)
- [ ] Network/multi-op support
- [ ] RTTY/digital mode integration
- [ ] Voice keyer
- [ ] Additional contest modules
- [ ] Log comparison/checking
- [ ] Cabrillo export
- [ ] ADIF export
- [ ] QSO editing/deletion
- [ ] Search/filter in log
- [ ] Log backup/archive
- [ ] Awards tracking (DXCC, WAS, etc.)

## Technical Architecture

### Technology Stack
- **Language**: C++17
- **Framework**: Qt6 (Core, Widgets, Network, SerialPort, Xml)
- **Build System**: CMake 3.16+
- **Platform**: Linux (primary), macOS/Windows (future)

### Key Components

1. **MainWindow** - Primary UI container
2. **ContestEngine** - Contest rules, scoring, validation
3. **QsoRecord** - Individual QSO data model
4. **QsoListModel** - Table model for log display
5. **FileHandler** - File I/O coordinator
6. **ClxFile** - .clx format parser/writer
7. **FlrigClient** - XML-RPC communication with flrig
8. **DxccDatabase** - CTY.DAT parser and lookup
9. **CWWindow** - CW console widget
10. **DxClusterPanel** - DX cluster integration
11. **DebugLogger** - Module-based logging system

### Design Principles

- **Contest Definition as Data**: JSON files define contest rules, not code
- **Modular Architecture**: Clear separation of UI, logic, and I/O
- **Cross-Platform**: Qt-native UI, no platform-specific code
- **flrig-First**: No direct rig control, rely on flrig for abstraction
- **Open File Format**: Human-readable JSON for logs
- **Extensible**: Easy to add new contests, features

## File Format: .clx

ContestLogX uses a JSON-based log format (.clx) that includes:

```json
{
  "contest_info": {
    "name": "ARRL 10 Meter Contest",
    "contest_id": "arrl_10m",
    "station_class": "W_VE_XE",
    "operators": ["N9OH"],
    "start_time": "2025-12-14T00:00:00Z"
  },
  "station_info": {
    "callsign": "N9OH",
    "name": "Steve Woodruff",
    "grid_square": "EL96",
    "qth": "FL"
  },
  "qsos": [
    {
      "timestamp": "2025-12-14T15:30:00Z",
      "callsign": "W4WOD",
      "frequency": 28500.0,
      "mode": "CW",
      "rst_sent": "599",
      "rst_received": "599",
      "exchange_sent": "FL",
      "exchange_received": "NC",
      "serial": 1,
      "points": 2,
      "multiplier": "NC",
      "dupe": false,
      "exchange_fields": {
        "RSTs": "599",
        "RSTr": "599",
        "EXCHs": "FL",
        "EXCHr": "NC"
      }
    }
  ],
  "score": {
    "qso_count": 1,
    "points": 2,
    "multipliers": 1,
    "total_score": 2
  }
}
```

## Contest Definition Format

Contests are defined in JSON files in the `contests/` directory:

```json
{
  "name": "ARRL 10 Meter Contest",
  "contest_id": "arrl_10m",
  "modes": ["CW", "SSB", "FM"],
  "bands": [{"name": "10m", "min": 28000, "max": 29700}],
  "exchange_fields": [
    {"name": "RST", "type": "string", "required": true},
    {"name": "EXCH", "type": "string", "required": true}
  ],
  "station_classes": [
    {
      "id": "W_VE_XE",
      "name": "W/VE/XE Stations",
      "exchange_sent_format": "{qth}",
      "exchange_received_format": "{qth_or_serial}"
    },
    {
      "id": "DX",
      "name": "DX Stations",
      "exchange_sent_format": "{serial}",
      "exchange_received_format": "{qth}"
    }
  ],
  "scoring": {
    "cw": {
      "same_country": 2,
      "different_country_same_continent": 4,
      "different_continent": 4
    },
    "ssb": {
      "same_country": 1,
      "different_country_same_continent": 2,
      "different_continent": 2
    }
  },
  "multipliers": {
    "type": "states_provinces",
    "list": ["AL", "AK", ..., "AB", "BC", ...]
  },
  "dupe_checking": "per_band"
}
```

## Integration Points

### flrig (Required)
- **Purpose**: Rig control abstraction layer
- **Protocol**: XML-RPC over TCP
- **Default**: localhost:7362
- **Methods used**:
  - `rig.get_vfo` - Get frequency
  - `rig.set_vfo` - Set frequency  
  - `rig.get_mode` - Get mode
  - `rig.set_mode` - Set mode
  - `rig.cwio_text` - Send CW via CAT
  - `rig.cwio_set_wpm` - Set CW speed

### DX Cluster (Optional)
- **Protocol**: Telnet
- **Commands**:
  - Auto-login with callsign
  - `sh/dx` - Get spots
  - `sh/wwv` - Get propagation data

### CTY.DAT (Optional)
- **Source**: http://www.country-files.com/cty/cty.dat
- **Purpose**: DXCC entity database
- **Format**: CT contest software format
- **Updates**: Menu option to download latest
- **Storage**: `~/.local/share/ContestLogX/cty.dat` (downloaded automatically to user data directory)

## Data Storage Locations

ContestLogX uses platform-specific directories for different types of data:

### User Data Directory
**Location:**
- Linux: `~/.local/share/ContestLogX/`
- macOS: `~/Library/Application Support/ContestLogX/`
- Windows: `%APPDATA%\ContestLogX\`

**Contents:**
- `cty.dat` - DXCC database (downloaded)
- `master.scp` - Super Check Partial database (downloaded)
- `history.json` - Call history records (auto-generated)

### Configuration Directory
**Location:**
- Linux: `~/.config/ContestLogX/`
- macOS: `~/Library/Preferences/ContestLogX/`
- Windows: `%APPDATA%\ContestLogX\`

**Contents:**
- `ContestLogX.json` - Application settings

### Static Bundled Data
**Location:** `./data/` and `./contests/` in the project directory

**Contents:**
- `./data/default_layout.json` - Default window layout
- `./contests/*.json` - Contest definitions (ARRL 10m, CWops CWT, NAQP, etc.)

These are read-only files that ship with the application.

Example `ContestLogX.json`:
```json
{
  "station": {
    "callsign": "N9OH",
    "name": "Steve Woodruff",
    "grid_square": "EL96",
    "qth": "FL"
  },
  "flrig": {
    "host": "localhost",
    "port": 7362,
    "auto_connect": true
  },
  "dx_cluster": {
    "host": "k4zs.no-ip.org",
    "port": 7300
  },
  "cw": {
    "wpm": 28,
    "memories": {
      "f1": {"title": "CQ", "text": "CQ TEST N9OH N9OH"},
      "f2": {"title": "ANS", "text": "TU 5NN FL"},
      ...
    }
  },
  "window_geometry": "...",
  "qso_log_column_widths": [...],
  "debug": {
    "flrig": false,
    "main_window": false,
    "contest_engine": true,
    "cw_window": false,
    "dxcc_database": true
  }
}
```

## Building ContestLogX

### Requirements
- Qt6 development libraries
- C++17 compiler (GCC, Clang, MSVC)
- CMake 3.16 or later

### Linux Build
```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install qt6-base-dev qt6-serialport-dev cmake build-essential

# Build
cd ContestLogX
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Running
```bash
# From source directory (so it finds contests/)
cd /home/steve/src/other/ContestLogX
build/ContestLogX

# Or specify data path
# TODO: this may not work yet
CONTESTLOGX_DATA_PATH=/path/to/data build/ContestLogX
```

## Known Issues / TODO

1. **DX Cluster Table**: Black rows have slight alignment issue (minor padding)
2. **ADIF Import**: Not fully tested with all field types
3. **Contest Modules**: Only ARRL 10m implemented so far and incomplete
4. **QSO Editing**: Cannot edit/delete logged QSOs yet
5. **Cabrillo Export**: Not implemented
6. **Multi-op**: No network support for multi-operator logging

## Future Development

### Near Term
- Add more contest modules (CQ WW, ARRL DX, etc.)
- QSO edit/delete functionality
- Cabrillo export
- Better dupe highlighting in log
- Partial callsign checking (SCP)

### Medium Term
- Band map window
- Multiplier status window
- Rate meter and statistics
- Log comparison/checking tools
- Additional file format support

### Long Term
- Network/multi-op support
- RTTY/digital mode integration
- Voice keyer
- Awards tracking
- Mobile app companion

## License

**To be determined**. Options include:
- GPL v3 (strong copyleft)
- LGPL v3 (library copyleft)
- MIT (permissive)
- Apache 2.0 (permissive with patent grant)

## Contact

Steve Woodruff, N9OH  
Email: steve@n9oh.com
Website: contestlogx.com (to be created)

## Acknowledgments

- **flrig** by Dave W1HKJ - Rig control abstraction
- **QLog** by Ladislav Foldyna OK1MLG - UI inspiration and flrig integration examples
- **Country Files** by Jim Reisert AD1C - CTY.DAT database
- **Qt Project** - Cross-platform framework

---

**ContestLogX v0.2.0**  
*Amateur Radio Contest Logging Software*  
Copyright (c) 2025-2026, by Steve Woodruff, N9OH

Last Updated: December 15, 2025
