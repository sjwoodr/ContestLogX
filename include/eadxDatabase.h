/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef EADXDATABASE_H
#define EADXDATABASE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QStringList>

struct EadxEntity {
    QString prefix;     // canonical prefix as stored in the JSON (e.g., "EA6", "IT9", "GM/s")
    QString name;       // display name (e.g., "Baleares", "Sicilia")
    QString continent;  // 2-letter continent code (e.g., "EU", "NA", "AS")
    QString cqZone;     // CQ zone(s) - string because the URE list uses ranges/multi-zone (e.g., "23-24")
    QString ituZone;    // ITU zone(s) - same
};

class EadxDatabase : public QObject
{
    Q_OBJECT

public:
    explicit EadxDatabase(QObject *parent = nullptr);
    ~EadxDatabase();

    // Load the EADX-100 reference data. `filename` should point to a JSON
    // file shaped like data/eadx100.json. Returns true on success.
    bool loadFromFile(const QString &filename);

    // Locate the EADX-100 entity for a callsign. Uses longest-prefix-match.
    // Returns an empty entity (prefix.isEmpty()) if no match is found.
    EadxEntity lookupCallsign(const QString &callsign) const;

    // Returns just the entity prefix (e.g., "EA6", "K", "JA") for a callsign,
    // or an empty string if no match.
    QString getEntityPrefix(const QString &callsign) const;

    bool isLoaded() const { return m_loaded; }
    int activeEntityCount() const { return m_entities.size(); }

private:
    // Strip /portable suffixes from a callsign before lookup. Mirrors the
    // logic in DxccDatabase but returns the candidate base call.
    QString stripPortableSuffixes(const QString &callsign) const;

    QHash<QString, EadxEntity> m_entities;   // prefix -> entity (uppercase keys for quick lookup)
    QStringList m_prefixesByLength;          // sorted longest-first for lookup-time iteration
    bool m_loaded = false;
};

#endif // EADXDATABASE_H
