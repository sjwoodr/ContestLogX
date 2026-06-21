# Contract: CloudStorageProvider Interface & S3 Operations

**Feature**: 005-cloud-storage | **Date**: 2026-06-21

This is the application-internal contract (a C++/Qt interface, the UI contract for a desktop app)
plus the S3 wire contract the concrete provider must satisfy.

## 1. CloudStorageProvider (abstract interface)

`CloudStorageProvider : public QObject` — analogous to `RigInterface`. All operations are
**asynchronous and non-blocking**; results/errors arrive via signals on the GUI thread.

### Methods (slots / invokable)
| Method | Purpose |
|--------|---------|
| `void listLogs()` | List `.clx` objects in the configured bucket. |
| `void downloadLog(const QString& key, const QString& localPath)` | Download object `key` to `localPath`. |
| `void uploadLog(const QString& key, const QString& localPath)` | Upload `localPath` to object `key`. |
| `void deleteLog(const QString& key)` | *(optional)* Delete object `key`. |
| `void testConnection()` | Verify credentials/connectivity (lightweight list). |
| `bool isConfigured() const` | True iff config is complete and usable. |
| `CloudProviderType type() const` | Provider identity. |

### Signals
| Signal | Emitted when |
|--------|--------------|
| `listReady(QVector<CloudObject> objects)` | listLogs succeeded. |
| `downloadReady(QString key, QString localPath)` | downloadLog succeeded; file written. |
| `uploadFinished(QString key)` | uploadLog succeeded. |
| `deleteFinished(QString key)` | deleteLog succeeded. |
| `testResult(bool ok, QString message)` | testConnection completed. |
| `operationFailed(CloudOp op, QString message)` | any op failed (clear, user-facing message). |
| `objectExists(QString key, bool exists)` | result of an overwrite pre-check (HEAD/list). |

`CloudOp` enum: `List, Download, Upload, Delete, Test, Exists`.

### Error semantics (FR-014)
`operationFailed` message MUST distinguish at least these classes, mapped from HTTP status / network
error:
| Condition | Trigger | Message theme |
|-----------|---------|---------------|
| Auth failure | HTTP 403 / `SignatureDoesNotMatch` / `InvalidAccessKeyId` | "Authentication failed — check your access/secret keys." |
| Missing bucket | HTTP 404 `NoSuchBucket` | "Bucket '<bucket>' not found." |
| Object not found | HTTP 404 `NoSuchKey` (download) | "Log '<key>' no longer exists." |
| Network timeout | `QNetworkReply::TimeoutError` / transfer timeout | "Network timeout — could not reach <endpoint>." |
| Other | any other non-2xx / network error | include HTTP status + S3 `<Code>` if present. |

In ALL failure cases the local filesystem remains available as a fallback (the provider never
disables local open/save).

**Secret-handling invariants** (all operations):
- The secret key is used ONLY to derive the SigV4 signature; it is NEVER placed in a request,
  header, URL, or query string sent to the provider (only `Authorization`'s computed signature is).
- No `operationFailed`/`testResult` message, and no log line, may contain the secret key, the
  derived signing key, or the full `Authorization` value.
- All requests MUST use `https://`; TLS certificate validation MUST NOT be disabled. A non-HTTPS
  endpoint in config is rejected/warned before any request is made.

### Threading contract
- The concrete provider is a main-thread facade owning a `QThread` + worker (Hamlib pattern).
- `QNetworkAccessManager` is owned by and used only on the worker thread.
- Public methods marshal to the worker via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- No public method blocks the GUI thread (exception: `testConnection` MAY use a bounded
  `BlockingQueuedConnection` for a synchronous Preferences result, guarded by a transfer timeout).

## 2. S3 wire contract (what S3StorageProvider/worker must produce)

Path-style addressing: `https://<endpoint-host>/<bucket>/<key>`. All requests carry SigV4
`Authorization`, `x-amz-date`, `x-amz-content-sha256`, and `Host` headers.

### ListObjectsV2
```
GET https://<endpoint>/<bucket>?list-type=2[&prefix=<p>][&continuation-token=<t>]
x-amz-content-sha256: <sha256("")>          # empty-body hash
```
Response `200`, XML `ListBucketResult`. Parse with `QXmlStreamReader`:
- repeat `<Contents>`: `<Key>`, `<Size>`, `<LastModified>`
- `<IsTruncated>true</IsTruncated>` + `<NextContinuationToken>` ⇒ fetch next page, accumulate.
- Filter results to keys ending `.clx` (case-insensitive).

### GetObject (download)
```
GET https://<endpoint>/<bucket>/<key>
x-amz-content-sha256: <sha256("")>
```
Response `200` with object bytes → write to `localPath`. `404 NoSuchKey` ⇒ object-not-found error.

### PutObject (upload)
```
PUT https://<endpoint>/<bucket>/<key>
Content-Type: application/json            # .clx is JSON
x-amz-content-sha256: <hex sha256(file bytes)>
<body = file bytes>
```
Response `200`/`204` ⇒ success (ETag header optional). Single-shot (no multipart).

### HEAD / exists pre-check (overwrite confirmation, FR-012)
```
HEAD https://<endpoint>/<bucket>/<key>     # or reuse a targeted list with prefix=<key>
```
`200` ⇒ exists (prompt to overwrite); `404` ⇒ does not exist.

### DeleteObject (optional)
```
DELETE https://<endpoint>/<bucket>/<key>
```
Response `204` ⇒ deleted.

## 3. SigV4 signing contract (s3Signer)

Pure function, no Qt network types — unit-testable in isolation.

Input: HTTP method, host, canonical URI (path), query params (map), headers (map), payload-hash
(hex), `S3Config` (region, accessKey, secretKey), and a UTC timestamp.
Output: the `Authorization` header value + the `x-amz-date` value.

MUST conform to AWS Signature Version 4 (see research.md R2):
- canonical request → string-to-sign → derived signing key (kDate/kRegion/kService/kSigning) →
  hex HMAC-SHA256 signature → `AWS4-HMAC-SHA256 Credential=.../SignedHeaders=.../Signature=...`.
- service = `s3`, terminator = `aws4_request`.
- RFC-3986 URI encoding (do NOT encode `/` in path segments separator; do NOT encode `~`).
- signed headers include at least `host;x-amz-content-sha256;x-amz-date`.

### Acceptance test (G-level)
`s3Signer` MUST reproduce the signature from at least one **AWS published SigV4 test vector**
(known input → known expected `Authorization`) in a unit test, ensuring canonicalization is exact.
Additionally, an integration smoke test against the live FileLu `s5lu.com` endpoint (PUT then GET a
small object) validates real-world correctness before merge.
