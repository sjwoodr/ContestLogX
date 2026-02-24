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
