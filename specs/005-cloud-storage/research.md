# Phase 0 Research: Cloud Storage Backends

**Feature**: 005-cloud-storage
**Date**: 2026-06-21

This document resolves the technical unknowns for implementing S3-compatible cloud storage for
contest logs, using only Qt6 (no new third-party dependencies).

## R1: FileLu access method - S3-compatible vs native API

**Decision**: Use FileLu's S3-compatible "S5" object storage interface.

**Rationale**: FileLu exposes a standard S3-compatible API. Targeting it means a single S3
client serves both FileLu and AWS S3; the two providers differ only by configuration. The user
already has working S3/S5 access/secret keys for FileLu.

**FileLu S5 facts** (from FileLu S5 docs, https://filelu.com/pages/s5-object-storage/):
- Endpoint (default): `https://s5lu.com`. Regional: `https://us.s5lu.com`, `https://eu.s5lu.com`,
  `https://me.s5lu.com`, `https://ap.s5lu.com`.
- Region: `global` (default), or `us-east` / `eu-central` / `ap-southeast` / `me-central`.
- **Path-style addressing** (`https://endpoint/<bucket>/<key>`), not virtual-hosted.
- Access key + secret key generated in FileLu "My Account".
- Sign-up: `https://filelu.com` (developer offer: up to 200 GB free lifetime for tools that
  integrate FileLu).
- Note from docs: AWS-CLI multipart uploads should set checksum calculation to `when_required`
  to avoid `Content-Encoding: aws-chunked`. We avoid multipart entirely (see R4), so this is moot.

**Alternatives considered**: FileLu native REST API (`https://filelu.com/pages/api`) - rejected:
FileLu-only, would not generalize to AWS S3, and would be a second integration to maintain.

## R2: AWS Signature Version 4 signing with Qt only

**Decision**: Implement AWS SigV4 ("Authorization header" variant) by hand using
`QCryptographicHash` for SHA-256 and HMAC-SHA256. No AWS SDK.

**Rationale**: SigV4 is a well-specified, stable algorithm. Qt provides everything needed:
`QCryptographicHash::hash(data, QCryptographicHash::Sha256)` for hashing and
`QMessageAuthenticationCode(QCryptographicHash::Sha256, key)` for HMAC. The full AWS C++ SDK is a
large dependency that conflicts with the constitution's no-new-dependencies principle and would
complicate Windows/macOS/Linux CI.

**SigV4 algorithm (the pieces we must implement)**:

1. **Canonical request**:
   ```
   <HTTPMethod>\n
   <CanonicalURI>\n              (URI-encoded path; path-style includes /<bucket>/<key>)
   <CanonicalQueryString>\n      (sorted, URI-encoded key=value pairs)
   <CanonicalHeaders>\n          (lowercased header name:value, sorted, newline-terminated each)
   <SignedHeaders>\n             (semicolon-joined sorted lowercased header names)
   <HashedPayload>               (lowercase hex SHA-256 of the body, or "UNSIGNED-PAYLOAD")
   ```
   Required signed headers: `host`, `x-amz-content-sha256`, `x-amz-date`.

2. **String to sign**:
   ```
   AWS4-HMAC-SHA256\n
   <amzDate: YYYYMMDDTHHMMSSZ>\n
   <credentialScope: YYYYMMDD/<region>/s3/aws4_request>\n
   <hex SHA-256 of canonical request>
   ```

3. **Signing key** (chained HMAC-SHA256):
   ```
   kDate    = HMAC("AWS4"+secretKey, YYYYMMDD)
   kRegion  = HMAC(kDate, region)
   kService = HMAC(kRegion, "s3")
   kSigning = HMAC(kService, "aws4_request")
   signature = hex( HMAC(kSigning, stringToSign) )
   ```

4. **Authorization header**:
   ```
   Authorization: AWS4-HMAC-SHA256 Credential=<accessKey>/<credentialScope>,
                  SignedHeaders=<signedHeaders>, Signature=<signature>
   ```

**Key correctness pitfalls** (call out in plan/tests):
- `x-amz-content-sha256` MUST equal the hex SHA-256 of the exact request body (use the precomputed
  body hash; payload signing is simplest and avoids streaming-chunk complications). For GET/LIST
  (empty body) it is the SHA-256 of the empty string
  (`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`).
- URI encoding: AWS requires RFC 3986 encoding where `~` is NOT encoded and `/` in the path is
  NOT encoded (path segments are). `QUrl`/`QByteArray::toPercentEncoding` need careful exclude/
  include sets; implement a small dedicated encoder rather than relying on default `QUrl` behavior.
- `x-amz-date` and the date in the credential scope MUST be the same UTC day; use one `QDateTime`
  in UTC for both (`QDateTime::currentDateTimeUtc()`).
- Headers included in `SignedHeaders` must be sent verbatim on the wire (same casing of values,
  trimmed). Send `Host` explicitly (Qt sets it, but it must be in the canonical headers).

**Alternatives considered**:
- `aws-sdk-cpp` - rejected (dependency weight, CI complexity, constitution conflict).
- SigV2 (older, simpler HMAC-SHA1) - rejected: AWS S3 deprecated SigV2 in most regions and
  FileLu/modern S3-compatible stores expect SigV4.

## R3: HTTP transport on a background thread

**Decision**: Run all S3 HTTP via `QNetworkAccessManager` owned by a worker `QObject` living on a
dedicated `QThread`, mirroring the existing `HamlibWorker`/`WinKeyerWorker` pattern. The UI calls
are queued (`QMetaObject::invokeMethod` / queued signals); results and errors return via signals.

**Rationale**: Constitution III (Keyboard-First / no perceptible latency) forbids blocking the QSO
entry path. Network round-trips to cloud storage can take seconds; they must never run on the GUI
thread. The Hamlib/WinKeyer worker pattern is the established, proven approach in this codebase.

**Notes**:
- `QNetworkAccessManager` must be created in and used from the worker thread (not shared with the
  GUI thread). Each operation uses `QNetworkReply` with a `finished` signal; aggregate to the
  worker which emits a high-level result signal.
- Add a request timeout (`QNetworkRequest::setTransferTimeout`, Qt 6) so a dead connection fails
  promptly with a clear error instead of hanging.

**Alternatives considered**: synchronous I/O with a local event loop (like FlrigClient) - rejected:
risks UI stalls on slow links, contrary to the constitution.

## R4: S3 operations needed and their wire formats

**Decision**: Implement only the minimal object operations. No multipart, no bucket management.

| Operation | HTTP | Path (path-style) | Body | Response |
|-----------|------|-------------------|------|----------|
| List logs | GET | `/<bucket>?list-type=2&prefix=<>` | none | XML (`ListBucketResult`) |
| Download  | GET | `/<bucket>/<key>` | none | object bytes |
| Upload    | PUT | `/<bucket>/<key>` | file bytes | empty (200) + ETag header |
| Delete (optional) | DELETE | `/<bucket>/<key>` | none | 204 |

**ListObjectsV2 response parsing**: response is XML. Parse with **Qt Xml** (`QXmlStreamReader`),
extracting each `<Contents><Key>...</Key><Size>...</Size><LastModified>...</LastModified></Contents>`.
Filter to keys ending in `.clx`. Handle pagination via `<IsTruncated>` + `<NextContinuationToken>`
(loop until not truncated) - buckets of contest logs will be small, but pagination correctness is
cheap to add and avoids silently truncating long lists.

**Upload simplicity**: contest `.clx` files are small JSON; a single PUT of the whole body is
sufficient. No multipart upload, which also sidesteps the `aws-chunked` checksum caveat in R1.

**Rationale**: Matches the spec's open/save/list needs and the YAGNI principle; delete is optional
and can be deferred or wired to a future "delete remote log" affordance.

## R5: Provider abstraction shape

**Decision**: A single abstract `CloudStorageProvider` interface (QObject-based) exposing
async operations (`listLogs`, `downloadLog`, `uploadLog`, optional `deleteLog`, `testConnection`)
that emit result/error signals. One concrete `S3StorageProvider` implements it, parameterized by an
`S3Config { endpoint, region, bucket, accessKey, secretKey, pathStyle }`. FileLu and AWS S3 are the
same class with different config; the provider type only changes defaults and the signup link.

**Rationale**: Spec FR-016 requires FileLu and AWS S3 to share one mechanism differing only by
config. This mirrors `RigInterface` (one interface, multiple backends) and keeps the door open for
future native (non-S3) providers (Dropbox/GDrive/iCloud) implementing the same interface later - but
those are stubs this round (out of scope), so no concrete classes are written for them now (YAGNI).

**Alternatives considered**: separate `FileLuProvider` and `AwsS3Provider` classes - rejected:
duplicates the entire S3 client for a config difference.

## R6: Credential storage

**Decision**: Store cloud provider config (including secret key) in the existing JSON settings file
under a new `cloudStorage` section, reusing the same XOR obfuscation already applied to QRZ and
online-scoring passwords (`Settings` credential helpers). Mask the secret key in the UI with
`QLineEdit::Password` echo mode.

**Rationale**: Consistent with existing credential handling; no new dependency; cross-platform.
The obfuscation is not strong encryption, but it matches the application's current security posture
and the constitution's simplicity principle. (Exact helper names/structure to be confirmed against
`src/utils/settings.cpp` in the design phase and reflected in data-model.md.)

**Alternatives considered**: OS keychain via QtKeychain - rejected (new dependency, inconsistent
with existing storage).

## R7: Sync model (open/save)

**Decision**: Open = list bucket → user picks a key → download object bytes to a local working copy
file (under `QStandardPaths::writableLocation(AppLocalDataLocation)/cloud-cache/` or a temp dir) →
hand that path to the existing local log-load path. Save = serialize via the existing local save
path to a local file → upload the bytes via PUT. Track the originating provider+key so subsequent
saves of a cloud-opened log default back to the same destination.

**Rationale**: Reuses all existing, tested local open/save/parse/serialize logic; files are small;
keeps a local copy so a failed upload never loses data (spec FR-018). Minimal new surface area.

**Alternatives considered**: direct in-memory streaming to/from cloud - rejected: more complex,
risks data loss and UI stalls for negligible benefit at these file sizes.

## Open items deferred to design (Phase 1)

- Exact `Settings` obfuscation helper names and section read/write idiom (from settings.cpp).
- Exact existing open/save method names in `mainWindow.cpp` to hook the provider chooser into.
- Preferences dialog section/layout idiom to follow.

(These are repo-internal details captured in data-model.md / plan.md once confirmed; they do not
change any decision above.)
