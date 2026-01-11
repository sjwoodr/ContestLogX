# Static Data Directory

This directory contains **read-only bundled files** that ship with ContestLogX.

## Files in this directory:
- `default_layout.json` - Default window layout for first run

## User-writable data files are stored elsewhere:

### Downloaded databases (user-writable):
- `cty.dat` - DXCC database
- `master.scp` - Super Check Partial database

**Location:**
- Linux: `~/.local/share/ContestLogX/`
- macOS: `~/Library/Application Support/ContestLogX/`
- Windows: `%APPDATA%\ContestLogX\`

**How to obtain:**
- `cty.dat`: File → Download DXCC Database
- `master.scp`: File → Download Super Check Partial (master.scp)

### User-generated data:
- `history.json` - Call history records

**Same location as above** (`~/.local/share/ContestLogX/`)

### Configuration files:
- `ContestLogX.json` - Application settings

**Location:**
- Linux: `~/.config/ContestLogX/`
- macOS: `~/Library/Preferences/ContestLogX/`
- Windows: `%APPDATA%\ContestLogX\`

## Contest Definitions

Contest definitions are in `../contests/` directory (static, read-only).
