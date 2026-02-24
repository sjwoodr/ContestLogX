/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "adifFile.h"
#include "../utils/bandPlan.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

AdifFile::AdifFile()
{
}

bool AdifFile::load(const QString& filename, QList<QsoRecord>& qsos)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot open file: " + file.errorString();
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    qsos.clear();

    // Find <eoh> or <EOH> marker
    int dataStart = content.indexOf("<eoh>", 0, Qt::CaseInsensitive);
    if (dataStart == -1) {
        dataStart = content.indexOf("<EOH>", 0, Qt::CaseInsensitive);
    }
    if (dataStart == -1) {
        dataStart = 0;  // No header, start from beginning
    } else {
        dataStart += 5; // Skip past <eoh>
    }

    // Parse QSO records
    int pos = dataStart;
    while (pos < content.length()) {
        int eorPos = content.indexOf("<eor>", pos, Qt::CaseInsensitive);
        if (eorPos == -1) break;

        QString record = content.mid(pos, eorPos - pos);

        QsoRecord qso;

        // Generic tag parser: extract all <tagname:len>value pairs
        QMap<QString, QString> tags;
        QRegularExpression tagRx("<([^:>]+):(\\d+)>", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator tagIt = tagRx.globalMatch(record);
        while (tagIt.hasNext()) {
            QRegularExpressionMatch m = tagIt.next();
            QString tagName = m.captured(1).toUpper();
            int len = m.captured(2).toInt();
            int valueStart = m.capturedEnd(0);
            QString value = record.mid(valueStart, len).trimmed();
            tags.insert(tagName, value);
        }

        // Core fields
        if (tags.contains("CALL"))
            qso.setCall(tags["CALL"]);

        if (tags.contains("QSO_DATE") && tags.contains("TIME_ON")) {
            QDateTime dt = QDateTime::fromString(tags["QSO_DATE"] + tags["TIME_ON"], "yyyyMMddHHmmss");
            dt.setTimeSpec(Qt::UTC);
            qso.setDateTime(dt);
        }

        if (tags.contains("FREQ")) {
            double freqMHz = tags["FREQ"].toDouble();
            qso.setFrequency(QString::number(freqMHz * 1000.0, 'f', 1));
        }

        // Band: read ADIF BAND tag directly; fall back to deriving from frequency
        if (tags.contains("BAND")) {
            qso.setBandName(tags["BAND"]);
        } else if (!qso.getFrequency().isEmpty()) {
            QString band = BandPlan::freq2Band(qso.getFrequency().toDouble());
            if (!band.isEmpty())
                qso.setBandName(band);
        }

        if (tags.contains("MODE"))
            qso.setMode(tags["MODE"]);

        if (tags.contains("RST_SENT"))
            qso.setRstSent(tags["RST_SENT"]);

        if (tags.contains("RST_RCVD"))
            qso.setRstReceived(tags["RST_RCVD"]);

        // Standard ADIF → exchange field reverse mappings
        QMap<QString, QString> adifToExchange;
        adifToExchange["STX"] = "SNs";
        adifToExchange["SRX"] = "SNr";
        adifToExchange["NAME"] = "NAMEr";
        adifToExchange["QTH"] = "EXCHr";
        adifToExchange["NOTES"] = "NOTES";
        adifToExchange["GRIDSQUARE"] = "GRIDr";
        adifToExchange["MY_GRIDSQUARE"] = "GRIDs";

        for (auto it = adifToExchange.constBegin(); it != adifToExchange.constEnd(); ++it) {
            if (tags.contains(it.key()))
                qso.setExchangeField(it.value(), tags[it.key()]);
        }

        // COMMENT → NOTES (fallback if NOTES tag wasn't present)
        if (tags.contains("COMMENT") && !tags.contains("NOTES"))
            qso.setExchangeField("NOTES", tags["COMMENT"]);

        // APP_CLX_* tags → strip prefix and use as exchange field name
        for (auto it = tags.constBegin(); it != tags.constEnd(); ++it) {
            if (it.key().startsWith("APP_CLX_")) {
                QString fieldName = it.key().mid(8); // strip "APP_CLX_"
                qso.setExchangeField(fieldName, it.value());
            }
        }

        qsos.append(qso);
        pos = eorPos + 5;
    }

    qDebug() << "Loaded" << qsos.count() << "QSOs from ADIF:" << filename;
    return true;
}

bool AdifFile::save(const QString& filename, const QList<QsoRecord>& qsos)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = "Cannot create file: " + file.errorString();
        return false;
    }

    QTextStream out(&file);

    // Write header
    out << "ADIF Export from ContestLogX\n";
    out << "<ADIF_VER:5>3.1.0\n";
    out << "<PROGRAMID:11>ContestLogX\n";
    QString ver = QApplication::applicationVersion();
    out << QString("<PROGRAMVERSION:%1>%2\n").arg(ver.length()).arg(ver);
    out << "<EOH>\n\n";

    // Write QSOs
    for (const QsoRecord& qso : qsos) {
        QDateTime dt = qso.getDateTime();
        QString call = qso.getCall();
        QString freq = QString::number(qso.getFrequency().toDouble() / 1000.0, 'f', 6); // kHz to MHz
        QString mode = qso.getMode();
        QString rstSent = qso.getRstSent();
        QString rstRcvd = qso.getRstReceived();

        // Core fields on first line
        out << "<QSO_DATE:8>" << dt.toString("yyyyMMdd")
            << " <TIME_ON:6>" << dt.toString("HHmmss")
            << " <FREQ:" << freq.length() << ">" << freq
            << " <MODE:" << mode.length() << ">" << mode << "\n";

        // Remaining fields indented, one per line
        out << "    <CALL:" << call.length() << ">" << call << "\n";

        if (!rstSent.isEmpty())
            out << "    <RST_SENT:" << rstSent.length() << ">" << rstSent << "\n";
        if (!rstRcvd.isEmpty())
            out << "    <RST_RCVD:" << rstRcvd.length() << ">" << rstRcvd << "\n";

        if (!m_stationCallsign.isEmpty())
            out << "    <STATION_CALLSIGN:" << m_stationCallsign.length() << ">" << m_stationCallsign << "\n";

        // Write exchange fields — map known fields to standard ADIF tags,
        // everything else as APP_CLX_ tags
        QMap<QString, QString> exchFields = qso.getExchangeFields();
        for (auto it = exchFields.constBegin(); it != exchFields.constEnd(); ++it) {
            if (it.value().isEmpty())
                continue;
            QString key = it.key().toUpper();
            QString adifTag;
            if (key == "RSTS" || key == "RSTR" || key == "CALL")
                continue;
            if (key == "SNS")        adifTag = "STX";
            else if (key == "SNR")   adifTag = "SRX";
            else if (key == "NAMER") adifTag = "NAME";
            else if (key == "EXCHR") adifTag = "QTH";
            else if (key == "NOTES") adifTag = "NOTES";
            else if (key == "GRIDR") adifTag = "GRIDSQUARE";
            else if (key == "GRIDS") adifTag = "MY_GRIDSQUARE";

            if (!adifTag.isEmpty())
                out << "    <" << adifTag << ":" << it.value().length() << ">" << it.value() << "\n";
            else
                out << "    <APP_CLX_" << key << ":" << it.value().length() << ">" << it.value() << "\n";
        }

        out << "<EOR>\n";
    }

    qDebug() << "Saved" << qsos.count() << "QSOs to ADIF:" << filename;
    return true;
}
