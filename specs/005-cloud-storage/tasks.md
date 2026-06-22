# Tasks: Cloud Storage Backends for Contest Logs

**Feature**: 005-cloud-storage | **Branch**: `sw/0.9.2` (no new branch)
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md) | **Contracts**: [contracts/cloud-storage-provider.md](./contracts/cloud-storage-provider.md)

**Conventions**: `[P]` = parallel-safe (different files, no incomplete deps). Story labels map to
spec user stories: [US1] FileLu save/open (P1), [US4] Open/Save chooser (P1), [US2] AWS S3 (P2),
[US3] Settings config + stubs (P2). Constraints throughout: **no new dependencies**; all network
I/O on the worker thread; reuse `FileHandler` for byte-identical `.clx` round-trips.

---

## Phase 1: Setup

- [x] T001 Add new source/header file stubs to the build: register `src/net/s3Signer.cpp`,
  `src/net/s3StorageWorker.cpp`, `src/net/s3StorageProvider.cpp` in the `SOURCES` list (~L28-98)
  and `include/net/cloudStorageTypes.h`, `include/net/cloudStorageProvider.h`,
  `include/net/s3Signer.h`, `include/net/s3StorageWorker.h`, `include/net/s3StorageProvider.h` in
  the `HEADERS` list (~L100-175) of `CMakeLists.txt`. Confirm `Qt6::Network` and `Qt6::Xml` are
  already linked (they are) - add nothing new.
- [x] T002 [P] Create `include/net/cloudStorageTypes.h` with the shared types from data-model.md:
  `enum class CloudProviderType { FileLu, AwsS3, Dropbox, GoogleDrive, ICloudDrive }`, helper
  decls `isFunctional/displayName/signupUrl`, `struct S3Config { endpoint, region, bucket,
  accessKey, secretKey; bool pathStyle=true; }`, `struct CloudProviderConfig { CloudProviderType
  type; bool enabled; S3Config s3; }`, `struct CloudObject { QString key; qint64 size;
  QDateTime lastModified; }`, `struct CloudOrigin { CloudProviderType providerType; QString key; }`
  (tracks where a cloud-opened log came from for re-save defaulting), and
  `enum class CloudOp { List, Download, Upload, Delete, Test, Exists }`.

---

## Phase 2: Foundational (blocking prerequisites for all stories)

### S3 SigV4 signer (the correctness core)

- [x] T003 [P] Create `include/net/s3Signer.h` declaring a Qt-network-free signer: a function/class
  taking (method, host, canonicalUri, query map, headers map, hexPayloadHash, `S3Config`, UTC
  `QDateTime`) and returning the `Authorization` header value + `x-amz-date`. No QNetwork types.
- [x] T004 Implement `src/net/s3Signer.cpp` per contracts §3 / research §R2: RFC-3986 URI encoder
  (do NOT encode `~` or path `/`), canonical request, string-to-sign, chained HMAC signing key
  (kDate/kRegion/kService=`s3`/kSigning=`aws4_request`), hex HMAC-SHA256 signature, using
  `QCryptographicHash::Sha256` and `QMessageAuthenticationCode`. Single UTC timestamp drives both
  `x-amz-date` and credential-scope date. **Never log or expose the secret/signing key (FR-020/21).**
- [x] T005 [P] Add a unit test in `tests/` that feeds an **AWS published SigV4 test vector**
  (known canonical request + expected `Authorization`) into `s3Signer` and asserts an exact match
  (contracts §Acceptance test). This is the key gate before any live call. Wire it into `make test`.

### Settings: cloudStorage section

- [x] T006 Add `cloudStorage` accessors to `include/settings.h`: `getCloudProviderConfig(CloudProviderType)`,
  `setCloudProviderConfig(CloudProviderType, const CloudProviderConfig&)`,
  `getConfiguredCloudProviders()` (functional+complete only).
- [x] T007 Implement those accessors in `src/utils/settings.cpp` storing under
  `m_settings["cloudStorage"]["filelu"|"awss3"]` per data-model §Settings JSON. Reuse the EXISTING
  XOR(`"ContestLogX"`)+Base64 encode/decode helper (as in `getOnlineScoringPassword`/
  `setOnlineScoringCredentials`, ~L1756-1786) for `accessKey`/`secretKey` ONLY; store
  endpoint/region/bucket plain. Do NOT persist stub providers. `Settings::save()` persists as today.

### Provider abstraction + S3 backend (worker thread)

- [x] T008 [P] Create `include/net/cloudStorageProvider.h`: abstract `CloudStorageProvider : public
  QObject` per contracts §1 - slots `listLogs/downloadLog/uploadLog/deleteLog/testConnection`,
  `isConfigured()/type()`, and signals `listReady/downloadReady/uploadFinished/deleteFinished/
  testResult/operationFailed(CloudOp,QString)/objectExists`.
- [x] T009 Create `include/net/s3StorageWorker.h` + implement `src/net/s3StorageWorker.cpp`: a
  `QObject` worker that owns a `QNetworkAccessManager` (created on the worker thread). Implement the
  S3 ops per contracts §2 path-style: ListObjectsV2 (GET `?list-type=2`, parse `ListBucketResult`
  with `QXmlStreamReader`, follow `IsTruncated`/`NextContinuationToken`, filter `.clx`
  case-insensitive), GetObject→file, single-shot PutObject (Content-Type `application/json`,
  `x-amz-content-sha256`=hex SHA-256 of body), HEAD exists-check, optional DeleteObject. Sign every
  request via `s3Signer`. Set `QNetworkRequest::setTransferTimeout`. Map HTTP/S3 errors to the
  classes in contracts §Error semantics; **never include secret/signing material in messages.**
  Enforce `https://` only; never disable TLS cert validation (FR-019).
- [x] T010 Create `include/net/s3StorageProvider.h` + implement `src/net/s3StorageProvider.cpp`: the
  main-thread facade implementing `CloudStorageProvider`, owning a `QThread` and an
  `S3StorageWorker` moved onto it (mirror `HamlibClient`/`HamlibWorker`: `moveToThread`,
  `connect(&thread,&QThread::finished,worker,&deleteLater)`, start). Marshal public calls via
  `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; relay worker result/error signals.
  `testConnection` may use a bounded `Qt::BlockingQueuedConnection` (timeout-guarded). Constructed
  from an `S3Config`; FileLu vs AWS differ only by config.

**Checkpoint**: Foundation builds (`make`, zero warnings) and the SigV4 unit test passes
(`make test`). No UI yet.

---

## Phase 3: User Story 1 - Save & open logs on FileLu (P1) 🎯 MVP

**Goal**: Configure FileLu and save/open `.clx` logs to/from its S3 bucket, fully in-app.
**Independent test**: With FileLu configured, save a log to FileLu, reopen it in a fresh session,
verify QSOs/multipliers/score are byte-identical (SC-001).

> NOTE: US1's *configuration UI* is delivered by US3 (T020-T023) and its *open/save entry points*
> by US4 (T016-T019). To exercise US1 alone before that UI exists, use a temporary hard-coded
> `S3Config` (or the FileLu settings written by T020) - the provider/worker (T008-T010) already
> deliver the core FileLu capability. T011-T015 below are the FileLu-specific glue.

- [x] T011 [US1] Add a cloud-cache helper in `src/ui/mainWindow.cpp` (+ decl in header) resolving
  working-copy paths under `QStandardPaths::writableLocation(AppLocalDataLocation)/cloud-cache/`,
  creating the dir if needed. Used for download targets and upload sources.
- [x] T012 [US1] Implement the open-from-FileLu path: given a configured provider, call `listLogs`,
  present a remote object-picker (list of `.clx` `CloudObject`s with size/date), download the chosen
  key to the cloud cache, then feed that path into the EXISTING `FileHandler::loadClxWithContest()`
  / `LoadingWorker` flow in `mainWindow.cpp` (do not reimplement parsing). Track `CloudOrigin`
  (provider+key) on the open document.
- [x] T013 [US1] Implement the save-to-FileLu path: serialize via the EXISTING
  `FileHandler::saveClxWithContest()` to the cloud-cache working copy, run the HEAD exists-check,
  prompt overwrite-confirm if present (FR-012), then `uploadLog`. On a cloud-opened doc, default the
  destination to its `CloudOrigin` (FR re-save). Preserve the local copy on failure and inform the
  user (FR-018).
- [x] T014 [US1] Wire provider result/error signals to non-blocking UI feedback (progress/active
  indicator during transfer; clear messages for auth/timeout/missing-bucket/not-found per
  contracts §Error semantics). Ensure QSO entry stays responsive throughout (FR-013, SC-003).
- [x] T015 [US1] Manual live smoke test against FileLu `https://s5lu.com`: with real keys, save a
  small log to FileLu and reopen it; confirm byte-identical round-trip and correct error messages
  for a wrong bucket / bad key. Record result in quickstart.md notes.

**Checkpoint**: FileLu save/open works end-to-end (using US3 config + US4 chooser once those land).

---

## Phase 4: User Story 4 - Choose storage location on open/save (P1)

**Goal**: Open/Save offer a Local-vs-cloud chooser when ≥1 functional provider is configured;
identical-to-today behavior when none are configured.
**Independent test**: With no provider configured, Open/Save are unchanged (SC-002); with FileLu
configured, both offer Local + FileLu.

- [x] T016 [US4] In `mainWindow.cpp` `onOpenLog()` (~L2430): if
  `Settings::getConfiguredCloudProviders()` is empty, run the EXISTING local
  `QFileDialog::getOpenFileName` path unchanged (byte-for-byte). Otherwise show a source chooser
  (Local / each configured provider) first; Local → existing path; cloud → US1 open path (T012).
- [x] T017 [US4] In `mainWindow.cpp` `onSaveLog()` (~L3068): same pattern - empty config →
  unchanged local save; otherwise destination chooser; cloud → US1 save path (T013).
- [x] T018 [P] [US4] Build the source/destination chooser as a small keyboard-navigable `QDialog`
  (or `QInputDialog`-style list) - keep it Tab/Enter friendly per constitution III. Reusable for
  both open and save.
- [x] T019 [P] [US4] Build the remote object-picker dialog (list `CloudObject`s, key/size/date,
  keyboard-navigable, empty-bucket message). Used by T012; keep separate from the chooser.

**Checkpoint**: Full open/save UX works; zero-config users see no change.

---

## Phase 5: User Story 3 - Cloud Storage settings + stubs (P2)

**Goal**: A "Cloud Storage" Preferences tab to configure FileLu/AWS and show disabled stubs.
**Independent test**: Enter FileLu creds, save, reopen Preferences → values persisted, secret
masked; Dropbox/GDrive/iCloud shown disabled + "Not implemented yet" (US3 acceptance, SC-006).

- [x] T020 [US3] Add a "Cloud Storage" tab to the `QTabWidget` in `src/ui/preferencesDialog.cpp`
  (+ member widget decls in header). FileLu group (`QFormLayout`): Enabled checkbox; signup
  `QLabel` link to `https://filelu.com`; endpoint (default `https://s5lu.com`); region (default
  `global`); bucket; access key; secret key as `QLineEdit::Password` (FR-004); "Test connection"
  button + status label. Mirror the Online Scoring tab (~L304-367).
- [x] T021 [US3] Populate the tab from `Settings` in the constructor and write back in
  `saveSettings()` (~L503-621) via `setCloudProviderConfig(...)` then `settings.save()`. Validate
  required fields when Enabled; warn on non-`https://` endpoint (FR-019).
- [x] T022 [P] [US3] Wire the "Test connection" button to `provider->testConnection()` and show the
  `testResult(ok,message)` outcome (no secrets in the message, FR-020).
- [x] T023 [P] [US3] Render Dropbox, Google Drive, iCloud Drive as labeled groups with all inputs
  `setEnabled(false)` and a "Not implemented yet" caption + signup link; collect/persist NO
  credentials (FR-023, SC-006).

**Checkpoint**: FileLu fully configurable via UI; stubs visible but inert.

---

## Phase 6: User Story 2 - AWS S3 (P2)

**Goal**: AWS S3 works identically to FileLu, differing only by config.
**Independent test**: Configure an AWS S3 bucket, save and reopen a log against it (SC-005).

- [x] T024 [US2] Add the AWS S3 group to the Cloud Storage tab (`preferencesDialog.cpp`) mirroring
  the FileLu group: enabled, signup link (`https://aws.amazon.com/s3/`), endpoint (e.g.
  `https://s3.us-east-1.amazonaws.com`), region (e.g. `us-east-1`), bucket, access/secret keys,
  Test connection. Persist via `setCloudProviderConfig(AwsS3, ...)`.
- [x] T025 [US2] Confirm the open/save chooser (T016-T019) and provider/worker (T008-T010) treat
  AWS S3 purely as another `S3Config` - no AWS-specific code path (FR-016). Add no new logic beyond
  config; verify path-style/region/endpoint flow through unchanged.

**Checkpoint**: Both functional providers selectable and working.

---

## Phase 7: Polish & Cross-Cutting

- [x] T026 [P] Update `CHANGELOG.md` under "Other Changes and Bugfixes": brief, user-facing release
  note - "Cloud Storage: open/save contest logs to FileLu or AWS S3 (Settings → Cloud Storage);
  other providers coming later." Keep it short per changelog brevity preference.
- [x] T027 [P] Update `CLAUDE.md` with a "Cloud Storage" core-module section: `CloudStorageProvider`
  abstraction, `S3StorageProvider`/`S3StorageWorker` (Hamlib-style worker thread), `s3Signer`
  (SigV4), `cloudStorage` settings section, and the open/save chooser integration points.
- [x] T028 Build gate: `make` succeeds with ZERO warnings on GCC/Clang (constitution Dev Workflow).
- [x] T029 Test gate: `make test` passes (incl. the SigV4 vector test) and `make test-logs` remains
  green (engine untouched - round-trip via `FileHandler` must keep scores/mults identical).
  Also verify **FR-022**: grep the new code paths to confirm `accessKey`/`secretKey` are never
  written to the cloud-cache working copies, the uploaded object body, or the `.clx` files -
  credentials must exist only in the application settings.
- [x] T030 Cross-platform sanity: confirm no platform-specific APIs were introduced (no new
  `#ifdef` needed); paths use `QStandardPaths`/`QDir`; verify the feature compiles on the Windows
  (MSVC) and macOS CI builds.

---

## Dependencies & Execution Order

- **Phase 1 (T001-T002)** → blocks everything.
- **Phase 2 (T003-T010)** → foundation; blocks all user stories. Within it: T004 needs T003; T005
  needs T004; T007 needs T006 + T002; T009/T010 need T008 + T004 + T002.
- **US1 (T011-T015)** depends on Phase 2. Is the MVP core.
- **US4 (T016-T019)** depends on US1's open/save paths (T012/T013) + Settings (T006/T007).
- **US3 (T020-T023)** depends on Settings (T006/T007) + provider for Test (T010).
- **US2 (T024-T025)** depends on US3 tab (T020-T021) + US4 chooser (T016-T019).
- **Polish (T026-T030)** last.

**Suggested MVP**: Phase 1 + Phase 2 + US1 + US4 + the FileLu portion of US3 (T020-T022) - this
delivers configurable FileLu save/open, the spec's primary value (SC-001).

## Parallel Opportunities

- T002 alongside T001 finalization.
- T003 / T008 (header decls) in parallel; T005 (test) parallel with T006-T007 once T004 lands.
- T018 / T019 (two dialogs) in parallel.
- T022 / T023 in parallel; T026 / T027 (docs) in parallel.

## Task Count

30 tasks: Setup 2, Foundational 8, US1 5, US4 4, US3 4, US2 2, Polish 5.
