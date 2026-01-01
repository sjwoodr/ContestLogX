/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "supercheckpartial.h"
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QEventLoop>
#include <QDebug>

SuperCheckPartial& SuperCheckPartial::instance()
{
    static SuperCheckPartial scp;
    return scp;
}

SuperCheckPartial::SuperCheckPartial()
{
}

bool SuperCheckPartial::loadDatabase(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open SCP file:" << filePath;
        return false;
    }
    
    m_callsigns.clear();
    
    while (!file.atEnd()) {
        QString line = file.readLine().trimmed();
        if (!line.isEmpty()) {
            // SCP file format: one callsign per line
            m_callsigns.insert(line.toUpper());
        }
    }
    
    file.close();
    qDebug() << "Loaded" << m_callsigns.size() << "callsigns from SCP database";
    return true;
}

QStringList SuperCheckPartial::search(const QString& prefix, int maxResults) const
{
    if (prefix.isEmpty() || m_callsigns.isEmpty()) {
        return QStringList();
    }
    
    QString searchPrefix = prefix.toUpper();
    QStringList results;
    
    // Search for callsigns that start with the prefix
    for (const QString& call : m_callsigns) {
        if (call.startsWith(searchPrefix)) {
            results.append(call);
            if (results.size() >= maxResults) {
                break;
            }
        }
    }
    
    return results;
}

QString SuperCheckPartial::getDataFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("master.scp");
}

bool SuperCheckPartial::downloadLatestDatabase(const QString& targetPath, QString& errorMessage)
{
    QUrl url("https://supercheckpartial.com/MASTER.SCP");
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    
    // Set a timeout
    request.setTransferTimeout(30000); // 30 seconds
    
    // Allow insecure SSL connections (bypass certificate verification)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
    
    QNetworkReply* reply = manager.get(request);
    
    // Create event loop to wait for download
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    loop.exec();
    
    if (reply->error() != QNetworkReply::NoError) {
        errorMessage = QString("Download failed: %1").arg(reply->errorString());
        reply->deleteLater();
        return false;
    }
    
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        errorMessage = QString("Failed to write file: %1").arg(targetPath);
        reply->deleteLater();
        return false;
    }
    
    file.write(reply->readAll());
    file.close();
    reply->deleteLater();
    
    qDebug() << "Downloaded SCP database to:" << targetPath;
    return true;
}
