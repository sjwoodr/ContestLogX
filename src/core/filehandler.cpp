/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "filehandler.h"
#include "clxfile.h"
#include "stationinfo.h"
#include "debuglogger.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDataStream>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>
#include <cstring>

FileHandler::FileHandler()
{
}

QString FileHandler::formatDateTime(const QDateTime& dt) const
{
    return dt.toUTC().toString("yyyy-MM-dd HH:mm:ss");
}

QDateTime FileHandler::parseDateTime(const QString& str) const
{
    QDateTime dt = QDateTime::fromString(str, "yyyy-MM-dd HH:mm:ss");
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

bool FileHandler::load(const QString& filename, QList<QsoRecord>& qsos)
{
    QFileInfo fileInfo(filename);
    QString ext = fileInfo.suffix().toLower();
    
    if (ext == "clx") {
        return loadClx(filename, qsos);
    } else if (ext == "csv") {
        return loadCsv(filename, qsos);
    } else if (ext == "adi" || ext == "adif") {
        return loadAdif(filename, qsos);
    } else if (ext == "wl") {
        return loadWl(filename, qsos);
    } else {
        // Default to CLX
        return loadClx(filename, qsos);
    }
}

bool FileHandler::save(const QString& filename, const QList<QsoRecord>& qsos)
{
    QFileInfo fileInfo(filename);
    QString ext = fileInfo.suffix().toLower();
    
    if (ext == "clx") {
        return saveClx(filename, qsos);
    } else if (ext == "csv") {
        return saveCsv(filename, qsos);
    } else if (ext == "adi" || ext == "adif") {
        return saveAdif(filename, qsos);
    } else if (ext == "wl") {
        return saveWl(filename, qsos);
    } else {
        // Default to CLX
        return saveClx(filename, qsos);
    }
}

// CSV Format Implementation
bool FileHandler::loadCsv(const QString& filename, QList<QsoRecord>& qsos)
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
        
        if (fields.count() > 7) {
            qso.setDupe(fields[7] == "D");
        }
        
        qsos.append(qso);
    }
    
    qDebug() << "Loaded" << qsos.count() << "QSOs from CSV:" << filename;
    return true;
}

bool FileHandler::saveCsv(const QString& filename, const QList<QsoRecord>& qsos)
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

// ADIF Format Implementation
bool FileHandler::loadAdif(const QString& filename, QList<QsoRecord>& qsos)
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

bool FileHandler::saveAdif(const QString& filename, const QList<QsoRecord>& qsos)
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
    out << "<PROGRAMID:5>ContestLogX\n";
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
        QString exchange = qso.getExchange();
        
        // Core fields on first line
        out << "<QSO_DATE:8>" << dt.toString("yyyyMMdd")
            << " <TIME_ON:6>" << dt.toString("HHmmss")
            << " <FREQ:" << freq.length() << ">" << freq
            << " <MODE:" << mode.length() << ">" << mode << "\n";

        // Remaining fields indented, one per line
        out << "    <CALL:" << call.length() << ">" << call << "\n";

        if (!rstSent.isEmpty()) {
            out << "    <RST_SENT:" << rstSent.length() << ">" << rstSent << "\n";
        }
        if (!rstRcvd.isEmpty()) {
            out << "    <RST_RCVD:" << rstRcvd.length() << ">" << rstRcvd << "\n";
        }

        if (!m_stationCallsign.isEmpty()) {
            out << "    <STATION_CALLSIGN:" << m_stationCallsign.length() << ">" << m_stationCallsign << "\n";
        }

        // Write exchange fields — map known fields to standard ADIF tags,
        // everything else as APP_CLX_ tags
        QMap<QString, QString> exchFields = qso.getExchangeFields();
        for (auto it = exchFields.constBegin(); it != exchFields.constEnd(); ++it) {
            if (it.value().isEmpty())
                continue;
            QString key = it.key().toUpper();
            QString adifTag;
            // Skip fields already written as standard ADIF tags above
            if (key == "RSTS" || key == "RSTR" || key == "CALL")
                continue;
            if (key == "SNS")           adifTag = "STX";
            else if (key == "SNR")      adifTag = "SRX";
            else if (key == "NAMER")    adifTag = "NAME";
            else if (key == "EXCHR")    adifTag = "QTH";
            else if (key == "NOTES")    adifTag = "NOTES";
            else if (key == "GRIDR")    adifTag = "GRIDSQUARE";
            else if (key == "GRIDS")    adifTag = "MY_GRIDSQUARE";

            if (!adifTag.isEmpty()) {
                out << "    <" << adifTag << ":" << it.value().length() << ">" << it.value() << "\n";
            } else {
                out << "    <APP_CLX_" << key << ":" << it.value().length() << ">" << it.value() << "\n";
            }
        }

        out << "<EOR>\n";
    }
    
    qDebug() << "Saved" << qsos.count() << "QSOs to ADIF:" << filename;
    return true;
}

// ContestLogX Binary Format Implementation
bool FileHandler::loadWl(const QString& filename, QList<QsoRecord>& qsos)
{
    Q_UNUSED(filename);
    Q_UNUSED(qsos);
    m_lastError = "Legacy .wl binary format is not supported. "
                  "Try exporting as ADIF from Windows ContestLogX.";
    return false;
}

bool FileHandler::saveWl(const QString& filename, const QList<QsoRecord>& qsos)
{
    Q_UNUSED(filename);
    Q_UNUSED(qsos);
    m_lastError = "Legacy .wl binary format is not supported for saving. "
                  "Use .clx format instead.";
    return false;
}

// CLX Format (JSON-based)
bool FileHandler::loadClx(const QString& filename, QList<QsoRecord>& qsos)
{
    ClxFile clxFile;
    if (!clxFile.load(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    qsos = clxFile.qsos();
    return true;
}

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion)
{
    QString dummy;
    return loadClxWithContest(filename, qsos, contestFile, stationClass, contestVersion, dummy);
}

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion, QString& stationClassExchange)
{
    ClxFile clxFile;
    if (!clxFile.load(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    qsos = clxFile.qsos();
    contestFile = clxFile.contest().contestFile();
    stationClass = clxFile.contest().category("station_class");
    contestVersion = clxFile.contest().contestVersion();
    stationClassExchange = clxFile.contest().category("station_class_exchange");
    return true;
}

bool FileHandler::saveClx(const QString& filename, const QList<QsoRecord>& qsos)
{
    ClxFile clxFile;
    for (const QsoRecord& qso : qsos) {
        clxFile.addQso(qso);
    }
    
    if (!clxFile.save(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchange)
{
    ClxFile clxFile;
    
    // Set contest info
    if (!contestDef.isEmpty()) {
        QString contestName = "General DXCC Logging";
        
        // Try to get contest name from nested contest object
        if (contestDef.contains("contest")) {
            QJsonObject contestObj = contestDef["contest"].toObject();
            if (contestObj.contains("name")) {
                contestName = contestObj["name"].toString();
            }
            // Save contest version if available
            if (contestObj.contains("version")) {
                clxFile.contest().setContestVersion(contestObj["version"].toString());
            }
        }
        
        // Fall back to top-level name if nested one not found
        if (contestName == "General DXCC Logging" && contestDef.contains("name")) {
            QString topLevelName = contestDef["name"].toString();
            if (!topLevelName.isEmpty()) {
                contestName = topLevelName;
            }
        }
        
        clxFile.contest().setName(contestName);
        clxFile.contest().setType(contestName);
        clxFile.contest().setContestFile(contestFile);
        
        // Add station class if provided
        if (!stationClass.isEmpty()) {
            clxFile.contest().setCategory("station_class", stationClass);
        }
        
        // Add station class exchange data if provided
        if (!stationClassExchange.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange", stationClassExchange);
        }
    }
    
    // Add QSOs
    for (const QsoRecord& qso : qsos) {
        clxFile.addQso(qso);
    }
    
    if (!clxFile.save(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchangeName, const QString& stationClassExchangeId)
{
    ClxFile clxFile;
    
    // Set contest info
    if (!contestDef.isEmpty()) {
        QString contestName = "General DXCC Logging";
        
        // Try to get contest name from nested contest object
        if (contestDef.contains("contest")) {
            QJsonObject contestObj = contestDef["contest"].toObject();
            if (contestObj.contains("name")) {
                contestName = contestObj["name"].toString();
            }
            // Save contest version if available
            if (contestObj.contains("version")) {
                clxFile.contest().setContestVersion(contestObj["version"].toString());
            }
        }
        
        // Fall back to top-level name if nested one not found
        if (contestName == "General DXCC Logging" && contestDef.contains("name")) {
            QString topLevelName = contestDef["name"].toString();
            if (!topLevelName.isEmpty()) {
                contestName = topLevelName;
            }
        }
        
        clxFile.contest().setName(contestName);
        clxFile.contest().setType(contestName);
        clxFile.contest().setContestFile(contestFile);
        
        // Add station class if provided
        if (!stationClass.isEmpty()) {
            clxFile.contest().setCategory("station_class", stationClass);
        }
        
        // Add station class exchange data - name and ID separately
        if (!stationClassExchangeName.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange_name", stationClassExchangeName);
        }
        if (!stationClassExchangeId.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange_id", stationClassExchangeId);
        }
    }
    
    // Add QSOs
    for (const QsoRecord& qso : qsos) {
        clxFile.addQso(qso);
    }
    
    if (!clxFile.save(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    return true;
}

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion, QString& stationClassExchangeName, QString& stationClassExchangeId)
{
    ClxFile clxFile;
    if (!clxFile.load(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    qsos = clxFile.qsos();
    contestFile = clxFile.contest().contestFile();
    stationClass = clxFile.contest().category("station_class");
    contestVersion = clxFile.contest().contestVersion();
    
    // Load name and exchange separately
    stationClassExchangeName = clxFile.contest().category("station_class_exchange_name");
    stationClassExchangeId = clxFile.contest().category("station_class_exchange_id");
    
    // Fallback: if separate fields not found, try loading combined field and split it
    if (stationClassExchangeName.isEmpty() && stationClassExchangeId.isEmpty()) {
        QString combined = clxFile.contest().category("station_class_exchange");
        if (!combined.isEmpty()) {
            int spacePos = combined.indexOf(' ');
            if (spacePos > 0) {
                stationClassExchangeName = combined.left(spacePos).toUpper();
                stationClassExchangeId = combined.mid(spacePos + 1).toUpper();
            } else {
                stationClassExchangeName = combined.toUpper();
            }
        }
    }
    
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchangeName, const QString& stationClassExchangeId, const QMap<QString, QString>& userPromptValues)
{
    ClxFile clxFile;
    
    // Set contest info
    if (!contestDef.isEmpty()) {
        QString contestName = "General DXCC Logging";
        
        // Try to get contest name from nested contest object
        if (contestDef.contains("contest")) {
            QJsonObject contestObj = contestDef["contest"].toObject();
            if (contestObj.contains("name")) {
                contestName = contestObj["name"].toString();
            }
            // Save contest version if available
            if (contestObj.contains("version")) {
                clxFile.contest().setContestVersion(contestObj["version"].toString());
            }
        }
        
        // Fall back to top-level name if nested one not found
        if (contestName == "General DXCC Logging" && contestDef.contains("name")) {
            QString topLevelName = contestDef["name"].toString();
            if (!topLevelName.isEmpty()) {
                contestName = topLevelName;
            }
        }
        
        clxFile.contest().setName(contestName);
        clxFile.contest().setType(contestName);
        clxFile.contest().setContestFile(contestFile);
        
        // Add station class if provided
        if (!stationClass.isEmpty()) {
            clxFile.contest().setCategory("station_class", stationClass);
        }
        
        // Add station class exchange data - name and ID separately
        if (!stationClassExchangeName.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange_name", stationClassExchangeName);
        }
        if (!stationClassExchangeId.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange_id", stationClassExchangeId);
        }
        
        // Add user prompt values (e.g., contestMonth, gridSquare, etc.)
        if (userPromptValues.isEmpty()) {
            DebugLogger::instance().log("FileHandler", "No userPromptValues to save");
        } else {
            DebugLogger::instance().log("FileHandler", 
                QString("Saving %1 userPromptValues to CLX file").arg(userPromptValues.size()));
            for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it) {
                if (!it.value().isEmpty()) {
                    DebugLogger::instance().log("FileHandler", 
                        QString("  Saving userPrompt %1 = '%2'").arg(it.key(), it.value()));
                    clxFile.contest().setCategory("userPrompt_" + it.key(), it.value());
                }
            }
        }
    }
    
    // Add QSOs
    for (const QsoRecord& qso : qsos) {
        clxFile.addQso(qso);
    }
    
    if (!clxFile.save(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass, const QString& stationClassExchangeName, const QString& stationClassExchangeId, const QMap<QString, QString>& userPromptValues, const StationInfo& stationInfo)
{
    ClxFile clxFile;

    // Set station info from parameter
    clxFile.station() = stationInfo;

    // Set contest info
    if (!contestDef.isEmpty()) {
        QString contestName = "General DXCC Logging";

        // Try to get contest name from nested contest object
        if (contestDef.contains("contest")) {
            QJsonObject contestObj = contestDef["contest"].toObject();
            if (contestObj.contains("name")) {
                contestName = contestObj["name"].toString();
            }
            // Save contest version if available
            if (contestObj.contains("version")) {
                clxFile.contest().setContestVersion(contestObj["version"].toString());
            }
        }

        // Fall back to top-level name if nested one not found
        if (contestName == "General DXCC Logging" && contestDef.contains("name")) {
            QString topLevelName = contestDef["name"].toString();
            if (!topLevelName.isEmpty()) {
                contestName = topLevelName;
            }
        }

        clxFile.contest().setName(contestName);
        clxFile.contest().setType(contestName);
        clxFile.contest().setContestFile(contestFile);

        // Add station class if provided
        if (!stationClass.isEmpty()) {
            clxFile.contest().setCategory("station_class", stationClass);
        }

        // Add station class exchange data - name and ID separately
        if (!stationClassExchangeName.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange_name", stationClassExchangeName);
        }
        if (!stationClassExchangeId.isEmpty()) {
            clxFile.contest().setCategory("station_class_exchange_id", stationClassExchangeId);
        }

        // Add user prompt values (e.g., contestMonth, gridSquare, etc.)
        if (userPromptValues.isEmpty()) {
            DebugLogger::instance().log("FileHandler", "No userPromptValues to save");
        } else {
            DebugLogger::instance().log("FileHandler",
                QString("Saving %1 userPromptValues to CLX file").arg(userPromptValues.size()));
            for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it) {
                if (!it.value().isEmpty()) {
                    DebugLogger::instance().log("FileHandler",
                        QString("  Saving userPrompt %1 = '%2'").arg(it.key(), it.value()));
                    clxFile.contest().setCategory("userPrompt_" + it.key(), it.value());
                }
            }
        }
    }

    // Contest-specific memories
    if (m_useContestMemories) {
        clxFile.setUseContestMemories(true);
        clxFile.setCwMemories(m_contestCwMemories);
        clxFile.setSsbMemories(m_contestSsbMemories);
    }

    // Add QSOs
    for (const QsoRecord& qso : qsos) {
        clxFile.addQso(qso);
    }

    if (!clxFile.save(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }

    return true;
}

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion, QString& stationClassExchangeName, QString& stationClassExchangeId, QMap<QString, QString>& userPromptValues)
{
    ClxFile clxFile;
    if (!clxFile.load(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    
    qsos = clxFile.qsos();
    contestFile = clxFile.contest().contestFile();
    stationClass = clxFile.contest().category("station_class");
    contestVersion = clxFile.contest().contestVersion();
    
    // Load name and exchange separately
    stationClassExchangeName = clxFile.contest().category("station_class_exchange_name");
    stationClassExchangeId = clxFile.contest().category("station_class_exchange_id");
    
    // Fallback: if separate fields not found, try loading combined field and split it
    if (stationClassExchangeName.isEmpty() && stationClassExchangeId.isEmpty()) {
        QString combined = clxFile.contest().category("station_class_exchange");
        if (!combined.isEmpty()) {
            int spacePos = combined.indexOf(' ');
            if (spacePos > 0) {
                stationClassExchangeName = combined.left(spacePos).toUpper();
                stationClassExchangeId = combined.mid(spacePos + 1).toUpper();
            } else {
                stationClassExchangeName = combined.toUpper();
            }
        }
    }
    
    // Load user prompt values from categories
    QMap<QString, QString> categories = clxFile.contest().categories();
    for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
        if (it.key().startsWith("userPrompt_")) {
            QString promptId = it.key().mid(11);  // Remove "userPrompt_" prefix
            userPromptValues[promptId] = it.value();
        }
    }
    
    return true;
}
