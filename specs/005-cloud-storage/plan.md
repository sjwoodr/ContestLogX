# Implementation Plan: Cloud Storage Backends for Contest Logs

**Branch**: `sw/0.9.2` (existing branch — no new feature branch) | **Date**: 2026-06-21 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/005-cloud-storage/spec.md`

## Summary

Add S3-compatible cloud storage as an open/save destination for `.clx` contest logs, alongside
the local filesystem (which remains the default). A new "Cloud Storage" section in Preferences
configures providers; FileLu (via its S3-compatible `s5lu.com` endpoint) and AWS S3 are
functional, while Dropbox/Google Drive/iCloud Drive are present-but-disabled stubs. The S3 client
is self-implemented on QtNetwork + QCryptographicHash (AWS SigV4 signing) — no new dependencies —
and runs on a background QThread worker mirroring the existing `HamlibClient`/`HamlibWorker`
pattern so the QSO-entry UI never blocks. Opening downloads the chosen object to a local working
copy and reuses the existing `FileHandler` load path; saving writes locally via the existing save
path then uploads.

## Technical Context

**Language/Version**: C++17
**Primary Dependencies**: Qt6 (Core, Widgets, Network, Xml) — all already linked; **no new deps**
**Storage**: Application JSON settings file (existing `Settings` singleton) for provider config;
local working-copy files under `QStandardPaths::AppLocalDataLocation` for the cloud cache
**Testing**: `make` (zero-warning build, GCC+Clang), `make test` (unit), `make test-logs`
(log validation). Add focused unit tests for SigV4 signing against AWS published test vectors.
**Target Platform**: Linux (AppImage), macOS (.app bundle), Windows (MSVC / Inno Setup)
**Project Type**: Desktop application (single project)
**Performance Goals**: No perceptible latency added to QSO entry/logging; cloud ops fully async
**Constraints**: No third-party dependencies; cross-platform with `#ifdef` only where unavoidable
(this feature needs none); credentials stored with existing obfuscation; never block the GUI thread
**Scale/Scope**: Personal/club use — small `.clx` files (KBs–low MBs), tens to hundreds of objects
per bucket; single-user, single active log at a time

## Constitution Check

*GATE: evaluated against `.specify/memory/constitution.md` v1.0.1.*

| Principle | Assessment | Status |
|-----------|-----------|--------|
| I. Contest Accuracy | No scoring/mult/dupe/exchange logic touched. Cloud open/save reuses the existing `FileHandler` `.clx` load/save path verbatim, so round-tripped logs are byte-identical. `make test-logs` must still pass. | ✅ Pass |
| II. Qt6-Native & Cross-Platform | Uses only Qt6 idioms (QObject ownership, signals/slots, `QNetworkAccessManager`, `QCryptographicHash`, `QXmlStreamReader`). No third-party UI framework; the S3 client is networking, not UI, and adds no dependency. No platform-specific APIs needed → no `#ifdef` blocks required. | ✅ Pass |
| III. Keyboard-First / No Latency | All network I/O on a background `QThread` worker (Hamlib pattern). The open/save provider chooser is keyboard-navigable; when no provider is configured, the flow is byte-for-byte identical to today (FR-008). No latency added to QSO entry. | ✅ Pass |
| IV. JSON-Driven Contest Definitions | Not applicable — this feature is storage/transport, not contest mechanics. No contest JSON or engine changes. | ✅ N/A |
| V. Simplicity & YAGNI | One S3 client serves both functional providers (config-only difference). Only the 3–4 object ops we need are implemented (no multipart, no bucket mgmt). Stub providers get no concrete classes (just disabled UI). Self-rolled signing avoids a heavy SDK. | ✅ Pass |

**Result**: PASS — no violations, Complexity Tracking not required.

## Project Structure

### Documentation (this feature)

```text
specs/005-cloud-storage/
├── spec.md              # Phase 1 (/speckit.specify) — done
├── plan.md              # This file (/speckit.plan)
├── research.md          # Phase 0 — done
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   └── cloud-storage-provider.md   # CloudStorageProvider interface + S3 op contracts
├── checklists/
│   └── requirements.md  # spec quality checklist — done
└── tasks.md             # Phase 2 (/speckit.tasks — NOT created here)
```

### Source Code (repository root)

New files (follow the existing `src/net/` + `include/net/` networking convention):

```text
include/net/
├── cloudStorageProvider.h     # Abstract QObject interface (analogous to RigInterface)
├── s3StorageProvider.h        # Concrete S3-backed provider (facade owning the worker thread)
├── s3StorageWorker.h          # QObject worker living on a QThread (QNAM lives here)
├── s3Signer.h                 # Pure SigV4 signing helper (no Qt network deps → unit-testable)
└── cloudStorageTypes.h        # S3Config, CloudObject, CloudProviderType enums/structs

src/net/
├── cloudStorageProvider.cpp   # (if any shared/base impl)
├── s3StorageProvider.cpp
├── s3StorageWorker.cpp
└── s3Signer.cpp

src/utils/settings.cpp         # MODIFY: add cloudStorage section getters/setters (reuse XOR helper)
include/settings.h             # MODIFY: declare cloudStorage accessors

src/ui/preferencesDialog.cpp   # MODIFY: add "Cloud Storage" tab (load + saveSettings)
include/preferencesDialog.h    # MODIFY: declare new member widgets

src/ui/mainWindow.cpp          # MODIFY: provider chooser in onOpenLog()/onSaveLog(); cloud cache
include/mainWindow.h           # MODIFY: hold active CloudStorageProvider(s) / helpers

ui/cloudOpenDialog.* (or inline)# Object-picker dialog listing remote .clx logs (or a QDialog built in code)

CMakeLists.txt                 # MODIFY: add new .cpp to SOURCES, .h to HEADERS
CHANGELOG.md                   # MODIFY: Other Changes and Bugfixes entry
CLAUDE.md                      # MODIFY: document the cloud storage module + S3 client
```

Tests:

```text
tests/  (existing unit-test harness)
└── s3 signer test            # Validate SigV4 canonicalization/signature vs AWS test vectors
```

**Structure Decision**: Single desktop-app project. Networking code goes in `src/net/` with headers
in `include/net/`, matching the existing convention (`clxSnapshot`, `httpServer`). The signer is
isolated into a Qt-network-free helper (`s3Signer`) so it can be unit-tested deterministically
against AWS's published SigV4 test vectors without any network.

## Key Design Decisions (from research.md + repo patterns)

1. **Provider abstraction** — `CloudStorageProvider : public QObject` declares async ops
   (`listLogs`, `downloadLog`, `uploadLog`, optional `deleteLog`, `testConnection`) and result/error
   signals (`listReady`, `downloadReady`, `uploadФinished`, `operationFailed(QString)`), analogous
   to `RigInterface` in `include/rigInterface.h`. One concrete `S3StorageProvider` implements it,
   parameterized by `S3Config`. FileLu and AWS S3 are the same class, different config.

2. **Threading** — `S3StorageProvider` is the main-thread facade owning a `QThread` and an
   `S3StorageWorker` moved onto it, exactly like `HamlibClient`/`HamlibWorker`
   (`include/hamlibClient.h`, `src/rig/hamlibClient.cpp`): `worker->moveToThread(&m_thread)`,
   `connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater)`, start the thread.
   The `QNetworkAccessManager` is created in and used only from the worker thread. UI requests are
   dispatched with `QMetaObject::invokeMethod(..., Qt::QueuedConnection, ...)`; results return via
   queued signals. `testConnection` may use a short `Qt::BlockingQueuedConnection` like Hamlib's
   `doConnect` if a synchronous Preferences "Test" result is wanted (with a transfer timeout).

3. **SigV4 signing** — `s3Signer` implements the algorithm fully documented in research.md using
   `QCryptographicHash::Sha256` and `QMessageAuthenticationCode` (HMAC-SHA256). Payload-signed
   (`x-amz-content-sha256` = hex SHA-256 of the body; empty-body hash for GET/LIST). Custom RFC-3986
   URI encoder (do not rely on default `QUrl` encoding). Single UTC `QDateTime` drives both
   `x-amz-date` and credential-scope date.

4. **S3 operations** — minimal set, path-style addressing (`https://<endpoint>/<bucket>/<key>`):
   `GET /<bucket>?list-type=2&prefix=` (ListObjectsV2, parse XML with `QXmlStreamReader`, follow
   `IsTruncated`/`NextContinuationToken`, filter to `.clx`), `GET /<bucket>/<key>` (download),
   `PUT /<bucket>/<key>` (single-shot upload, no multipart), optional `DELETE`. Set
   `QNetworkRequest::setTransferTimeout` so dead links fail fast with a clear error.

5. **Settings** — new `cloudStorage` section in `m_settings` (the `QJsonObject` in
   `src/utils/settings.cpp`). Per-provider sub-objects `cloudStorage.filelu` / `cloudStorage.awss3`
   with `enabled`, `endpoint`, `region`, `bucket`, `accessKey`, `secretKey`. Reuse the existing XOR
   (`"ContestLogX"` key) + Base64 encode/decode helper used by `getOnlineScoringPassword`/
   `setOnlineScoringCredentials` for `accessKey`/`secretKey`. `Settings::save()` persists as today.

6. **Preferences UI** — add a "Cloud Storage" tab to the existing `QTabWidget` in
   `preferencesDialog.cpp`. Each functional provider is a `QFormLayout` group with: enabled
   checkbox, signup `QLabel` with an `<a href>` link, endpoint (pre-filled `https://s5lu.com` for
   FileLu), region (pre-filled `global`), bucket, access key, secret key (`QLineEdit::Password`
   echo), and a "Test connection" button + status label — mirroring the Online Scoring tab
   (lines ~304–367). Stub providers (Dropbox/GDrive/iCloud) render as a labeled group with all
   inputs `setEnabled(false)` and a "Not implemented yet" caption. Populate in the constructor;
   write back in `saveSettings()` (lines ~503–621), then `settings.save()`.

7. **Open/Save integration** — in `mainWindow.cpp`:
   - `onOpenLog()` (line ~2430): if ≥1 functional provider configured, first show a small
     source chooser (Local / FileLu / AWS S3). Local → existing `QFileDialog::getOpenFileName`
     path unchanged. Cloud → call provider `listLogs`, show a remote object-picker dialog,
     download the selected key to the cloud-cache working copy, then feed that path into the
     existing `FileHandler::loadClxWithContest()` / `LoadingWorker` flow.
   - `onSaveLog()` (line ~3068): if ≥1 functional provider configured, show a destination chooser.
     Local → unchanged. Cloud → serialize via existing `FileHandler::saveClxWithContest()` to the
     working copy, confirm overwrite if the key exists (HEAD/list check), then `uploadLog`.
   - Remember the originating provider+key of a cloud-opened log so a subsequent save defaults back
     to the same cloud destination.
   - **Zero-config path**: if no functional provider is configured, skip the chooser entirely — the
     code path is identical to today (FR-008, SC-002).

8. **Cloud cache** — working copies live under
   `QStandardPaths::writableLocation(AppLocalDataLocation)/cloud-cache/`. On failed upload, the
   local copy is retained and the user is told the cloud copy may be incomplete (FR-018).

## Phase 0: Research

Complete — see [research.md](./research.md) (R1–R7: FileLu access method, SigV4 algorithm,
background-thread transport, S3 operations/wire formats, provider abstraction, credential storage,
sync model). No unresolved `NEEDS CLARIFICATION` items.

## Phase 1: Design & Contracts

- [data-model.md](./data-model.md) — entities: `S3Config`, `CloudProviderConfig`, `CloudObject`,
  `CloudProviderType`, working-copy/cache model, settings JSON shape.
- [contracts/cloud-storage-provider.md](./contracts/cloud-storage-provider.md) — the
  `CloudStorageProvider` interface contract (methods, signals, error semantics) and the S3
  operation request/response shapes.
- [quickstart.md](./quickstart.md) — how to configure FileLu in CLX and verify save/open; how to
  run the SigV4 unit test.

## Complexity Tracking

No constitution violations — section intentionally empty.
