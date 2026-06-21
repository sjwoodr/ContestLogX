/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "net/s3Signer.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QStringList>

namespace S3Signer {

QString hashSha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString emptyPayloadHash()
{
    // SHA-256 of "" — the canonical constant AWS expects for empty bodies.
    return QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

QString uriEncode(const QString& value, bool encodeSlash)
{
    // RFC 3986: unreserved = A-Z a-z 0-9 - _ . ~  are never encoded.
    // Everything else is percent-encoded as uppercase hex. '/' is encoded
    // only when encodeSlash is true (false for the canonical path).
    static const char hexDigits[] = "0123456789ABCDEF";
    const QByteArray utf8 = value.toUtf8();
    QString out;
    out.reserve(utf8.size() * 3);

    for (char c : utf8) {
        const unsigned char ch = static_cast<unsigned char>(c);
        const bool unreserved =
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~';

        if (unreserved) {
            out.append(QChar(ch));
        } else if (ch == '/' && !encodeSlash) {
            out.append(QChar('/'));
        } else {
            out.append(QChar('%'));
            out.append(QChar(hexDigits[(ch >> 4) & 0xF]));
            out.append(QChar(hexDigits[ch & 0xF]));
        }
    }
    return out;
}

static QByteArray hmacSha256(const QByteArray& key, const QByteArray& data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

static QString canonicalEncodedPath(const QString& path)
{
    // Encode each segment but preserve the '/' separators.
    if (path.isEmpty())
        return QStringLiteral("/");
    return uriEncode(path, /*encodeSlash=*/false);
}

static QString canonicalQueryString(const QMap<QString, QString>& params)
{
    // QMap iterates keys in sorted order; AWS requires sorting by encoded key.
    QStringList pairs;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        pairs << (uriEncode(it.key(), true) + QLatin1Char('=') + uriEncode(it.value(), true));
    }
    return pairs.join(QLatin1Char('&'));
}

SignedHeaders sign(const SigningInput& in)
{
    SignedHeaders result;

    const QString amzDate = in.utcTimestamp.toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    const QString dateStamp = in.utcTimestamp.toString(QStringLiteral("yyyyMMdd"));
    result.amzDate = amzDate;
    result.contentSha256 = in.hexPayloadHash;

    // --- Canonical headers (always include host, x-amz-content-sha256, x-amz-date) ---
    QMap<QString, QString> headers; // lowercased name -> value, sorted by QMap
    headers.insert(QStringLiteral("host"), in.host);
    headers.insert(QStringLiteral("x-amz-content-sha256"), in.hexPayloadHash);
    headers.insert(QStringLiteral("x-amz-date"), amzDate);
    for (auto it = in.headers.constBegin(); it != in.headers.constEnd(); ++it)
        headers.insert(it.key().toLower(), it.value().trimmed());

    QString canonicalHeaders;
    QStringList signedHeaderNames;
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        canonicalHeaders += it.key() + QLatin1Char(':') + it.value() + QLatin1Char('\n');
        signedHeaderNames << it.key();
    }
    const QString signedHeaders = signedHeaderNames.join(QLatin1Char(';'));

    // --- Canonical request ---
    const QString canonicalRequest =
        in.httpMethod + QLatin1Char('\n') +
        canonicalEncodedPath(in.canonicalUri) + QLatin1Char('\n') +
        canonicalQueryString(in.queryParams) + QLatin1Char('\n') +
        canonicalHeaders + QLatin1Char('\n') +
        signedHeaders + QLatin1Char('\n') +
        in.hexPayloadHash;

    // --- String to sign ---
    const QString credentialScope =
        dateStamp + QLatin1Char('/') + in.region + QLatin1Char('/') +
        in.service + QStringLiteral("/aws4_request");

    const QString hashedCanonicalRequest = hashSha256Hex(canonicalRequest.toUtf8());

    const QString stringToSign =
        QStringLiteral("AWS4-HMAC-SHA256\n") +
        amzDate + QLatin1Char('\n') +
        credentialScope + QLatin1Char('\n') +
        hashedCanonicalRequest;

    // --- Derive signing key (chained HMAC) ---
    const QByteArray kDate =
        hmacSha256(("AWS4" + in.secretKey).toUtf8(), dateStamp.toUtf8());
    const QByteArray kRegion = hmacSha256(kDate, in.region.toUtf8());
    const QByteArray kService = hmacSha256(kRegion, in.service.toUtf8());
    const QByteArray kSigning = hmacSha256(kService, QByteArrayLiteral("aws4_request"));

    const QString signature =
        QString::fromLatin1(hmacSha256(kSigning, stringToSign.toUtf8()).toHex());

    // --- Authorization header ---
    result.authorization =
        QStringLiteral("AWS4-HMAC-SHA256 ") +
        QStringLiteral("Credential=") + in.accessKey + QLatin1Char('/') + credentialScope +
        QStringLiteral(", SignedHeaders=") + signedHeaders +
        QStringLiteral(", Signature=") + signature;

    return result;
}

} // namespace S3Signer
