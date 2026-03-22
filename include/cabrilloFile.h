/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CABRILLOFILE_H
#define CABRILLOFILE_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include "qsoRecord.h"

class CabrilloFile
{
public:
    CabrilloFile();

    bool load(const QString& filename, QList<QsoRecord>& qsos,
              const QJsonObject& contestDef = QJsonObject());

    bool exportToFile(const QString& filename,
                      const QList<QsoRecord>& qsos,
                      const QJsonObject& contestDef,
                      const QJsonObject& headerData,
                      const QString& myCall = QString(),
                      const QString& selectedMode = QString());

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
    QString m_myCall;

    QString generateHeader(const QJsonObject& contestDef, const QJsonObject& headerData,
                           const QString& selectedMode = QString());
    QString generateQsoLine(const QsoRecord& qso, const QString& qsoTemplate);
    QString formatFrequency(double freqKhz);
    QString formatFrequency(const QString& freq);
    bool isHeaderRequired(const QString& headerName, const QJsonArray& requiredHeaders);
};

#endif // CABRILLOFILE_H
