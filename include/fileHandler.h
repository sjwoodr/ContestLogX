/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
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
 * @brief Handles reading/writing ContestLogX file formats
 * 
 * Supports:
 * - CSV format (simple, human-readable)
 * - ADIF format (standard amateur radio interchange)
 * - ContestLogX .wl binary (future - complex binary format)
 */
class FileHandler
{
public:
    FileHandler();

    // Set station callsign for ADIF export (STATION_CALLSIGN field)
    void setStationCallsign(const QString& callsign) { m_stationCallsign = callsign; }

    // Set contest definition used by loadCabrillo() when called via load()
    void setContestDefinition(const QJsonObject& def) { m_contestDefinition = def; }

    // Main load/save (detects format from extension)
    bool load(const QString& filename, QList<QsoRecord>& qsos);
    bool save(const QString& filename, const QList<QsoRecord>& qsos);
    
    // Format-specific methods
    bool loadCsv(const QString& filename, QList<QsoRecord>& qsos);
    bool saveCsv(const QString& filename, const QList<QsoRecord>& qsos);
    
    bool loadAdif(const QString& filename, QList<QsoRecord>& qsos);
    bool saveAdif(const QString& filename, const QList<QsoRecord>& qsos);

    bool loadCabrillo(const QString& filename, QList<QsoRecord>& qsos,
                      const QJsonObject& contestDef = QJsonObject());
    
    bool loadWl(const QString& filename, QList<QsoRecord>& qsos);
    bool saveWl(const QString& filename, const QList<QsoRecord>& qsos);
    
    bool loadClx(const QString& filename, QList<QsoRecord>& qsos);
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion);
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion, QString& stationClassExchange);
    // New overloads for separate name and exchange
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion, QString& stationClassExchangeName, QString& stationClassExchangeId);
    // Overload with userPromptValues
    bool loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion, QString& stationClassExchangeName, QString& stationClassExchangeId, QMap<QString, QString>& userPromptValues);
    bool saveClx(const QString& filename, const QList<QsoRecord>& qsos);
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass = QString(), const QString& stationClassExchange = QString());
    // New overload for separate name and exchange
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchangeName, const QString& stationClassExchangeId);
    // Overload with userPromptValues
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchangeName, const QString& stationClassExchangeId, const QMap<QString, QString>& userPromptValues);
    // Overload with userPromptValues and StationInfo
    bool saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchangeName, const QString& stationClassExchangeId, const QMap<QString, QString>& userPromptValues, const StationInfo& stationInfo);

    // Contest-specific memories pass-through
    void setContestCwMemories(const QList<CwMemory>& memories) { m_contestCwMemories = memories; }
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

    QString formatDateTime(const QDateTime& dt) const;
    QDateTime parseDateTime(const QString& str) const;
};

#endif // FILEHANDLER_H
