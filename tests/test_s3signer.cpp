/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest>
#include <QDateTime>
#include <QTimeZone>

#include "net/s3Signer.h"

/**
 * Validates the SigV4 signer against AWS's published "Signature Version 4"
 * S3 test vectors. If these pass, the canonicalization and key derivation are
 * exact and real S3/FileLu requests will authenticate.
 *
 * Reference: AWS docs "Examples: Signature calculations" - GET Object and
 * PUT Object examples (bucket "examplebucket", us-east-1, the well-known
 * AKIAIOSFODNN7EXAMPLE / wJalrX... credentials, date 2013-05-24).
 */
class TestS3Signer : public QObject
{
    Q_OBJECT

private:
    static QDateTime fixedDate() {
        // 2013-05-24T00:00:00Z
        return QDateTime(QDate(2013, 5, 24), QTime(0, 0, 0), QTimeZone::utc());
    }
    static QString accessKey() { return QStringLiteral("AKIAIOSFODNN7EXAMPLE"); }
    static QString secretKey() { return QStringLiteral("wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"); }

private slots:
    void emptyPayloadHashConstant()
    {
        // SHA-256 of "" must equal the AWS canonical empty-body constant.
        QCOMPARE(S3Signer::hashSha256Hex(QByteArray()), S3Signer::emptyPayloadHash());
    }

    void uriEncodePreservesPathSlash()
    {
        QCOMPARE(S3Signer::uriEncode(QStringLiteral("/a/b c~d"), false),
                 QStringLiteral("/a/b%20c~d"));
        // With encodeSlash, '/' becomes %2F.
        QCOMPARE(S3Signer::uriEncode(QStringLiteral("a/b"), true),
                 QStringLiteral("a%2Fb"));
    }

    void getObjectVector()
    {
        // AWS "GET Object" example: GET /test.txt with a Range header.
        S3Signer::SigningInput in;
        in.httpMethod = QStringLiteral("GET");
        in.host = QStringLiteral("examplebucket.s3.amazonaws.com");
        in.canonicalUri = QStringLiteral("/test.txt");
        in.headers.insert(QStringLiteral("range"), QStringLiteral("bytes=0-9"));
        in.hexPayloadHash = S3Signer::emptyPayloadHash();
        in.region = QStringLiteral("us-east-1");
        in.accessKey = accessKey();
        in.secretKey = secretKey();
        in.utcTimestamp = fixedDate();

        const S3Signer::SignedHeaders out = S3Signer::sign(in);

        QCOMPARE(out.amzDate, QStringLiteral("20130524T000000Z"));
        const QString expected = QStringLiteral(
            "AWS4-HMAC-SHA256 "
            "Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, "
            "SignedHeaders=host;range;x-amz-content-sha256;x-amz-date, "
            "Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41");
        QCOMPARE(out.authorization, expected);
    }

    void putObjectVector()
    {
        // AWS "PUT Object" example: PUT /test$file.text with body "Welcome to Amazon S3."
        // and an x-amz-storage-class header.
        const QByteArray body = QByteArrayLiteral("Welcome to Amazon S3.");

        S3Signer::SigningInput in;
        in.httpMethod = QStringLiteral("PUT");
        in.host = QStringLiteral("examplebucket.s3.amazonaws.com");
        in.canonicalUri = QStringLiteral("/test$file.text");
        in.headers.insert(QStringLiteral("date"), QStringLiteral("Fri, 24 May 2013 00:00:00 GMT"));
        in.headers.insert(QStringLiteral("x-amz-storage-class"), QStringLiteral("REDUCED_REDUNDANCY"));
        in.hexPayloadHash = S3Signer::hashSha256Hex(body);
        in.region = QStringLiteral("us-east-1");
        in.accessKey = accessKey();
        in.secretKey = secretKey();
        in.utcTimestamp = fixedDate();

        const S3Signer::SignedHeaders out = S3Signer::sign(in);

        const QString expected = QStringLiteral(
            "AWS4-HMAC-SHA256 "
            "Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, "
            "SignedHeaders=date;host;x-amz-content-sha256;x-amz-date;x-amz-storage-class, "
            "Signature=98ad721746da40c64f1a55b78f14c238d841ea1380cd77a1b5971af0ece108bd");
        QCOMPARE(out.authorization, expected);
    }
};

QTEST_MAIN(TestS3Signer)
#include "test_s3signer.moc"
