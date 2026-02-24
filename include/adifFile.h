/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef ADIFFILE_H
#define ADIFFILE_H

#include <QString>
#include <QList>
#include "qsoRecord.h"

class AdifFile
{
public:
    AdifFile();

    bool load(const QString& filename, QList<QsoRecord>& qsos);
    bool save(const QString& filename, const QList<QsoRecord>& qsos);

    void setStationCallsign(const QString& callsign) { m_stationCallsign = callsign; }
    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
    QString m_stationCallsign;
};

#endif // ADIFFILE_H
