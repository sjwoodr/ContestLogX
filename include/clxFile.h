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

#ifndef CLXFILE_H
#define CLXFILE_H

#include "contestInfo.h"
#include "stationInfo.h"
#include "exchangeField.h"
#include "qsoRecord.h"
#include "cwMemory.h"
#include "ssbMemory.h"
#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>

class ClxFile
{
public:
    ClxFile();
    
    // File I/O
    bool load(const QString& filename);
    bool save(const QString& filename);
    
    // Metadata
    QString version() const { return m_version; }
    QDateTime created() const { return m_created; }
    QDateTime modified() const { return m_modified; }
    
    // Contest & Station
    ContestInfo& contest() { return m_contest; }
    const ContestInfo& contest() const { return m_contest; }
    
    StationInfo& station() { return m_station; }
    const StationInfo& station() const { return m_station; }
    
    // Exchange Fields
    QList<ExchangeField>& exchangeFields() { return m_exchangeFields; }
    const QList<ExchangeField>& exchangeFields() const { return m_exchangeFields; }
    void addExchangeField(const ExchangeField& field) { m_exchangeFields.append(field); }
    
    // QSOs
    QList<QsoRecord>& qsos() { return m_qsos; }
    const QList<QsoRecord>& qsos() const { return m_qsos; }
    void addQso(const QsoRecord& qso) { m_qsos.append(qso); }
    
    // Contest-specific memories
    QList<CwMemory> cwMemories() const { return m_cwMemories; }
    void setCwMemories(const QList<CwMemory>& memories) { m_cwMemories = memories; }
    QList<SsbMemory> ssbMemories() const { return m_ssbMemories; }
    void setSsbMemories(const QList<SsbMemory>& memories) { m_ssbMemories = memories; }
    bool useContestMemories() const { return m_useContestMemories; }
    void setUseContestMemories(bool use) { m_useContestMemories = use; }
    
    // Statistics
    int totalQsos() const;
    int totalPoints() const;
    int totalMultipliers() const;
    int score() const;
    
    // Error handling
    QString lastError() const { return m_lastError; }
    
private:
    bool loadJson(const QJsonObject& json);
    QJsonObject toJson() const;
    
    QString m_version;
    QDateTime m_created;
    QDateTime m_modified;
    ContestInfo m_contest;
    StationInfo m_station;
    QList<ExchangeField> m_exchangeFields;
    QList<QsoRecord> m_qsos;
    QList<CwMemory> m_cwMemories;
    QList<SsbMemory> m_ssbMemories;
    bool m_useContestMemories = false;
    QString m_lastError;
};

#endif // CLXFILE_H
