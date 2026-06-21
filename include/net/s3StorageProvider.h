/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef S3STORAGEPROVIDER_H
#define S3STORAGEPROVIDER_H

#include <QThread>
#include "net/cloudStorageProvider.h"

class S3StorageWorker;

/**
 * S3-backed CloudStorageProvider. Main-thread facade that owns a QThread and an
 * S3StorageWorker moved onto it (mirrors HamlibClient/HamlibWorker). FileLu and
 * AWS S3 are the same class, differing only by the S3Config passed in.
 */
class S3StorageProvider : public CloudStorageProvider
{
    Q_OBJECT

public:
    S3StorageProvider(CloudProviderType type, const S3Config& config, QObject* parent = nullptr);
    ~S3StorageProvider() override;

    CloudProviderType type() const override { return m_type; }
    bool isConfigured() const override { return m_config.isComplete(); }

public slots:
    void listLogs() override;
    void downloadLog(const QString& key, const QString& localPath) override;
    void uploadLog(const QString& key, const QString& localPath) override;
    void deleteLog(const QString& key) override;
    void checkExists(const QString& key) override;
    void testConnection() override;

private:
    CloudProviderType m_type;
    S3Config m_config;
    QThread m_thread;
    S3StorageWorker* m_worker;
};

#endif // S3STORAGEPROVIDER_H
