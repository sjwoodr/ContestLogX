/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "csvFile.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

CsvFile::CsvFile()
{
}

QString CsvFile::formatDateTime(const QDateTime& dt) const
{
    return dt.toUTC().toString("yyyy-MM-dd HH:mm:ss");
}

QDateTime CsvFile::parseDateTime(const QString& str) const
{
    QDateTime dt = QDateTime::fromString(str, "yyyy-MM-dd HH:mm:ss");
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

bool CsvFile::load(const QString& filename, QList<QsoRecord>& qsos)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot open file: " + file.errorString();
        return false;
    }

    QTextStream in(&file);
    qsos.clear();

    // Read header line
    QString header = in.readLine();
    if (!header.startsWith("DateTime")) {
        m_lastError = "Invalid CSV format: missing header";
        return false;
    }

    int lineNum = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        lineNum++;

        if (line.trimmed().isEmpty())
            continue;

        QStringList fields = line.split(',');
        if (fields.count() < 7) {
            qWarning() << "Line" << lineNum << "has too few fields, skipping";
            continue;
        }

        QsoRecord qso;
        qso.setDateTime(parseDateTime(fields[0]));
        qso.setCall(fields[1]);
        qso.setFrequency(fields[2]);
        qso.setMode(fields[3]);
        qso.setBand(fields[4].toInt());
        qso.setExchange(fields[5]);
        qso.setSerial(fields[6].toULong());

        if (fields.count() > 7)
            qso.setDupe(fields[7] == "D");

        qsos.append(qso);
    }

    qDebug() << "Loaded" << qsos.count() << "QSOs from CSV:" << filename;
    return true;
}

bool CsvFile::save(const QString& filename, const QList<QsoRecord>& qsos)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = "Cannot create file: " + file.errorString();
        return false;
    }

    QTextStream out(&file);

    // Write header
    out << "DateTime,Call,Frequency,Mode,Band,Exchange,Serial,Dupe\n";

    // Write QSOs
    for (const QsoRecord& qso : qsos) {
        out << formatDateTime(qso.getDateTime()) << ","
            << qso.getCall() << ","
            << qso.getFrequency() << ","
            << qso.getMode() << ","
            << qso.getBand() << ","
            << qso.getExchange() << ","
            << qso.getSerial() << ","
            << (qso.isDupe() ? "D" : "") << "\n";
    }

    qDebug() << "Saved" << qsos.count() << "QSOs to CSV:" << filename;
    return true;
}
