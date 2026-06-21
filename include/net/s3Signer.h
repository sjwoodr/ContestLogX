/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef S3SIGNER_H
#define S3SIGNER_H

#include <QString>
#include <QByteArray>
#include <QMap>
#include <QDateTime>

/**
 * AWS Signature Version 4 signer (S3 service), implemented with Qt crypto only
 * (QCryptographicHash + QMessageAuthenticationCode). No network types here, so
 * this is deterministically unit-testable against AWS published test vectors.
 *
 * See specs/005-cloud-storage/contracts/cloud-storage-provider.md §3 and
 * research.md §R2. The secret key is used only to derive the signature; it is
 * never returned, logged, or transmitted.
 */
namespace S3Signer {

struct SigningInput {
    QString httpMethod;                 // "GET", "PUT", "DELETE", "HEAD"
    QString host;                       // e.g. "s5lu.com"
    QString canonicalUri;               // path, e.g. "/bucket/key" (NOT yet encoded)
    QMap<QString, QString> queryParams; // unencoded keys/values
    QMap<QString, QString> headers;     // extra signed headers (lowercased name -> value)
    QString hexPayloadHash;             // lowercase hex SHA-256 of the body
    QString region;                     // e.g. "global", "us-east-1"
    QString service = QStringLiteral("s3");
    QString accessKey;
    QString secretKey;
    QDateTime utcTimestamp;             // single UTC instant for x-amz-date + scope
};

struct SignedHeaders {
    QString authorization;   // full Authorization header value
    QString amzDate;         // x-amz-date: YYYYMMDDTHHMMSSZ
    QString contentSha256;   // x-amz-content-sha256 (== hexPayloadHash)
};

/** SHA-256 hex digest (lowercase) of arbitrary data. */
QString hashSha256Hex(const QByteArray& data);

/** Hex of the SHA-256 of the empty string (used for empty-body GET/LIST). */
QString emptyPayloadHash();

/** RFC-3986 encode. If encodeSlash is false, '/' is left intact (for the path). */
QString uriEncode(const QString& value, bool encodeSlash);

/** Produce the SigV4 Authorization header (and the dependent header values). */
SignedHeaders sign(const SigningInput& in);

} // namespace S3Signer

#endif // S3SIGNER_H
