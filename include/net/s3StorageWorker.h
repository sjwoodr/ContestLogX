/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef S3STORAGEWORKER_H
#define S3STORAGEWORKER_H

#include <QObject>
#include <QVector>
#include "net/cloudStorageTypes.h"

class QNetworkAccessManager;
class QNetworkReply;

/**
 * Worker that performs S3 HTTP operations on a background thread. The
 * QNetworkAccessManager is created lazily on the worker thread (in ensureNam(),
 * called from the slots) so all network I/O stays off the GUI thread.
 *
 * Signs every request with SigV4 (s3Signer). Path-style addressing only.
 * HTTPS is enforced; TLS validation is never disabled. The secret key is never
 * logged nor placed in any emitted message.
 */
class S3StorageWorker : public QObject
{
    Q_OBJECT

public:
    explicit S3StorageWorker(QObject* parent = nullptr);
    ~S3StorageWorker() override;

    // Called from the facade before issuing operations. Splits a "bucket" or
    // "bucket/folder/path" value into the bucket name and a key prefix.
    void setConfig(const S3Config& cfg);

    // Backend tag ("filelu"/"aws") used to prefix this worker's debug logs.
    void setProviderType(CloudProviderType type) { m_tag = CloudProvider::logTag(type); }

public slots:
    void doList();
    void doDownload(const QString& key, const QString& localPath);
    void doUpload(const QString& key, const QString& localPath);
    void doDelete(const QString& key);
    void doCheckExists(const QString& key);
    void doTest();

signals:
    void listReady(const QVector<CloudObject>& objects);
    void downloadReady(const QString& key, const QString& localPath);
    void uploadFinished(const QString& key);
    void deleteFinished(const QString& key);
    void objectExists(const QString& key, bool exists);
    void testResult(bool ok, const QString& message);
    void operationFailed(CloudOp op, const QString& message);

private:
    QNetworkAccessManager* ensureNam();

    // Tagged debug log: "[<backend>] <msg>" under the CloudStorage component.
    void dlog(const QString& msg) const;

    // Performs a fully-signed, blocking-on-worker-thread request via a local event
    // loop. Returns the reply (caller must deleteLater). httpStatus/errorMsg out-params.
    QNetworkReply* sendSigned(const QString& method,
                              const QString& key,                 // object key ("" for bucket-level)
                              const QMap<QString, QString>& query,
                              const QByteArray& body,             // empty for GET/LIST/DELETE/HEAD
                              const QString& contentType,
                              QByteArray& responseBody,
                              int& httpStatus,
                              QString& networkError);

    // Build a user-facing, secret-free error message for a failed op.
    QString classifyError(int httpStatus, const QString& networkError,
                          const QByteArray& responseBody) const;

    // Parse a ListBucketResult XML page; appends to objects; returns the
    // NextContinuationToken (empty if not truncated).
    QString parseListPage(const QByteArray& xml, QVector<CloudObject>& objects) const;

    S3Config m_config;
    QString m_tag = QStringLiteral("cloud");  // backend tag for log lines
    QString m_bucketName;   // first path segment of the configured bucket field
    QString m_prefix;       // key prefix ("clx_logs/"), empty or ends with '/'
    QNetworkAccessManager* m_nam = nullptr;
};

#endif // S3STORAGEWORKER_H
