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

#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include "qsoRecord.h"
#include "cwMemory.h"
#include "ssbMemory.h"

class StationInfo;  // Forward declaration

/**
 * @brief Routes file load/save to the appropriate format handler.
 *
 * Format implementations live in src/fileformats/:
 *   - AdifFile    (.adi / .adif)
 *   - CabrilloFile (.log / .cbr / .cab)
 *   - CsvFile     (.csv)
 *   - ClxFile     (.clx)  — native JSON format
 */
class FileHandler
{
public:
    FileHandler();

    // Set station callsign for ADIF export (STATION_CALLSIGN field)
    void setStationCallsign(const QString& callsign) { m_stationCallsign = callsign; }

    // Set contest definition used by Cabrillo import when called via load()
    void setContestDefinition(const QJsonObject& def) { m_contestDefinition = def; }

    // Main load/save — detects format from file extension
    bool load(const QString& filename, QList<QsoRecord>& qsos);
    bool save(const QString& filename, const QList<QsoRecord>& qsos);

    // CLX format methods (native format, multiple overloads for contest metadata)
    bool loadClx(const QString& filename, QList<QsoRecord>& qsos);
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                            QString& contestFile, QString& stationClass, QString& contestVersion);
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                            QString& contestFile, QString& stationClass, QString& contestVersion,
                            QString& stationClassExchange);
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                            QString& contestFile, QString& stationClass, QString& contestVersion,
                            QString& stationClassExchangeName, QString& stationClassExchangeId);
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                            QString& contestFile, QString& stationClass, QString& contestVersion,
                            QString& stationClassExchangeName, QString& stationClassExchangeId,
                            QMap<QString, QString>& userPromptValues);

    bool saveClx(const QString& filename, const QList<QsoRecord>& qsos);
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                            const QString& contestFile, const QJsonObject& contestDef,
                            const QString& stationClass = QString(),
                            const QString& stationClassExchange = QString());
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                            const QString& contestFile, const QJsonObject& contestDef,
                            const QString& stationClass,
                            const QString& stationClassExchangeName,
                            const QString& stationClassExchangeId);
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                            const QString& contestFile, const QJsonObject& contestDef,
                            const QString& stationClass,
                            const QString& stationClassExchangeName,
                            const QString& stationClassExchangeId,
                            const QMap<QString, QString>& userPromptValues);
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                            const QString& contestFile, const QJsonObject& contestDef,
                            const QString& stationClass,
                            const QString& stationClassExchangeName,
                            const QString& stationClassExchangeId,
                            const QMap<QString, QString>& userPromptValues,
                            const StationInfo& stationInfo);

    // Contest-specific memories (written into CLX files)
    void setContestCwMemories(const QList<CwMemory>& memories)  { m_contestCwMemories = memories; }
    void setContestSsbMemories(const QList<SsbMemory>& memories) { m_contestSsbMemories = memories; }
    void setUseContestMemories(bool use) { m_useContestMemories = use; }

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
    QString m_stationCallsign;
    QJsonObject m_contestDefinition;
    QList<CwMemory> m_contestCwMemories;
    QList<SsbMemory> m_contestSsbMemories;
    bool m_useContestMemories = false;
};

#endif // FILEHANDLER_H
