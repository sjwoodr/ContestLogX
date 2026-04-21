# ContestLogX Advanced Features Roadmap

**Visual Band Map, SO2R, Networked Multi-Op, and CW Decoder support for ContestLogX.**

This document defines the specification roadmap for the next major feature cycle.
Each specification is executed end-to-end through the SpecKit workflow
(specify → clarify → plan → checklist → tasks → analyze → implement) before
moving to the next.

**Branch:** `sw/0.7.x` (feature branches per spec)
**Tracker:** GitHub Issues

---

## Table of Contents

1. [Roadmap Overview](#roadmap-overview)
2. [Dependency Graph](#dependency-graph)
3. [Progress Tracking](#progress-tracking)
4. [Specification Sections](#specification-sections)
5. [Environment & Infrastructure](#environment--infrastructure)
6. [References](#references)

---

## Roadmap Overview

The feature cycle is decomposed into **7 specifications** across **4 dependency tiers**. Two specs (SPEC-006, SPEC-007) were added after the original plan was drafted to capture work that emerged during the 0.7.x series.

| Tier | Specs | Purpose | Parallelization |
|------|-------|---------|-----------------|
| **1** | SPEC-001 | Visual Band Map — standalone DX spot visualization | ✅ Shipped |
| **2** | SPEC-002 | SO2R Support — second radio rig control and focus switching | ✅ Shipped |
| **2** | SPEC-005 | CW Decoder — audio-based Morse decode with QSO entry integration | Independent; can run any time |
| **2** | SPEC-006 | Online Score Publishing — real-time posting to contestonlinescore.com | Independent; tasks ready |
| **2** | SPEC-007 | Unify Station Config — migrate legacy stationClasses to userPrompts | Independent; spec draft only |
| **3** | SPEC-003 | Network Foundation — internet-capable log sync across stations | Sequential after SPEC-002 |
| **4** | SPEC-004 | Multi-Op Coordination — serial pool, band lockouts, station roles | Requires SPEC-003 |

**Current Execution Focus:** SPEC-005 (CW Decoder) and SPEC-006 (Online Scoring) are the next targets. SPEC-003 → SPEC-004 remain the path to networked multi-op.

**Dependency Constraints:**
- ✅ SPEC-001 had no dependencies — shipped as Visual Band Map (see `specs/001-band-map/`)
- ✅ SPEC-002 shipped in v0.7.2 as full SO2R support (Radio L/R, backtick toggle, per-radio Run/S&P, mocked backend) — implemented pre-SDD, no `specs/` folder
- SPEC-003 has no dependency on SPEC-001 or SPEC-002, but is ordered after them so
  the radio control architecture is stable before networking is layered on
- SPEC-004 requires SPEC-003 (needs the network layer and QSO sync infrastructure)
- SPEC-005 has no dependencies on any other spec — truly independent audio subsystem
- SPEC-006 has no dependencies — HTTP POST integration against an external service
- SPEC-007 has no hard dependencies but touches contest JSON loading and setup UI — any station-class contest work should land after it if feasible

---

## Dependency Graph

```text
SPEC-001 (Visual Band Map)  ✅ SHIPPED
    │
    │  [band map enriched with 2nd radio in SPEC-002]
    ▼
SPEC-002 (SO2R Support)  ✅ SHIPPED v0.7.2
    │
    │  [stable radio control before networking]
    ▼
SPEC-003 (Network Foundation)        ┌─ SPEC-005 (CW Decoder)          ─ independent
    │                                ├─ SPEC-006 (Online Scoring)      ─ independent
    └──► SPEC-004 (Multi-Op)         └─ SPEC-007 (Unify Station Config)─ independent
              │
         ─── FEATURE CYCLE COMPLETE ───
```

---

## Progress Tracking

| Spec | Name | Status | Spec Folder / Workflow | Next Phase |
|------|------|--------|------------------------|------------|
| SPEC-001 | Visual Band Map | ✅ Complete | [`specs/001-band-map/`](../../../specs/001-band-map/) · [SPEC-001-workflow.md](SPEC-001-workflow.md) | Shipped |
| SPEC-002 | SO2R Support | ✅ Complete | Shipped in v0.7.2 (pre-SDD, no `specs/` folder) | Shipped |
| SPEC-005 | CW Decoder | ⏳ Pending | [SPEC-005-workflow.md](SPEC-005-workflow.md) | Specify (independent) |
| SPEC-006 | Online Score Publishing | 🔄 Tasks Ready | [`specs/003-online-scoring/`](../../../specs/003-online-scoring/) | Implement |
| SPEC-007 | Unify Station Config | 🔄 Spec Draft | [`specs/002-unify-station-config/`](../../../specs/002-unify-station-config/) | Plan |
| SPEC-003 | Network Foundation | ⏳ Pending | [SPEC-003-workflow.md](SPEC-003-workflow.md) | Specify |
| SPEC-004 | Multi-Op Coordination | ⏳ Pending | [SPEC-004-workflow.md](SPEC-004-workflow.md) | Blocked by SPEC-003 |

**Status Legend:** ⏳ Pending | 🔄 In Progress | ✅ Complete | ⚠️ Blocked

> **Numbering note:** Master-plan `SPEC-NNN` IDs are the stable roadmap identifiers. The `specs/NNN-name/` directories use SpecKit's default sequential naming and were numbered as work was started — so `specs/001-band-map/` → SPEC-001, `specs/002-unify-station-config/` → SPEC-007, `specs/003-online-scoring/` → SPEC-006. The two numbering schemes are not expected to align going forward.

---

## Specification Sections

### SPEC-001: Visual Band Map ✅ COMPLETE

**Priority:** P1 | **Depends On:** None | **Enables:** SPEC-002 (band map shows both radios) | **Status:** Shipped — `specs/001-band-map/`

**Goal:** Add a dockable Band Map widget that displays DX cluster spots on a
frequency-axis view for the operator's current band, with click-to-QSY.

**Scope:**
- New `BandMapWidget` QDockWidget showing a frequency axis spanning the current
  contest band (e.g., 14.000–14.350 MHz for 20m), populated from the existing
  DX cluster spot stream
- Spots rendered as labeled markers on the frequency axis: callsign + mode indicator
- Color coding by status: new multiplier (distinct color), already-worked (muted),
  unworked non-mult (default)
- Click on a spot to QSY the active radio (calls the existing flrig QSY path)
- Widget follows the active radio's current band automatically; re-renders when
  band changes
- Real-time updates: new spots arriving from the DX cluster panel appear on the
  map immediately; spots older than a configurable age (default: 30 min) expire
- Zoom/pan: operator can expand or compress the frequency range shown; defaults
  to the full contest band segment
- Widget added to Window menu alongside existing dock panels; state persisted in
  QMainWindow saveState/restoreState

**Out of Scope:**
- Showing both radios' positions on the map (handled by SPEC-002)
- Networked spot sharing (all spots come from the local DX cluster connection)
- Waterfall or SDR integration — frequency-axis spot map only

**Key Decisions:**
**Spot Source Decision (2026-03-21):** Band map consumes spots from the existing
`DxClusterPanel` via a signal rather than maintaining its own cluster connection.
This avoids a second telnet session and keeps spot state in one place.

**Key Files:**
- `src/ui/bandMapWidget.cpp` / `include/bandMapWidget.h` — new widget
- `src/ui/dxClusterPanel.cpp` / `.h` — add `spotReceived(SpotData)` signal
- `src/ui/mainWindow.cpp` — add dock, connect signals, QSY routing
- `src/ui/mainWindow.h` — `m_bandMapWidget` member

---

### SPEC-002: SO2R Support ✅ COMPLETE

**Priority:** P1 | **Depends On:** None | **Enables:** SPEC-003 | **Status:** Shipped in v0.7.2 (implemented pre-SDD, no `specs/` folder)

**Shipped behavior:** Dual independent QSO entry dock widgets (Radio L / Radio R), independent rig backends/host/port per radio, backtick (`) keyboard toggle with visual indicator on active panel, independent Run/S&P/Off per radio, tabbed Rig Connection Settings dialog, mocked rig backend for practice, DX cluster spot and CW/SSB keying routed to active radio.

**Goal:** Allow a single operator to control two independent radios via two separate
flrig instances, with keyboard-driven focus switching and all QSO entry, CW/SSB
keying, and scoring routed to the active radio.

**Scope:**
- Support two independent flrig XML-RPC connections (Radio 1 and Radio 2), each
  with its own host/port configuration in Preferences
- Radio focus switching: dedicated keyboard shortcut (e.g., backtick or configurable)
  toggles the "active radio"; a visual indicator (colored border on QSO entry, or
  R1/R2 badge in status bar) shows which radio is active
- QSO entry panel reflects the active radio's current frequency and mode (polled
  from its flrig instance); logging writes the correct band/mode to the QSO record
- CW keying (`cwio_text`) and SSB TTS both route to the active radio's flrig instance
- Run/S&P mode tracked independently per radio; switching focus preserves each
  radio's mode state
- Band map (from SPEC-001) extended to show both radios' current frequencies as
  distinct markers (e.g., R1 in green, R2 in orange)
- SO2R can be enabled/disabled in Preferences; when disabled, app behaves exactly
  as today (single-radio mode) — no regression for non-SO2R operators
- Radio 2 configuration and SO2R enable/disable persisted in application settings

**Out of Scope:**
- Automatic band switching or lockout between the two radios (SPEC-004 scope —
  that is for networked multi-op, but a future enhancement could apply it locally)
- Audio routing / headphone switching — hardware concern, out of software scope
- Support for more than two radios

**Key Decisions:**
**Rig Control Architecture Decision (2026-03-21):** Each radio is a separate flrig
instance (separate XML-RPC endpoint). This is the only viable path since ContestLogX
delegates all CAT control to flrig. A `RigController` class will be extracted from
the current monolithic flrig polling code to support N instances cleanly.
Alternatives considered: single flrig instance with VFO A/B split — rejected because
flrig's VFO model doesn't map cleanly to independent-radio SO2R operation.

**Key Files:**
- `src/core/rigController.cpp` / `include/rigController.h` — extract from
  `mainWindow.cpp`; supports multiple instances
- `src/ui/so2rFocusWidget.cpp` / `include/so2rFocusWidget.h` — R1/R2 indicator
- `src/ui/mainWindow.cpp` — SO2R enable, focus switching, dual polling, keying
  routing
- `src/ui/preferencesDialog.cpp` — Radio 2 host/port/enable settings
- `include/mainWindow.h` — second `RigController*` member

---

### SPEC-003: Network Foundation

**Priority:** P1 | **Depends On:** None (ordered after SPEC-002) | **Enables:** SPEC-004

**Goal:** Implement an internet-capable TCP client-server network layer that
synchronizes QSOs, dupe state, and scores across all stations in a multi-op setup
in real time, with connection management and reconnection handling.

**Scope:**
- TCP-based client-server architecture: one station acts as server (configurable),
  others connect as clients; all stations can initiate connections (server listens
  on a configurable port)
- Internet-capable: must work across VLANs and NAT; no assumption of LAN multicast
  or shared subnet — pure TCP connections to a known host:port
- QSO sync: when any station logs a QSO, it is broadcast to all connected stations
  and appended to their local log; the log view updates live
- Shared dupe detection: dupe state is derived from the merged log across all
  stations; incoming remote QSOs trigger dupe recalculation
- Score sync: score totals (QSOs, points, multipliers) are broadcast and displayed
  in each station's score widget, broken down by station
- Station identity: each connection identifies itself by station callsign (from
  station config); shown in a network status panel listing connected stations
- Connection management: manual connect/disconnect via Network menu; reconnection
  attempted automatically on drop with exponential backoff; network status shown in
  status bar (connected/disconnected/N stations)
- Message protocol: JSON over TCP with length-prefix framing; messages include
  type (QSO, SCORE, DUPE, PING, STATION_ID), station ID, timestamp, and payload
- All network activity optional: if no network is configured, app behaves exactly
  as today

**Out of Scope:**
- Serial number pool coordination (SPEC-004)
- Band lockout enforcement (SPEC-004)
- Encryption / authentication beyond station callsign identification — TLS and
  auth tokens are a future hardening step; for now, assume trusted network
- Log merging/conflict resolution for pre-existing logs — network sync applies only
  to QSOs logged after connection is established

**Key Decisions:**
**Protocol Decision (2026-03-21):** JSON over TCP with length-prefix framing (4-byte
big-endian message length header). Chosen over UDP for reliability (QSO records must
not be lost), over WebSocket for simplicity (no HTTP layer needed for a desktop app),
and over a binary protocol for debuggability.

**Topology Decision (2026-03-21):** One designated server station, N client stations.
Server is the source of truth for ordering. Any station can be the server; it is a
configuration choice, not a hardware distinction.

**Key Files:**
- `src/network/networkServer.cpp` / `include/network/networkServer.h`
- `src/network/networkClient.cpp` / `include/network/networkClient.h`
- `src/network/networkProtocol.h` — message struct definitions, JSON serialization
- `src/ui/networkStatusPanel.cpp` / `include/networkStatusPanel.h` — connected
  stations list
- `src/ui/mainWindow.cpp` — network enable, QSO broadcast hooks, score sync
- `CMakeLists.txt` — Qt6::Network already a dependency; verify linkage

---

### SPEC-004: Multi-Op Coordination

**Priority:** P1 | **Depends On:** SPEC-003 | **Enables:** Complete multi-op feature

**Goal:** Layer multi-operator contest coordination on top of the network foundation:
a shared serial number pool with atomic distributed allocation, band lockout
enforcement to prevent two stations transmitting on the same band simultaneously,
and per-station operator identification in the log.

**Scope:**
- **Shared serial number pool:** Serial numbers are allocated by the server station
  and distributed to clients on request; each station requests the next SN before
  logging a QSO; server guarantees uniqueness and monotonic ordering; SNs are never
  reused even if a QSO is deleted
- **Band lockout:** When a station begins transmitting on a band (indicated by
  moving to a frequency or by explicit "I'm on 20m" signal), the server broadcasts
  a band-claim to all stations; other stations receive a visual warning if they
  attempt to operate on a claimed band; lockout enforcement is advisory (warning)
  not hard-blocking by default, with a configurable strict mode
- **Operator identification:** Each QSO record includes the logging station's
  callsign (or configurable operator handle); visible in the log view as an
  additional column when networked; exported in CLX file under each QSO
- **Station roles (informational):** Each station can declare a role (Run, Mult,
  Rover, etc.) visible to all connected stations in the network status panel; roles
  are advisory, not enforced by the software
- **Score breakdown:** Score widget extended to show a per-station QSO/points/mult
  breakdown alongside the combined total

**Out of Scope:**
- Automated radio frequency control based on band lockout (operator still manually
  QSYs; software only warns)
- Internet relay / cloud server hosting — the server role must be run by one of the
  contest stations on a reachable IP

**Key Decisions:**
**Serial Allocation Decision (2026-03-21):** Server is the sole allocator of serial
numbers. Clients request the next SN via a `SN_REQUEST` message; server responds with
`SN_GRANT`. If the client is disconnected, it may use a locally cached "hold" SN and
reconcile on reconnect. This avoids gaps in serial sequences under brief disconnects.
Alternatives considered: pre-allocated SN ranges per station — rejected because it
produces non-monotonic serials in the final log.

**Band Lockout Decision (2026-03-21):** Advisory (warning) by default; strict
(block logging) configurable. Strict mode is off by default because contest rules
and operator etiquette vary; a warning is sufficient for most use cases and avoids
frustrating operators during fast-paced contacts.

**Key Files:**
- `src/network/serialPool.cpp` / `include/network/serialPool.h` — server-side SN
  allocator
- `src/network/bandLockout.cpp` / `include/network/bandLockout.h` — claim/release
  and warning logic
- `src/ui/multiOpStatusPanel.cpp` / `include/multiOpStatusPanel.h` — per-station
  scores + roles
- `src/ui/mainWindow.cpp` — SN request/grant flow, band claim on QSY, lockout
  warnings
- `src/network/networkProtocol.h` — add SN_REQUEST, SN_GRANT, BAND_CLAIM,
  BAND_RELEASE message types

---

### SPEC-005: CW Decoder

**Priority:** P2 | **Depends On:** None | **Enables:** Nothing blocked (independent feature)

**Goal:** Add a dockable CW Decoder window that captures audio from a configurable
system audio input device, decodes incoming Morse code in real time, and displays
the decoded text with click-to-fill integration into the QSO entry panel.

**Scope:**
- New `CwDecoderWidget` QDockWidget with a scrolling decoded-text display and
  audio device selector (populated from Qt6::Multimedia `QMediaDevices`)
- Audio capture via `QAudioSource` (Qt6::Multimedia) from the selected input
  device — this is the system audio device the radio's audio output is routed to
  (physical sound card or virtual audio cable, same as fldigi uses); ContestLogX
  does not access flrig's audio directly
- DSP pipeline: Goertzel algorithm for efficient single-frequency CW tone detection;
  adaptive tone frequency tracking (operator can pin a frequency or let it track
  automatically within a configurable window); configurable speed range (5–60 WPM)
  for timing calibration
- Morse timing decoder: measures dot/dash lengths and inter-element/character/word
  gaps; adapts to the sender's speed automatically using a windowed dot-length
  estimator
- Decoded text rendered in the scrolling view; tokens (callsigns, RST values)
  are clickable:
  - Click a callsign-shaped token → fills the CALL field in QSO entry
  - Click a signal report token (e.g., 599, 57) → fills the RSTr field
- Controls in the widget toolbar: audio device selector, tone frequency display
  (read-only or pinnable), WPM display (auto-detected), squelch/threshold slider,
  clear button
- Widget added to Window menu; state (device selection, threshold) persisted in
  application settings; layout state persisted in QMainWindow saveState

**Out of Scope:**
- Decoding SSB voice (audio-to-text) — CW only
- Sending CW via the decoder (decode only; keying is handled by the existing CW
  console and flrig)
- RTTY or other digital mode decoding
- Network sharing of decoded text across multi-op stations

**Key Decisions:**
**Audio Source Decision (2026-03-21):** Audio is captured via Qt6::Multimedia
`QAudioSource` from a system audio input device, not from a flrig API. flrig does
not expose an audio stream interface; it uses the system audio layer for monitoring.
This is the same approach fldigi and other decoders use (virtual audio cable or
direct soundcard input). Qt6::Multimedia provides cross-platform device enumeration
and capture without additional dependencies.

**Decoding Algorithm Decision (2026-03-21):** Goertzel algorithm for tone detection
(efficient single-frequency DFT, low CPU) with adaptive timing rather than a
full FFT. A full FFT would be needed for a waterfall display; since we only need
tone presence/absence detection, Goertzel is appropriate. Speed range is user-
configurable as a hint; the decoder adapts within that range automatically.

**Key Files:**
- `src/audio/cwAudioDecoder.cpp` / `include/audio/cwAudioDecoder.h` — DSP +
  Morse timing engine (Goertzel + dot-length estimator)
- `src/audio/audioCapture.cpp` / `include/audio/audioCapture.h` — `QAudioSource`
  wrapper; emits `audioReady(QByteArray)` signal for decoder consumption
- `src/ui/cwDecoderWidget.cpp` / `include/cwDecoderWidget.h` — dock widget,
  scrolling text, device selector, clickable token detection
- `src/ui/mainWindow.cpp` — add dock, wire click signals to QSO entry fields
- `CMakeLists.txt` — add `Qt6::Multimedia` to target_link_libraries

---

### SPEC-006: Online Score Publishing

**Priority:** P2 | **Depends On:** None | **Enables:** Nothing blocked | **Spec Folder:** `specs/003-online-scoring/`

**Status:** 🔄 Spec + Plan + Tasks complete (draft, 2026-03-23). Ready for `/speckit.implement`.

**Goal:** Real-time contest score posting to contestonlinescore.com so operators and clubs can track live results on the public scoreboard.

**Scope (from spec):**
- Preferences pane for contestonlinescore.com credentials (callsign + password) and posting interval, persisted across sessions
- Automatic periodic POST of the current score (QSOs, points, multipliers, breakdown by band/mode) at the configured interval while a contest log is open
- Manual "Post Now" action and status indicator showing last successful post / error state
- Optional per-contest enable/disable; gracefully degrades when offline

**Out of Scope:**
- Private team scoreboards or other score-reporting services
- Historical log upload / Cabrillo submission (distinct feature)

**Key Files:** `src/network/onlineScoreClient.cpp` (planned), preferences dialog extension, mainWindow score hook. See `specs/003-online-scoring/plan.md` and `tasks.md` for the detailed plan.

---

### SPEC-007: Unify Station Config (stationClasses → userPrompts)

**Priority:** P2 | **Depends On:** None | **Enables:** Cleaner station-class-dependent contests (ARRL DX, SPDX, etc.) | **Spec Folder:** `specs/002-unify-station-config/`

**Status:** 🔄 Spec draft + requirements checklist (2026-03-22). Needs `/speckit.plan` and `/speckit.tasks`.

**Goal:** Replace the legacy `stationClasses` system with enhanced `userPrompts`, unifying all station-dependent contest configuration (exchange fields, scoring, multipliers, partner restrictions) under a single mechanism.

**Scope (from spec):**
- Station type selected via standard `userPrompts` during contest setup (no separate station class dialog)
- `userPromptValues` drive exchange-field display, scoring rules, multiplier categories, and invalid-partner enforcement
- Migration path for existing contest JSON files that use `stationClasses`
- Backward compatibility for already-saved CLX logs created under the old system

**Out of Scope:**
- Contest-specific rule redesigns — this is a refactor of the config mechanism, not a rule change

**Key Files:** `src/contestengine.cpp`, contest JSON loader, contest setup dialog, and any contest JSON files currently using `stationClasses` (ARRL DX, SPDX, and similar).

---

## Environment & Infrastructure

### Existing Infrastructure (No Changes Needed)

| Resource | Detail |
|----------|--------|
| Rig control | flrig via XML-RPC (`QNetworkAccessManager` / `QXmlStreamReader`) |
| DX cluster | Telnet via `QTcpSocket` in `DxClusterPanel` |
| Qt6::Network | Already linked in `CMakeLists.txt` — available for TCP server/client |
| Build system | CMake + `Makefile` wrapper; `make` / `make test` / `make test-logs` |
| Test framework | `scripts/run_log_tests.py` + `test_logs/automated_tests.json` |

### Changes Required

| Change | Where | Detail |
|--------|-------|--------|
| Network server listen port | Preferences dialog | Configurable port (default: 52001) |
| Radio 2 host/port | Preferences dialog | Second flrig XML-RPC endpoint |
| SO2R enable toggle | Preferences dialog | Enables second radio polling and focus UI |
| `src/core/` directory | New directory | Extract `RigController` from `mainWindow.cpp` |
| `src/network/` directory | New directory | All networking code for SPEC-003/004 |
| `include/network/` directory | New directory | Network headers |
| `src/audio/` directory | New directory | CW decoder DSP engine (SPEC-005) |
| `include/audio/` directory | New directory | Audio capture + decoder headers |
| Qt6::Multimedia | `CMakeLists.txt` | Required for `QAudioSource` / `QMediaDevices` (SPEC-005) |

### Local Development Setup

| Requirement | How |
|-------------|-----|
| Second flrig instance | Launch with `flrig --port 12346` alongside default port 12345 |
| Multi-station test | Run two ContestLogX instances on localhost with different ports |
| Network test | Use `nc` or a second machine on the LAN to test TCP connections |
| CW decoder test | Use a virtual audio cable (e.g., PulseAudio loopback) to route CW audio from fldigi or a web-based Morse sender into ContestLogX |

---

## References

- **Constitution:** `.specify/memory/constitution.md`
- **Project Standards:** `CLAUDE.md`
- **Developer Notes:** `docs/DeveloperNotes.md`
- **CLX Format:** `docs/CLX_FORMAT_SPEC.md`
- **Contest Module Format:** `docs/contest-module-format.md`
- **Workflow Template:** `~/.claude/skills/speckit-coach/templates/workflow-template.md`
