/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QString>
#include <QList>
#include "qsorecord.h"

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
    
    // Main load/save (detects format from extension)
    bool load(const QString& filename, QList<QsoRecord>& qsos);
    bool save(const QString& filename, const QList<QsoRecord>& qsos);
    
    // Format-specific methods
    bool loadCsv(const QString& filename, QList<QsoRecord>& qsos);
    bool saveCsv(const QString& filename, const QList<QsoRecord>& qsos);
    
    bool loadAdif(const QString& filename, QList<QsoRecord>& qsos);
    bool saveAdif(const QString& filename, const QList<QsoRecord>& qsos);
    
    bool loadWl(const QString& filename, QList<QsoRecord>& qsos);
    bool saveWl(const QString& filename, const QList<QsoRecord>& qsos);
    
    bool loadWl2(const QString& filename, QList<QsoRecord>& qsos);
    bool loadWl2WithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass);
    bool saveWl2(const QString& filename, const QList<QsoRecord>& qsos);
    bool saveWl2WithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass = QString());
    
    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
    
    QString formatDateTime(const QDateTime& dt) const;
    QDateTime parseDateTime(const QString& str) const;
};

#endif // FILEHANDLER_H
