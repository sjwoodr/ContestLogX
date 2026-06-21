# Data Model: Cloud Storage Backends

**Feature**: 005-cloud-storage | **Date**: 2026-06-21

This feature is transport/storage; it introduces no contest-engine data. The entities below are
configuration and runtime transfer structures.

## Enums

### CloudProviderType
```
enum class CloudProviderType {
    FileLu,        // functional (S3-compatible, s5lu.com)
    AwsS3,         // functional (S3)
    Dropbox,       // stub — "Not implemented yet"
    GoogleDrive,   // stub — "Not implemented yet"
    ICloudDrive    // stub — "Not implemented yet"
};
```
- `isFunctional(type)` → true only for `FileLu` and `AwsS3`.
- `signupUrl(type)` / `displayName(type)` → static lookup (FileLu → https://filelu.com, etc.).

## Configuration entities

### S3Config
Runtime parameters for a single S3-compatible endpoint. Drives the signer and the worker.
| Field | Type | Notes |
|-------|------|-------|
| `endpoint` | QString | e.g. `https://s5lu.com` (FileLu) or `https://s3.us-east-1.amazonaws.com` (AWS) |
| `region` | QString | e.g. `global` (FileLu default) or `us-east-1` |
| `bucket` | QString | target bucket, optionally with a folder path (e.g. `mybucket/clx_logs`); the worker splits it into bucket name + key prefix |
| `accessKey` | QString | S3 access key id |
| `secretKey` | QString | S3 secret access key (sensitive) |
| `pathStyle` | bool | true (FileLu requires path-style; default true) |

Validation: a functional provider is *usable* only when endpoint, region, bucket, accessKey, and
secretKey are all non-empty. Empty/partial config ⇒ provider omitted from the open/save chooser.

### CloudProviderConfig
Persisted per-provider settings (one per provider type). Stored in the `cloudStorage` settings
section; functional providers map 1:1 to an `S3Config`.
| Field | Type | Notes |
|-------|------|-------|
| `type` | CloudProviderType | which provider |
| `enabled` | bool | user toggle in Preferences |
| `s3` | S3Config | populated for FileLu/AwsS3; unused for stubs |

## Runtime transfer entities

### CloudObject
One remote `.clx` log as returned by ListObjectsV2.
| Field | Type | Notes |
|-------|------|-------|
| `key` | QString | object key (e.g. `naqp-2026.clx`) |
| `size` | qint64 | bytes |
| `lastModified` | QDateTime | from `<LastModified>` |

Listing filters to keys ending in `.clx` (case-insensitive).

### LocalWorkingCopy (concept, not a struct)
The on-disk bridge file. Path:
`QStandardPaths::writableLocation(AppLocalDataLocation)/cloud-cache/<key>`.
- On open: download writes the object bytes here; the existing `FileHandler` loads from this path.
- On save: `FileHandler` serializes here; the bytes are uploaded; the file is retained.
- Retained after a failed upload so no data is lost (FR-018).

### CloudOrigin (tracked on the open document)
Remembers where a cloud-opened log came from so re-save defaults to the same destination.
| Field | Type | Notes |
|-------|------|-------|
| `providerType` | CloudProviderType | originating provider (or "local") |
| `key` | QString | originating object key |

## Settings JSON shape

Added under the top-level `m_settings` object in `src/utils/settings.cpp`:

```json
{
  "cloudStorage": {
    "filelu": {
      "enabled": true,
      "endpoint": "https://s5lu.com",
      "region": "global",
      "bucket": "my-logs",
      "accessKey": "<XOR+base64 obfuscated>",
      "secretKey": "<XOR+base64 obfuscated>"
    },
    "awss3": {
      "enabled": false,
      "endpoint": "https://s3.us-east-1.amazonaws.com",
      "region": "us-east-1",
      "bucket": "",
      "accessKey": "<XOR+base64 obfuscated>",
      "secretKey": "<XOR+base64 obfuscated>"
    }
  }
}
```

- `accessKey` and `secretKey` are obfuscated with the **same** XOR(`"ContestLogX"`)+Base64 helper
  used by `getOnlineScoringPassword`/`setOnlineScoringCredentials` (settings.cpp ~lines 1756–1786).
- `endpoint`/`region`/`bucket` are stored plain (not secret).
- Stub providers (Dropbox/GoogleDrive/ICloudDrive) are **not** persisted (no functional config);
  they are rendered as disabled UI only. (Keeps the settings file clean per YAGNI.)
- New accessors on `Settings` (declared in `include/settings.h`):
  `getCloudProviderConfig(type)`, `setCloudProviderConfig(type, config)`,
  `getConfiguredCloudProviders()` (returns the list of functional+usable providers for the chooser).

## State / lifecycle notes

- A functional provider appears in the open/save chooser iff `enabled == true` **and** its
  `S3Config` is complete (all required fields non-empty).
- `testConnection` performs a lightweight ListObjectsV2 (max-keys small) and reports success/failure
  without mutating anything.
- No migration needed: absence of the `cloudStorage` section means "no providers configured" and
  the open/save flow behaves exactly as before (FR-008).
