<!--
SYNC IMPACT REPORT
==================
Version change: [template] → 1.0.1 (initial ratification + P-IV clarification)
Modified principles: N/A (initial population from template)
Added sections:
  - Core Principles (5 principles)
  - Technology Stack
  - Development Workflow
  - Governance
Removed sections: N/A
Templates checked:
  - .specify/templates/plan-template.md ✅ — Constitution Check section is generic; no updates needed
  - .specify/templates/spec-template.md ✅ — No constitution-specific references; no updates needed
  - .specify/templates/tasks-template.md ✅ — No constitution-specific references; no updates needed
Deferred TODOs: None
-->

# ContestLogX Constitution

## Core Principles

### I. Contest Accuracy (NON-NEGOTIABLE)

All scoring, duplicate detection, multiplier counting, and exchange validation MUST
exactly match the official rules of the contest being implemented. Approximations
are not acceptable. Every change to contest engine logic MUST be validated by the
automated log test suite (`make test-logs`) before merging. New contests MUST
include automated test data covering scoring, multiplier identity, and dupe behavior.

### II. Qt6-Native Architecture

The application MUST be built using Qt6 idioms throughout: signals/slots for
communication, QObject ownership for memory management, Qt containers and event
system for cross-platform behavior. Cross-platform compatibility (Linux, macOS,
Windows) is a non-negotiable requirement. Platform-specific code MUST be gated
behind clearly documented `#ifdef` blocks. No third-party UI frameworks may be
introduced alongside Qt6.

### III. Keyboard-First Operator Experience

Contest logging occurs under time pressure during active radio operation. The QSO
entry path MUST remain keyboard-driven at all times. All application features MUST
be reachable via keyboard shortcut or Tab navigation. Keyboard shortcuts MUST work
whether the QSO entry dock is embedded or floating. UI changes MUST NOT introduce
any perceptible latency to the QSO entry, logging, or CW/SSB sending workflows.

### IV. JSON-Driven Contest Definitions

Each supported contest MUST be fully defined in a JSON file under `contests/`.
The long-term goal is that adding a new contest requires no C++ changes — only a
JSON file. Until a stable 1.0 release, engine changes to support previously
unanticipated contest mechanics are expected and acceptable. However, new mechanics
MUST be implemented as generic, reusable engine capabilities (not contest-specific
code paths), and the JSON schema MUST be updated to expose them. Schema changes
MUST be accompanied by migration of all existing contest files.

### V. Simplicity and YAGNI

Features are added to serve real operator workflows, not to achieve feature
completeness. The simplest solution that meets a requirement MUST be preferred over
a more flexible or configurable one. Premature abstraction, over-engineering, and
backward-compatibility shims for removed features are prohibited. Complexity that
cannot be justified by a concrete operator need MUST be removed or not introduced.

## Technology Stack

**Language**: C++17 — features beyond C++17 MUST NOT be used without explicit
decision.

**Framework**: Qt6 (Core, Widgets, Network, SerialPort, Xml) — Qt5 compatibility
MUST NOT be maintained.

**Build system**: CMake ≥ 3.16 — build logic lives in `CMakeLists.txt` and the
`Makefile` wrapper; no other build systems are introduced.

**Rig control**: flrig via XML-RPC — the sole supported rig control interface.
Direct CAT/CI-V drivers MUST NOT be added without a separate architectural decision.

**Contest definitions**: JSON files in `contests/` — one file per contest, loaded
at runtime by `ContestEngine`.

**Testing**: `make test` (unit tests), `make test-logs` (automated log validation
with multiplier verification). Both MUST pass on all supported platforms before
merging to main.

## Development Workflow

**Branching**: Feature branches named `sw/<version>-<short-description>` or
`sw/<topic>`. All work merges to `main` via pull request or direct merge after
local validation.

**Build validation**: `make` MUST succeed with zero warnings on GCC and Clang
before any commit to main.

**Test gates**: `make test` and `make test-logs` MUST pass before merging any
change to `ContestEngine`, `DxccDatabase`, or contest JSON files.

**Commit style**: Concise imperative subject line. No "Co-Authored-By" trailers.

**Contest JSON changes**: Any modification to a contest JSON file MUST be validated
by re-running `make test-logs` and verifying the affected contest's expected scores
and multiplier lists remain correct.

**Callsign test data**: The script `scripts/generate_callsigns.py` MUST be used
when generating callsigns for test logs. Manually invented callsigns are prohibited.

## Governance

This constitution supersedes all other practices, preferences, and informal
conventions documented elsewhere. When a conflict exists between this constitution
and any other document, the constitution takes precedence.

**Amendment procedure**: Amendments require (1) updating this file with a version
bump, (2) updating `CLAUDE.md` if the change affects AI-assisted development
guidance, and (3) updating `.specify/templates/` if the change affects spec or plan
gates.

**Versioning policy**: MAJOR for principle removals or redefinitions incompatible
with prior governance; MINOR for new principles or materially expanded guidance;
PATCH for clarifications, wording, or non-semantic refinements.

**Compliance review**: All spec Constitution Check gates in `plan.md` files MUST
be evaluated against the current version of this document, not the version current
at spec creation time.

**Version**: 1.0.1 | **Ratified**: 2026-03-21 | **Last Amended**: 2026-03-21
