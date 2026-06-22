# Feature Specification: Cloud Storage Backends for Contest Logs

**Feature Branch**: `sw/0.9.2` (existing branch - no new feature branch)
**Spec Directory**: `specs/005-cloud-storage`
**Created**: 2026-06-21
**Status**: Draft
**Input**: User description: "Add support for multiple cloud storage providers within CLX so contest logs can be opened and saved to cloud storage in addition to the local filesystem. New 'Cloud Storage' settings section configures providers (FileLu, AWS S3, Dropbox, Google Drive, iCloud Drive). FileLu implemented first via its S3-compatible backend; AWS S3 also functional; the rest shown but marked 'Not implemented yet'. Local filesystem remains the default when nothing is configured."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Save and open logs on FileLu (Priority: P1)

An operator who uses FileLu for personal cloud storage wants their contest logs stored in
the cloud so they are backed up and available across machines. After entering their FileLu
credentials once in Settings, they can save a log to FileLu and later reopen it from FileLu,
without ever leaving the application or touching a separate sync tool.

**Why this priority**: This is the primary motivation for the feature and delivers the most
direct value. FileLu's free-lifetime-storage developer offer makes it the highest-leverage
backend to support first. Delivering only this story produces a usable, valuable MVP.

**Independent Test**: With a FileLu account configured, save a contest log to FileLu, confirm
it appears in the FileLu bucket, then open it back into a fresh session and verify all QSOs,
multipliers, and score match the original.

**Acceptance Scenarios**:

1. **Given** FileLu is configured with valid credentials and a bucket, **When** the operator
   saves the current log and chooses FileLu as the destination, **Then** the log is uploaded
   and a success confirmation is shown.
2. **Given** a log was previously saved to FileLu, **When** the operator opens a log and
   chooses FileLu, **Then** they see a list of `.clx` logs in the bucket and can select and
   open one, with all QSO data intact.
3. **Given** FileLu is selected for save and an object with the same name already exists,
   **When** the operator confirms the save, **Then** they are warned about overwriting and
   can proceed or cancel.
4. **Given** the operator enters invalid FileLu credentials, **When** they attempt to list or
   open logs, **Then** a clear authentication-failure message is shown and the local filesystem
   remains usable.

---

### User Story 2 - Save and open logs on AWS S3 (Priority: P2)

An operator who already uses Amazon S3 (or another S3-compatible store) wants to use it as a
log destination on the same terms as FileLu.

**Why this priority**: FileLu's chosen access method is S3-compatible, so AWS S3 support comes
at low marginal cost and broadens the feature's appeal. It is secondary to FileLu because it
is not the originating motivation.

**Independent Test**: With an AWS S3 bucket configured, save a log to it and reopen it,
verifying data integrity - identical flow to the FileLu test but against an AWS endpoint.

**Acceptance Scenarios**:

1. **Given** AWS S3 is configured with valid credentials, region, and bucket, **When** the
   operator saves a log to AWS S3, **Then** it is uploaded and confirmed.
2. **Given** a log exists in the AWS S3 bucket, **When** the operator opens a log from AWS S3,
   **Then** they can browse and open it identically to the FileLu flow.

---

### User Story 3 - Configure cloud providers in Settings (Priority: P2)

An operator opens a new "Cloud Storage" section in application settings, sees the list of
supported providers, follows a sign-up link if they do not have an account, and enters the
credentials/configuration for the provider(s) they use.

**Why this priority**: Configuration is a prerequisite for Stories 1 and 2 but is described
separately because it includes the non-functional provider stubs and the overall section
layout. It shares P-level urgency with the providers it enables.

**Independent Test**: Open Settings, locate the Cloud Storage section, enter FileLu
credentials, save, reopen Settings, and confirm the values persisted (with the secret key
masked). Confirm Dropbox/Google Drive/iCloud Drive appear but are clearly marked unavailable.

**Acceptance Scenarios**:

1. **Given** the operator opens Settings, **When** they navigate to Cloud Storage, **Then**
   they see FileLu, AWS S3, Dropbox, Google Drive, and iCloud Drive listed, each with a
   sign-up/info link.
2. **Given** the operator views Dropbox, Google Drive, or iCloud Drive, **When** they look at
   those rows, **Then** each is clearly labeled "Not implemented yet" and its inputs are
   disabled.
3. **Given** the operator enters and saves a secret key, **When** they reopen Settings,
   **Then** the secret key field is masked and the stored value is preserved.
4. **Given** the operator has entered credentials, **When** they use a "Test connection"
   action for a functional provider, **Then** they receive a clear success or failure result.

---

### User Story 4 - Local-primary saves with automatic cloud backup (Priority: P1)

Saving always writes to a local file the operator chose. If cloud storage is configured, each
save is automatically mirrored to the cloud in the background as a backup. Opening offers a
choice of local or a cloud source; choosing cloud prompts for where to keep the local copy. If
no cloud providers are configured, the experience is exactly as it is today.

**Why this priority**: This is the integration point that makes cloud backup part of the normal
logging workflow without changing the local-first habit. It must guarantee zero added friction
for the majority of users who never configure cloud storage.

**Independent Test**: With no cloud providers configured, confirm Open/Save behave exactly as
before. Then configure FileLu and confirm that saving locally also backs up to FileLu in the
background (status bar shows the sync), and that Open offers local vs FileLu.

**Acceptance Scenarios**:

1. **Given** no cloud providers are configured, **When** the operator opens or saves a log,
   **Then** the local file dialog appears immediately with no provider-selection step and no
   cloud activity.
2. **Given** a functional cloud provider is configured, **When** the operator saves a log,
   **Then** it is written locally and then mirrored to the cloud in the background, with the
   status bar showing the sync progress and result.
3. **Given** a functional cloud provider is configured, **When** the operator opens a log and
   chooses the cloud source, **Then** they pick a remote log, are prompted for a local location,
   and the log is downloaded there and opened.

---

### Edge Cases

- **No network / timeout**: A cloud operation that cannot reach the provider surfaces a clear,
  non-blocking error; the UI never freezes during the attempt, and the operator can fall back
  to local storage.
- **Wrong bucket / missing bucket**: Listing or saving against a nonexistent bucket reports a
  specific error rather than a generic failure.
- **Object not found**: Attempting to open a log that no longer exists in the bucket reports
  object-not-found clearly.
- **Overwrite on save**: Saving an object whose name already exists prompts for confirmation
  before overwriting.
- **Partial/failed upload**: If an upload fails midway, the operator is informed the cloud
  copy may be incomplete; the local working copy is preserved so no data is lost.
- **Empty bucket**: Opening from a provider whose bucket has no `.clx` objects shows an empty
  list with an informative message, not an error.
- **Credentials entered but provider unreachable**: The provider still appears in the chooser;
  the failure is reported only when an operation is attempted.
- **Large file / slow link**: Operations on a slow connection show progress/active state and
  remain cancelable or at least non-blocking to the QSO entry workflow.
- **Concurrent edit on another machine**: Out of scope for conflict resolution this round, but
  an overwrite-on-save confirmation is the safeguard.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide a "Cloud Storage" configuration area in application
  Settings listing these providers: FileLu, AWS S3, Dropbox, Google Drive, iCloud Drive.
- **FR-002**: Each listed provider MUST display a sign-up/info link to the provider's service.
- **FR-003**: The system MUST allow the operator to enter and persist the configuration needed
  for each functional provider (FileLu, AWS S3): endpoint, region, bucket, access key, and
  secret key.
- **FR-004**: Secret credentials MUST be masked in the UI and MUST NOT be stored in plain text
  in the application's settings; they MUST use the same obfuscation already applied to existing
  stored credentials (e.g. QRZ and online-scoring passwords).
- **FR-005**: Dropbox, Google Drive, and iCloud Drive MUST be shown but clearly marked "Not
  implemented yet", with their inputs disabled and no functional behavior.
- **FR-006**: The local filesystem is ALWAYS the primary store. Every save MUST write to a
  user-chosen local file; the system MUST NOT use a temporary/hidden location as the primary.
- **FR-007**: Cloud storage is a BACKUP. When one or more functional cloud providers are
  configured, every successful local save MUST also mirror the saved file (same name) to each
  configured provider; the operator is not asked per-save where to save.
- **FR-008**: When no cloud providers are configured, Open and Save MUST behave exactly as they
  do today (local filesystem only), with no additional selection step or cloud activity.
- **FR-009**: When opening a log with at least one functional provider configured, the system
  MUST let the operator choose the source (local filesystem or a configured provider) and, for a
  cloud source, MUST list the available `.clx` logs to pick from.
- **FR-010**: Opening a log from a cloud provider MUST prompt the operator for the LOCAL location
  to save the downloaded copy, download it there, and open that local file - such that all QSO,
  multiplier, and scoring data is identical to the stored log and subsequent saves re-sync.
- **FR-011**: The cloud backup of each save MUST run on a background thread and MUST report its
  progress/outcome in the status bar (e.g. "Syncing to FileLu…" then success/failure).
- **FR-012**: The cloud backup is a mirror and MUST overwrite the same-named object
  unconditionally (no per-save overwrite prompt); local save-as uses the normal local overwrite
  confirmation.
- **FR-013**: All cloud network operations MUST run without blocking or introducing perceptible
  latency to QSO entry, logging, or CW/SSB sending.
- **FR-014**: The system MUST surface clear, specific error messages for authentication
  failure, network timeout, missing bucket, and object-not-found, and MUST keep the local
  filesystem available as a fallback in all error cases.
- **FR-015**: The system MUST provide a way to verify a configured functional provider's
  credentials/connectivity (a "Test connection" action) and report the result clearly.
- **FR-016**: FileLu and AWS S3 MUST be reachable through a single common storage mechanism so
  that the two differ only by configuration (endpoint, region, addressing), not by separate
  code paths - keeping the design generic and reusable for future providers.
- **FR-017**: The feature MUST work on Linux, macOS, and Windows without behavioral difference.
- **FR-018**: No data loss: if a cloud save fails, the local working copy MUST remain intact
  and the operator MUST be informed.
- **FR-019**: All cloud endpoints MUST be accessed over HTTPS/TLS only. The system MUST NOT fall
  back to plaintext HTTP, MUST NOT disable TLS certificate validation, and SHOULD reject or warn
  on a non-`https://` endpoint entered in configuration.
- **FR-020**: Secret credentials (secret key, and the derived signing material) MUST NEVER be
  written to any log output (including the debug log) or included in any error message shown to
  the operator.
- **FR-021**: The secret key MUST NEVER be transmitted to the provider; only a derived
  request signature is sent. Stored secrets are obfuscated, not encrypted - this is an accepted
  limitation matching the application's existing credential handling, and MUST be documented as
  such (see Assumptions & Decisions).
- **FR-022**: Secret credentials MUST NOT be written into local working-copy files, the cloud
  cache, or the `.clx` log files themselves; credentials live only in the application settings.
- **FR-023**: Stub providers (Dropbox, Google Drive, iCloud Drive) MUST NOT collect or persist
  any credentials while marked "Not implemented yet", and MUST NOT be reachable from the
  open/save flow.

### Key Entities *(include if feature involves data)*

- **Cloud Provider Configuration**: A named, persisted set of settings for one provider -
  provider type, enabled/functional state, endpoint, region, bucket, access key, secret key
  (obfuscated), and a sign-up link. FileLu and AWS S3 are functional; the others are stubs.
- **Cloud Storage Provider (runtime)**: The active abstraction used to list, retrieve, store,
  and (optionally) delete contest log objects against a configured backend, independent of
  which concrete provider is selected.
- **Remote Log Object**: A `.clx` contest log stored in a provider's bucket, identified by a
  name/key and openable into a local working copy.
- **Local Working Copy**: The on-disk file used as the bridge between cloud objects and the
  existing local open/save/logging logic.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An operator can configure FileLu and successfully save and reopen a contest log
  entirely within the application, with 100% data fidelity (QSOs, multipliers, score match).
- **SC-002**: An operator with no cloud providers configured sees zero change to the Open/Save
  experience - the same number of steps as before the feature.
- **SC-003**: During any cloud operation, QSO entry and logging remain responsive with no
  perceptible pause (operator can keep logging while a transfer is in progress).
- **SC-004**: For each of authentication failure, network timeout, missing bucket, and
  object-not-found, the operator receives a specific, understandable message (not a generic or
  silent failure) in 100% of those cases.
- **SC-005**: The same configuration workflow and open/save flow work for AWS S3 as for FileLu,
  demonstrated by saving and reopening a log against an AWS S3 bucket.
- **SC-006**: Dropbox, Google Drive, and iCloud Drive are visibly present and unmistakably
  marked unavailable, with no path for the operator to accidentally attempt to use them.
- **SC-007**: The feature builds and passes the existing test gates (`make`, `make test`,
  `make test-logs`) on all three supported platforms with no new third-party dependencies.

## Assumptions & Decisions

These decisions were made up front based on research and the project's constitution. They are
recorded here (with rejected alternatives) so they are not re-litigated during planning.

- **FileLu access via S3-compatible API** (not FileLu's native REST API). FileLu exposes an
  S3-compatible "S5" object storage interface. Choosing it lets one mechanism serve both FileLu
  and AWS S3, and the operator already has working S3/S5 keys.
  *Rejected*: FileLu native REST API (`https://filelu.com/pages/api`) - FileLu-only, would not
  generalize to AWS S3 and would add a second, parallel integration to maintain.
- **Self-implemented S3 request signing on the existing networking stack** rather than adding a
  third-party AWS SDK. Only the operations the feature needs are implemented (list, retrieve,
  store, and optionally delete objects).
  *Rejected*: adding the `aws-sdk-cpp` dependency - conflicts with the project's
  no-new-third-party-dependencies principle and complicates cross-platform CI.
- **Credentials stored with the existing settings obfuscation**, matching how QRZ and
  online-scoring passwords are already stored. This is **obfuscation, not encryption**: it
  defends against casual inspection of the settings file, not against a determined attacker with
  local read access. The settings file lives in the application's per-user config location and is
  assumed to be protected by the OS's normal per-user file permissions (single-user desktop
  assumption); CLX does not add stronger at-rest protection this round. This tradeoff is accepted
  per the constitution's simplicity principle.
  *Rejected*: an OS keychain integration - adds a dependency and is inconsistent with current
  credential storage.
- **Download-to-local-copy on open, upload-on-save sync model**, reusing all existing local
  file logic. Contest log files are small JSON documents, so a full-file transfer is simple and
  safe.
  *Rejected*: direct streaming with no local copy - more complex and risks UI stalls or data
  loss for little benefit at these file sizes.
- **FileLu S5 facts** (for configuration defaults/validation): S3-compatible endpoints
  `https://s5lu.com` and regional `us/eu/me/ap.s5lu.com`; region `global` (default) or
  `us-east`/`eu-central`/`ap-southeast`/`me-central`; path-style addressing; access/secret keys
  generated by the user in FileLu "My Account"; sign-up at `https://filelu.com`.

## Out of Scope

- Functional implementation of Dropbox, Google Drive, and iCloud Drive (stubs only this round).
- Multi-file or bulk sync of logs.
- Background auto-sync / automatic cloud backup.
- Conflict resolution beyond a simple overwrite-on-save confirmation.
- Sharing or collaboration features.

## Dependencies

- Operator must have an account and credentials with a functional provider (FileLu or AWS S3)
  to use cloud storage; absent that, the feature is inert and local storage is unaffected.
- Network connectivity is required for cloud operations; offline operation falls back to local
  storage.
