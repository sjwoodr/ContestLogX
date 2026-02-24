/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "superCheckPartial.h"
#include "debugLogger.h"
#include "settings.h"
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
#include <QCoreApplication>
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
    DebugLogger::instance().log("ScpDialog", QString("Loaded %1 callsigns from SCP database").arg(m_callsigns.size()));
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
    // Use user data path for downloaded master.scp file
    QString dataPath = Settings::getUserDataPath();
    return QDir(dataPath).filePath("master.scp");
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
    
    DebugLogger::instance().log("ScpDialog", QString("Downloaded SCP database to: %1").arg(targetPath));
    return true;
}
