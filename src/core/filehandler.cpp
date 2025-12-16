/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "filehandler.h"
#include "clxfile.h"
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
    
    if (ext == "wl2") {
        return loadWl2(filename, qsos);
    } else if (ext == "csv") {
        return loadCsv(filename, qsos);
    } else if (ext == "adi" || ext == "adif") {
        return loadAdif(filename, qsos);
    } else if (ext == "wl") {
        return loadWl(filename, qsos);
    } else {
        // Default to WL2
        return loadWl2(filename, qsos);
    }
}

bool FileHandler::save(const QString& filename, const QList<QsoRecord>& qsos)
{
    QFileInfo fileInfo(filename);
    QString ext = fileInfo.suffix().toLower();
    
    if (ext == "wl2") {
        return saveWl2(filename, qsos);
    } else if (ext == "csv") {
        return saveCsv(filename, qsos);
    } else if (ext == "adi" || ext == "adif") {
        return saveAdif(filename, qsos);
    } else if (ext == "wl") {
        return saveWl(filename, qsos);
    } else {
        // Default to WL2
        return saveWl2(filename, qsos);
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
        
        // Extract CALL
        QRegularExpression callRx("<call:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch callMatch = callRx.match(record);
        if (callMatch.hasMatch()) {
            qso.setCall(callMatch.captured(2).trimmed());
        }
        
        // Extract QSO_DATE and TIME_ON
        QRegularExpression dateRx("<qso_date:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpression timeRx("<time_on:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch dateMatch = dateRx.match(record);
        QRegularExpressionMatch timeMatch = timeRx.match(record);
        if (dateMatch.hasMatch() && timeMatch.hasMatch()) {
            QString date = dateMatch.captured(2).trimmed();  // YYYYMMDD
            QString time = timeMatch.captured(2).trimmed();  // HHMMSS
            QDateTime dt = QDateTime::fromString(date + time, "yyyyMMddHHmmss");
            dt.setTimeSpec(Qt::UTC);
            qso.setDateTime(dt);
        }
        
        // Extract FREQ (MHz to kHz)
        QRegularExpression freqRx("<freq:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch freqMatch = freqRx.match(record);
        if (freqMatch.hasMatch()) {
            double freqMHz = freqMatch.captured(2).toDouble();
            qso.setFrequency(QString::number(freqMHz * 1000.0, 'f', 1));
        }
        
        // Extract MODE
        QRegularExpression modeRx("<mode:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch modeMatch = modeRx.match(record);
        if (modeMatch.hasMatch()) {
            qso.setMode(modeMatch.captured(2).trimmed());
        }
        
        // Extract RST_SENT + RST_RCVD + other exchange
        QRegularExpression rstRcvdRx("<rst_rcvd:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpression stxRx("<stx:(\\d+)>([^<]+)", QRegularExpression::CaseInsensitiveOption);
        
        QString exchange;
        QRegularExpressionMatch rstRcvdMatch = rstRcvdRx.match(record);
        if (rstRcvdMatch.hasMatch()) {
            exchange = rstRcvdMatch.captured(2).trimmed();
        }
        QRegularExpressionMatch stxMatch = stxRx.match(record);
        if (stxMatch.hasMatch()) {
            if (!exchange.isEmpty()) exchange += " ";
            exchange += stxMatch.captured(2).trimmed();
        }
        qso.setExchange(exchange);
        
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
    out << "<PROGRAMVERSION:8>12.0.0\n";
    out << "<EOH>\n\n";
    
    // Write QSOs
    for (const QsoRecord& qso : qsos) {
        QDateTime dt = qso.getDateTime();
        QString call = qso.getCall();
        QString freq = QString::number(qso.getFrequency().toDouble() / 1000.0, 'f', 6); // kHz to MHz
        QString mode = qso.getMode();
        QString exchange = qso.getExchange();
        
        out << "<CALL:" << call.length() << ">" << call << " ";
        out << "<QSO_DATE:8>" << dt.toString("yyyyMMdd") << " ";
        out << "<TIME_ON:6>" << dt.toString("HHmmss") << " ";
        out << "<FREQ:" << freq.length() << ">" << freq << " ";
        out << "<MODE:" << mode.length() << ">" << mode << " ";
        out << "<RST_SENT:3>599 ";
        out << "<RST_RCVD:" << exchange.length() << ">" << exchange << " ";
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

// WL2 Format (JSON-based)
bool FileHandler::loadWl2(const QString& filename, QList<QsoRecord>& qsos)
{
    Wl2File wl2File;
    if (!wl2File.load(filename)) {
        m_lastError = wl2File.lastError();
        return false;
    }
    
    qsos = wl2File.qsos();
    return true;
}

bool FileHandler::loadWl2WithContest(const QString& filename, QList<QsoRecord>& qsos, QString& contestFile, QString& stationClass, QString& contestVersion)
{
    Wl2File wl2File;
    if (!wl2File.load(filename)) {
        m_lastError = wl2File.lastError();
        return false;
    }
    
    qsos = wl2File.qsos();
    contestFile = wl2File.contest().contestFile();
    stationClass = wl2File.contest().category("station_class");
    contestVersion = wl2File.contest().contestVersion();
    return true;
}

bool FileHandler::saveWl2(const QString& filename, const QList<QsoRecord>& qsos)
{
    Wl2File wl2File;
    for (const QsoRecord& qso : qsos) {
        wl2File.addQso(qso);
    }
    
    if (!wl2File.save(filename)) {
        m_lastError = wl2File.lastError();
        return false;
    }
    
    return true;
}

bool FileHandler::saveWl2WithContest(const QString& filename, const QList<QsoRecord>& qsos, const QString& contestFile, const QJsonObject& contestDef, const QString& stationClass)
{
    Wl2File wl2File;
    
    // Set contest info
    if (!contestDef.isEmpty()) {
        QString contestName = contestDef["name"].toString();
        if (contestName.isEmpty()) {
            contestName = "General DXCC Logging";
        }
        wl2File.contest().setName(contestName);
        wl2File.contest().setType(contestName);
        wl2File.contest().setContestFile(contestFile);
        
        // Save contest version if available
        if (contestDef.contains("contest")) {
            QJsonObject contestObj = contestDef["contest"].toObject();
            if (contestObj.contains("version")) {
                wl2File.contest().setContestVersion(contestObj["version"].toString());
            }
        }
        
        // Add station class if provided
        if (!stationClass.isEmpty()) {
            wl2File.contest().setCategory("station_class", stationClass);
        }
    }
    
    // Add QSOs
    for (const QsoRecord& qso : qsos) {
        wl2File.addQso(qso);
    }
    
    if (!wl2File.save(filename)) {
        m_lastError = wl2File.lastError();
        return false;
    }
    
    return true;
}
