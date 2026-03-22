# Changelog

All notable changes to ContestLogX are documented in this file.

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
