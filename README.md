# ContestLogX

**Cross-platform amateur radio contest logging software**

[![Build](https://github.com/sjwoodr/ContestLogX/actions/workflows/ci.yml/badge.svg)](https://github.com/sjwoodr/ContestLogX/actions/workflows/ci.yml)

Most contest loggers are Windows-only, closed-source, or locked to a fixed set of contests. ContestLogX runs natively on Linux and macOS with no emulation or workarounds. Every contest is defined in a simple JSON file allowing you to add support for any contest without touching a line of C++.

**[Website](https://contestlogx.com)** | **[Documentation](https://contestlogx.com/docs/)** | **[Screenshots](https://contestlogx.com/screenshots/)** | **[Downloads](https://github.com/sjwoodr/ContestLogX/releases)**

## Features

### Contest Logging
- Dynamic QSO entry panel that adapts to each contest's exchange fields
- Automatic scoring with points and multiplier tracking
- Duplicate detection (per-band, per-mode, or combined)
- Serial number auto-increment
- QSO editing and deletion
- Search and filter in the log

### Rig Control (via flrig)
- Frequency and mode polling from your radio
- QSY and mode changes
- CW keying via CAT with programmable memories (F1-F8)
- SSB voice keyer with programmable memories
- WPM speed control
- QSY Back (Alt+B) — return to the frequency/mode of previous QSOs

### WSJT-X Integration
- Receives QSO Logged messages from WSJT-X via UDP (port 2237)
- Pre-fills call, exchange, RST, and frequency/mode for operator review before logging
- Supports FT8, FT4, JT65, and other digital modes

### DX Cluster
- Telnet connection to DX cluster servers
- Spots table — click a spot to QSY and populate the callsign field
- Console view with raw output and command input
- Propagation data display (SFI/A/K indices)

### Band Map
- Visual frequency display of DX cluster spots
- Color-coded contact status (needed, worked, dupe)
- Zoom control and frequency axis labels
- Click a spot to QSY

### Operating Aids
- Super Check Partial (SCP) for partial callsign lookup
- Multiplier checklist window
- Rate meter and statistics
- Call history for exchange pre-fill
- Run/S&P operating mode switching with memory sequencing

### File Formats
- Native `.clx` format (JSON-based, human-readable)
- ADIF import and export
- Cabrillo export for contest submissions
- CSV export

### Cloud Storage (backup)
- Automatic background backup of your logs to S3-compatible cloud storage
- Supported providers: **FileLu** and **Amazon S3** (Dropbox, Google Drive, and iCloud Drive are listed but not implemented yet)
- Logs always live locally — the local file is the primary copy; every save is mirrored to the cloud in the background, with sync status shown in the status bar
- Open lets you pull a log down from the cloud, choosing where the local copy is saved
- Configured under Preferences -> Cloud Storage (endpoint, region, bucket or `bucket/folder`, access/secret keys, with a Test connection button)
- Built on Qt with AWS Signature V4 signing — no third-party SDK; credentials are stored obfuscated and never sent in plaintext or written to logs

### Supported Contests
| Contest | File |
|---------|------|
| Alabama QSO Party | `alqp.json` |
| ARRL 10 Meter | `arrl_10m.json` |
| ARRL DX | `arrl_dx.json` |
| ARRL Field Day | `field_day.json` |
| ARRL VHF | `arrl_vhf.json` |
| CWops Mini-CWT | `cwops_cwt.json` |
| EU DX Contest | `eudx.json` |
| Florida QSO Party | `fqp.json` |
| General DXCC | `general_dxcc.json` |
| Georgia QSO Party | `gaqp.json` |
| Japan International DX Contest (JIDX) | `jidx.json` |
| Kentucky QSO Party | `kyqp.json` |
| Louisiana QSO Party | `laqp.json` |
| Michigan QSO Party | `miqp.json` |
| Minnesota QSO Party | `mnqp.json` |
| Mississippi QSO Party | `msqp.json` |
| Missouri QSO Party | `moqp.json` |
| New Mexico QSO Party | `nmqp.json` |
| North Dakota QSO Party | `ndqp.json` |
| OK-OM DX Contest | `okomdx.json` |
| North American QSO Party (CW/SSB/RTTY) | `naqp.json` |
| Russian DX Contest | `rdxc.json` |
| SP DX Contest | `spdx.json` |
| Virginia QSO Party | `vaqp.json` |
| West Virginia QSO Party | `wvqp.json` |
| Winter Field Day | `winter_field_day.json` |
| YB DX Contest | `ybdx.json` |

Adding a new contest is as simple as creating a JSON file in the `contests/` directory. See the existing definitions for examples.

## Download

Pre-built packages are available on the [Releases](https://github.com/sjwoodr/ContestLogX/releases) page:

- **Linux**: AppImage (no installation required)
- **macOS**: Application bundle

## Building from Source

### Requirements
- C++17 compiler (GCC or Clang)
- CMake 3.16+
- Qt6 (Core, Widgets, Network, Xml)
- Python 3 (for test scripts)

### Linux (Ubuntu/Debian)
```bash
sudo apt install cmake build-essential qt6-base-dev libqt6xml6-dev

make
```

### macOS
```bash
brew install qt6 cmake
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)  # if Qt6 not found automatically

make
```

### Running
```bash
./clx                                    # run
./clx --debug --log contest.clx          # debug mode with log file
```

### Testing
```bash
make test           # unit tests
make test-logs      # automated contest log validation
```

## Data Files

On first run, ContestLogX will prompt you to download:

- **cty.dat** — DXCC entity database from [Country Files](https://www.country-files.com/) by Jim Reisert AD1C
- **master.scp** — Super Check Partial database from [supercheckpartial.com](https://supercheckpartial.com/) by Bruce Horn WA7BNM

These are stored in the user data directory (`~/.local/share/ContestLogX/` on Linux, `~/Library/Application Support/ContestLogX/` on macOS) and can be refreshed from the application menu.

## Configuration

Settings are stored in `~/.config/ContestLogX/ContestLogX.json` (Linux) or `~/Library/Preferences/ContestLogX/ContestLogX.json` (macOS). This includes station info, rig connection settings, CW/SSB memories, window layout, and debug options.

## License

ContestLogX is released under the **MIT License**.
See the [LICENSE](LICENSE) file for the full license text.

## Acknowledgments

- **[flrig](http://www.w1hkj.com/flrig-help/)** by Dave Freese W1HKJ — rig control abstraction layer used for radio integration
- **[QLog](https://github.com/foldynl/QLog)** by Ladislav Foldyna OK1MLG — open-source amateur radio logging application (GPL v3) that served as UI inspiration and a reference for flrig/CW integration
- **[Country Files](https://www.country-files.com/)** by Jim Reisert AD1C — CTY.DAT DXCC country database
- **[Super Check Partial](https://supercheckpartial.com/)** by Bruce Horn WA7BNM — MASTER.SCP callsign database used for partial callsign lookup during contest logging
- **[WSJT-X](https://wsjt.sourceforge.io/)** by Joe Taylor K1JT et al. — digital mode software; ContestLogX implements the WSJT-X UDP protocol for QSO pre-fill integration
- **[Qt](https://www.qt.io)** — cross-platform application framework, used under the GNU LGPL v3

---

**[contestlogx.com](https://contestlogx.com)** | **ContestLogX v0.9.4** | *Copyright (c) 2025-2026 Steve Woodruff, N9OH*
