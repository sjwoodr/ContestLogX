# Changelog

All notable changes to ContestLogX are documented in this file.

## [0.6.14]
- Added CHANGELOG.md with CI-driven release notes from changelog entries
- Fixed make version inserting duplicate changelog sections
- Removed unnecessary cty.dat download from macOS CI build
- Removed qtserialport from CI Qt module installs

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
