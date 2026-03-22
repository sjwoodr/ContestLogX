/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CSVFILE_H
#define CSVFILE_H

#include <QString>
#include <QList>
#include "qsoRecord.h"

class CsvFile
{
public:
    CsvFile();

    bool load(const QString& filename, QList<QsoRecord>& qsos);
    bool save(const QString& filename, const QList<QsoRecord>& qsos);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;

    QString formatDateTime(const QDateTime& dt) const;
    QDateTime parseDateTime(const QString& str) const;
};

#endif // CSVFILE_H
