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

#ifndef DXCCDATABASE_H
#define DXCCDATABASE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>

struct DxccPrefix {
    QString prefix;
    int dxcc;
    QString country;
    QString continent;
    int cqZone;
    int ituZone;
    double latitude;
    double longitude;
    double gmtOffset;
    bool exactMatch = false;
};

struct DxccEntity {
    int dxcc;
    QString country;
    QString continent;
    int cqZone;
    int ituZone;
    double latitude;
    double longitude;
    double gmtOffset;
    QString primaryPrefix;
    QList<DxccPrefix> prefixes;
    bool waeOnly = false;  // true = DARC WAEDC list only, does NOT count for ARRL-sponsored contests
};

class DxccDatabase : public QObject
{
    Q_OBJECT

public:
    explicit DxccDatabase(QObject *parent = nullptr);
    ~DxccDatabase();

    bool loadFromFile(const QString &filename);
    bool downloadLatest();
    
    DxccEntity lookupCallsign(const QString &callsign) const;
    QString getCountry(const QString &callsign) const;
    QString getContinent(const QString &callsign) const;
    int getDxcc(const QString &callsign) const;
    int getItuZone(const QString &callsign) const;
    int getItuRegion(const QString &callsign) const;
    int mapItuZoneToRegion(int ituZone) const;
    
    bool isLoaded() const { return m_loaded; }
    QString getDataPath() const;
    QString stripPortableSuffixes(const QString &callsign) const;

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(bool success, const QString &error);

private slots:
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QString findBestMatch(const QString &callsign) const;
    void parseCtyLine(const QString &line);
    
    QMap<int, DxccEntity> m_entities;
    QMap<QString, DxccPrefix> m_prefixMap;
    bool m_loaded;
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
};

#endif // DXCCDATABASE_H
