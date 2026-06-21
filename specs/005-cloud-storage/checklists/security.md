# Security Requirements Quality Checklist: Cloud Storage Backends

**Purpose**: Validate that the spec/plan express complete, clear, consistent security requirements
before implementation. These are "unit tests for the requirements," not tests of the code.
**Created**: 2026-06-21
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md) | [contracts](../contracts/cloud-storage-provider.md)
**Domain**: Security

## Credential Storage at Rest

- [x] CHK001 - Is it explicitly stated that stored credentials use *obfuscation, not encryption*, and is that limitation acknowledged as an accepted risk? [Clarity, Spec §Assumptions, FR-004]
- [x] CHK002 - Are the specific secrets that must be protected (access key, secret key) enumerated, distinct from non-secret config (endpoint, region, bucket)? [Completeness, data-model §Settings JSON]
- [x] CHK003 - Is a requirement defined that non-secret config (endpoint/region/bucket) is stored in plaintext while only keys are obfuscated? [Clarity, data-model §Settings JSON]
- [x] CHK004 - Are requirements stated for the on-disk location and accessibility (file permissions / single-user assumption) of the settings file holding the obfuscated secrets? [Gap]
- [x] CHK005 - Is the threat model for credential storage documented (what the obfuscation does and does NOT defend against, e.g. casual inspection vs. a determined local attacker)? [Gap, Traceability]

## Secret Handling in UI & Logs

- [x] CHK006 - Is there a requirement that the secret key is masked in the UI (not shown in plaintext)? [Completeness, Spec §FR-004, US3 AS3]
- [x] CHK007 - Is there an explicit requirement that secrets are NEVER written to the debug log or any log output? [Gap]
- [x] CHK008 - Is there an explicit requirement that error messages never contain the secret key, signing key, or full `Authorization` material? [Gap, contracts §Error semantics]
- [x] CHK009 - Are requirements defined to keep secrets out of the local cloud-cache working copies and out of the `.clx` files themselves? [Gap]
- [x] CHK010 - Is the access key (non-masked but still sensitive) handling specified consistently with the secret key, or is its lesser sensitivity intentionally decided? [Consistency, Ambiguity]

## SigV4 / Wire Secret Exposure

- [x] CHK011 - Does the contract make explicit that the secret key is NEVER transmitted on the wire — only the derived SigV4 signature? [Clarity, contracts §SigV4]
- [x] CHK012 - Are requirements defined for which request components are signed (host, x-amz-date, x-amz-content-sha256) so signing integrity is unambiguous? [Completeness, contracts §SigV4]
- [x] CHK013 - Is there a requirement that signing material (derived signing key, kSigning) is not persisted or exposed beyond the signing operation? [Gap]

## Transport Security (TLS)

- [x] CHK014 - Is there an explicit requirement that all provider endpoints use HTTPS/TLS only? [Gap]
- [x] CHK015 - Is there a requirement prohibiting fallback to plaintext HTTP (and behavior if a user enters an `http://` endpoint)? [Gap, Edge Case]
- [x] CHK016 - Is there a requirement that TLS certificate validation is NOT disabled / no insecure-transport override exists? [Gap]

## Stub Providers

- [x] CHK017 - Is it explicitly required that Dropbox, Google Drive, and iCloud Drive collect and store NO credentials while disabled? [Completeness, Spec §FR-005, data-model]
- [x] CHK018 - Is the requirement clear that stub providers cannot be reached by the open/save flow (no accidental credential prompt or use)? [Consistency, Spec §FR-005, SC-006]

## Data Loss / Overwrite Safety

- [x] CHK019 - Is accidental remote overwrite gated by an explicit confirmation requirement? [Completeness, Spec §FR-012, US1 AS3]
- [x] CHK020 - Is the requirement that the local working copy is preserved on a failed upload (no data loss) stated and unambiguous? [Clarity, Spec §FR-018, Edge Cases]
- [x] CHK021 - Are requirements defined for what the user is told when an upload partially fails (cloud copy may be incomplete)? [Completeness, Spec §Edge Cases]

## Error Hygiene & Failure Disclosure

- [x] CHK022 - Are the distinct error classes (auth failure, missing bucket, object-not-found, timeout) specified such that messages are actionable WITHOUT leaking secrets? [Consistency, Spec §FR-014, contracts §Error semantics]
- [x] CHK023 - Is it required that local filesystem storage remains available in every cloud-failure case (no lockout)? [Coverage, Spec §FR-014]

## Notes

- Items left unchecked after review are gaps to remediate in spec.md / plan.md before `/speckit.tasks`.
- Security posture is intentionally "match existing app credential handling" (obfuscation) per the
  constitution's simplicity principle — but that decision and its limits must be *written down*, which
  several items above test for.
