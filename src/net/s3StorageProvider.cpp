/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "net/s3StorageProvider.h"
#include "net/s3StorageWorker.h"
#include "debugLogger.h"

S3StorageProvider::S3StorageProvider(CloudProviderType type, const S3Config& config, QObject* parent)
    : CloudStorageProvider(parent)
    , m_type(type)
    , m_config(config)
    , m_worker(new S3StorageWorker())  // no parent — moved to thread
{
    // Register custom types for cross-thread queued signal delivery (idempotent).
    qRegisterMetaType<CloudObject>("CloudObject");
    qRegisterMetaType<QVector<CloudObject>>("QVector<CloudObject>");
    qRegisterMetaType<CloudOp>("CloudOp");

    m_worker->setProviderType(type);
    m_worker->setConfig(m_config);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Relay worker signals out to the GUI thread (auto-queued across threads).
    connect(m_worker, &S3StorageWorker::listReady,       this, &CloudStorageProvider::listReady);
    connect(m_worker, &S3StorageWorker::downloadReady,   this, &CloudStorageProvider::downloadReady);
    connect(m_worker, &S3StorageWorker::uploadFinished,  this, &CloudStorageProvider::uploadFinished);
    connect(m_worker, &S3StorageWorker::deleteFinished,  this, &CloudStorageProvider::deleteFinished);
    connect(m_worker, &S3StorageWorker::objectExists,    this, &CloudStorageProvider::objectExists);
    connect(m_worker, &S3StorageWorker::testResult,      this, &CloudStorageProvider::testResult);
    connect(m_worker, &S3StorageWorker::operationFailed, this, &CloudStorageProvider::operationFailed);

    m_thread.start();

    DebugLogger::instance().log("CloudStorage",
        QString("[%1] Provider created: %2")
            .arg(CloudProvider::logTag(type), CloudProvider::displayName(type)));
}

S3StorageProvider::~S3StorageProvider()
{
    m_thread.quit();
    m_thread.wait(5000);
}

void S3StorageProvider::listLogs()
{
    QMetaObject::invokeMethod(m_worker, "doList", Qt::QueuedConnection);
}

void S3StorageProvider::downloadLog(const QString& key, const QString& localPath)
{
    QMetaObject::invokeMethod(m_worker, "doDownload", Qt::QueuedConnection,
                              Q_ARG(QString, key), Q_ARG(QString, localPath));
}

void S3StorageProvider::uploadLog(const QString& key, const QString& localPath)
{
    QMetaObject::invokeMethod(m_worker, "doUpload", Qt::QueuedConnection,
                              Q_ARG(QString, key), Q_ARG(QString, localPath));
}

void S3StorageProvider::deleteLog(const QString& key)
{
    QMetaObject::invokeMethod(m_worker, "doDelete", Qt::QueuedConnection,
                              Q_ARG(QString, key));
}

void S3StorageProvider::checkExists(const QString& key)
{
    QMetaObject::invokeMethod(m_worker, "doCheckExists", Qt::QueuedConnection,
                              Q_ARG(QString, key));
}

void S3StorageProvider::testConnection()
{
    QMetaObject::invokeMethod(m_worker, "doTest", Qt::QueuedConnection);
}
