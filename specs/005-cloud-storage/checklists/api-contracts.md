# API / Contract Requirements Quality Checklist: Cloud Storage Backends

**Purpose**: Validate the completeness, clarity, and consistency of the S3 wire contract and the
CloudStorageProvider interface requirements before implementation.
**Created**: 2026-06-21
**Feature**: [spec.md](../spec.md) | [plan.md](../plan.md) | [contracts](../contracts/cloud-storage-provider.md)
**Domain**: API / Contracts

## Interface Contract Completeness

- [x] CHK001 - Are all provider operations the open/save flow needs (list, download, upload, exists-check, optional delete, test) enumerated with defined inputs/outputs? [Completeness, contracts §1]
- [x] CHK002 - Is the asynchronous, non-blocking nature of every operation specified (no GUI-thread blocking), including the single allowed bounded-blocking exception? [Clarity, contracts §Threading, Spec §FR-013]
- [x] CHK003 - Are the result/error signals defined for every operation (one success path and one failure path each)? [Consistency, contracts §1 Signals]
- [x] CHK004 - Is the data returned by a list operation specified (key, size, last-modified) and its filtering rule (.clx only, case-insensitive)? [Completeness, contracts §ListObjectsV2, data-model §CloudObject]

## SigV4 Signing Correctness

- [x] CHK005 - Is the canonical-request construction fully specified (method, URI, query, headers, signed-headers, payload hash order)? [Completeness, research §R2, contracts §SigV4]
- [x] CHK006 - Are the URI-encoding rules stated unambiguously (RFC-3986; `~` not encoded; path separator `/` not encoded)? [Clarity, research §R2]
- [x] CHK007 - Is the payload-hash requirement specified for both empty-body (GET/LIST) and non-empty-body (PUT) requests, including the canonical empty-string SHA-256? [Completeness, research §R2/R4]
- [x] CHK008 - Is it required that `x-amz-date` and the credential-scope date derive from a single UTC timestamp? [Clarity, research §R2]
- [x] CHK009 - Is an objective acceptance criterion defined for signing correctness (reproduce an AWS published SigV4 test vector)? [Measurability, contracts §Acceptance test, quickstart]
- [x] CHK010 - Is the signed-headers minimum set specified (host; x-amz-content-sha256; x-amz-date)? [Completeness, contracts §SigV4]

## Addressing & Provider Parameterization

- [x] CHK011 - Is path-style addressing required and the URL shape `https://<endpoint>/<bucket>/<key>` specified? [Clarity, research §R1, contracts §2]
- [x] CHK012 - Is it specified that FileLu and AWS S3 differ ONLY by config (endpoint/region/bucket/keys/path-style) and use one client? [Consistency, Spec §FR-016, research §R5]
- [x] CHK013 - Are the default endpoint (`https://s5lu.com`) and region (`global`) for FileLu, and the regional-endpoint expectation for AWS, documented? [Completeness, research §R1, data-model]

## Request/Response Coverage Per Operation

- [x] CHK014 - Is ListObjectsV2 pagination handled in requirements (`IsTruncated` + `NextContinuationToken` loop) so long lists are not silently truncated? [Coverage, research §R4, contracts §ListObjectsV2]
- [x] CHK015 - Is the upload requirement specified as single-shot (no multipart) with the appropriate content type and content-sha256? [Clarity, research §R4, contracts §PutObject]
- [x] CHK016 - Is the overwrite pre-check (HEAD or targeted list) specified and tied to the overwrite-confirmation requirement? [Consistency, contracts §HEAD, Spec §FR-012]
- [x] CHK017 - Is the success-status interpretation per operation defined (200 list/get, 200/204 put, 204 delete)? [Completeness, contracts §2]

## Error Semantics & Mapping

- [x] CHK018 - Are all required error classes mapped from HTTP status / S3 error codes (403/SignatureDoesNotMatch, 404 NoSuchBucket, 404 NoSuchKey, timeout, other)? [Completeness, Spec §FR-014, contracts §Error semantics]
- [x] CHK019 - Is each error class required to produce a distinct, user-actionable message? [Clarity, Spec §SC-004]
- [x] CHK020 - Is parsing of the S3 XML error body (`<Code>`/`<Message>`) specified for the "other" failure case? [Coverage, contracts §Error semantics]
- [x] CHK021 - Is a transfer/connection timeout requirement specified so a dead endpoint fails promptly rather than hanging? [Completeness, research §R3, Edge Cases]

## Consistency & Reuse

- [x] CHK022 - Are the contract's operations consistent with the spec's functional requirements (no operation in the contract that the spec doesn't justify, and vice versa)? [Consistency, Spec §Requirements vs contracts §1]
- [x] CHK023 - Is the requirement to reuse the existing local `.clx` load/save path (not reimplement parsing/serialization) stated, so round-trip fidelity is guaranteed? [Consistency, Spec §FR-010/FR-011, plan §7]
- [x] CHK024 - Is the XML response parsing technology-agnostic in the spec while the contract pins the concrete format (ListBucketResult fields)? [Consistency, contracts §ListObjectsV2]

## Notes

- Items left unchecked after review are gaps to remediate before `/speckit.tasks`.
