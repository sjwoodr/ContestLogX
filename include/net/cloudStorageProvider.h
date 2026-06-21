/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CLOUDSTORAGEPROVIDER_H
#define CLOUDSTORAGEPROVIDER_H

#include <QObject>
#include <QVector>
#include "net/cloudStorageTypes.h"

/**
 * Abstract cloud storage backend for contest logs (analogous to RigInterface).
 *
 * All operations are asynchronous and non-blocking: results and errors arrive
 * via signals on the GUI thread. See
 * specs/005-cloud-storage/contracts/cloud-storage-provider.md.
 */
class CloudStorageProvider : public QObject
{
    Q_OBJECT

public:
    explicit CloudStorageProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~CloudStorageProvider() override = default;

    virtual CloudProviderType type() const = 0;
    virtual bool isConfigured() const = 0;

public slots:
    virtual void listLogs() = 0;
    virtual void downloadLog(const QString& key, const QString& localPath) = 0;
    virtual void uploadLog(const QString& key, const QString& localPath) = 0;
    virtual void deleteLog(const QString& key) = 0;
    virtual void checkExists(const QString& key) = 0;
    virtual void testConnection() = 0;

signals:
    void listReady(const QVector<CloudObject>& objects);
    void downloadReady(const QString& key, const QString& localPath);
    void uploadFinished(const QString& key);
    void deleteFinished(const QString& key);
    void objectExists(const QString& key, bool exists);
    void testResult(bool ok, const QString& message);
    void operationFailed(CloudOp op, const QString& message);
};

#endif // CLOUDSTORAGEPROVIDER_H
