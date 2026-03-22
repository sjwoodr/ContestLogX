/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
    bool isKnownPrefix(const QString& prefix) const { return m_prefixMap.contains(prefix.toUpper()); }
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
