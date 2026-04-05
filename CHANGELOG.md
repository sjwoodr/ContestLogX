# Changelog

All notable changes to ContestLogX are documented in this file.

## [0.7.8]
- Added validation when activating Run or S&P mode — if required memory roles (CQ/Run Exchange/TU for Run, My Call/S&P Exchange for S&P) are not assigned, an error dialog is shown and the CW or SSB memory editor opens automatically

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
