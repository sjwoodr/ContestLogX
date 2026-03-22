/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "cabrilloFile.h"
#include "../utils/bandPlan.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDate>
#include <QDebug>
#include <QJsonArray>
#include <QRegularExpression>

CabrilloFile::CabrilloFile()
{
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

bool CabrilloFile::load(const QString& filename, QList<QsoRecord>& qsos,
                        const QJsonObject& contestDef)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot open file: " + file.errorString();
        return false;
    }

    QTextStream in(&file);
    qsos.clear();

    // Resolve QSO template from contest definition, or use default RST-based template
    QString qsoTemplate;
    if (!contestDef.isEmpty()
        && contestDef.contains("logging")
        && contestDef["logging"].isObject()) {
        QJsonObject loggingObj = contestDef["logging"].toObject();
        if (loggingObj.contains("cabrillo") && loggingObj["cabrillo"].isObject()) {
            QJsonObject cabrilloObj = loggingObj["cabrillo"].toObject();
            if (cabrilloObj.contains("qsoTemplate"))
                qsoTemplate = cabrilloObj["qsoTemplate"].toString();
        }
    }
    if (qsoTemplate.isEmpty()) {
        // Default template covers most HF contests (RST + exchange)
        qsoTemplate = "QSO: {freq} {mode} {date} {time} {mycall} {rst_sent} {exch_sent} {call} {rst_rcvd} {exch_rcvd}";
    }

    // Parse template into ordered field list (split on whitespace, strip braces)
    QStringList templateTokens = qsoTemplate.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QStringList templateFields;
    for (const QString& tok : templateTokens) {
        if (tok.startsWith('{') && tok.endsWith('}'))
            templateFields.append(tok.mid(1, tok.length() - 2));
        else
            templateFields.append(QString()); // literal / skip
    }

    // Mode mapping: Cabrillo → internal
    QMap<QString, QString> modeMap;
    modeMap["CW"] = "CW";
    modeMap["PH"] = "SSB";
    modeMap["FM"] = "FM";
    modeMap["RY"] = "RTTY";
    modeMap["DG"] = "DIGITAL";

    // VHF band code → representative kHz frequency
    QMap<QString, QString> vhfBandFreq;
    vhfBandFreq["50"]   = "50125";
    vhfBandFreq["70"]   = "70200";
    vhfBandFreq["144"]  = "144200";
    vhfBandFreq["222"]  = "222100";
    vhfBandFreq["432"]  = "432100";
    vhfBandFreq["902"]  = "902100";
    vhfBandFreq["1.2G"] = "1296100";
    vhfBandFreq["2.3G"] = "2304100";
    vhfBandFreq["3.4G"] = "3400100";
    vhfBandFreq["5.7G"] = "5760100";
    vhfBandFreq["10G"]  = "10368100";
    vhfBandFreq["24G"]  = "24192100";
    vhfBandFreq["47G"]  = "47088100";

    QString parsedDate, parsedTime;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Only process QSO lines
        if (!line.startsWith("QSO:", Qt::CaseInsensitive))
            continue;

        QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (tokens.size() < templateFields.size())
            continue; // malformed line

        QsoRecord qso;
        parsedDate.clear();
        parsedTime.clear();

        for (int i = 0; i < templateFields.size() && i < tokens.size(); ++i) {
            const QString& field = templateFields[i];
            const QString& value = tokens[i];

            if (field.isEmpty()) continue; // literal token, skip

            if (field == "freq") {
                QString trimmed = value.trimmed();
                QString freqKhz = vhfBandFreq.contains(trimmed) ? vhfBandFreq[trimmed] : trimmed;
                qso.setFrequency(freqKhz);
                qso.setBandName(BandPlan::freq2Band(freqKhz.toDouble()));
            } else if (field == "mode") {
                QString internalMode = modeMap.value(value.toUpper(), value);
                qso.setMode(internalMode);
            } else if (field == "date") {
                parsedDate = value; // yyyy-MM-dd
            } else if (field == "time") {
                parsedTime = value; // HHmm or HH:mm
                if (parsedDate.isEmpty()) parsedDate = QDate::currentDate().toString("yyyy-MM-dd");
                QString normalizedTime = parsedTime.remove(':');
                if (normalizedTime.length() == 4)
                    normalizedTime += "00"; // append seconds
                QDateTime dt = QDateTime::fromString(parsedDate + normalizedTime, "yyyy-MM-ddHHmmss");
                dt.setTimeSpec(Qt::UTC);
                qso.setDateTime(dt);
            } else if (field == "mycall") {
                // Skip - this is the operator's own callsign
            } else if (field == "call") {
                qso.setCall(value.toUpper());
            } else if (field == "rst_sent") {
                qso.setRstSent(value);
                qso.setExchangeField("RSTs", value);
            } else if (field == "rst_rcvd") {
                qso.setRstReceived(value);
                qso.setExchangeField("RSTr", value);
            } else if (field == "exch_sent") {
                qso.setExchangeField("EXCHs", value);
            } else if (field == "exch_rcvd") {
                qso.setExchangeField("EXCHr", value);
            } else if (field == "name_sent") {
                qso.setExchangeField("NAMEs", value);
            } else if (field == "name_rcvd") {
                qso.setExchangeField("NAMEr", value);
            } else if (field == "GRIDs") {
                qso.setExchangeField("GRIDs", value);
            } else if (field == "GRIDr") {
                qso.setExchangeField("GRIDr", value);
            } else if (field == "serial_sent") {
                qso.setExchangeField("SNs", value);
            } else if (field == "serial_rcvd") {
                qso.setExchangeField("SNr", value);
            } else {
                // Generic: use the field name directly as exchange key
                qso.setExchangeField(field, value);
            }
        }

        if (!qso.getCall().isEmpty())
            qsos.append(qso);
    }

    qDebug() << "Loaded" << qsos.count() << "QSOs from Cabrillo:" << filename;
    return true;
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

bool CabrilloFile::exportToFile(const QString& filename,
                                const QList<QsoRecord>& qsos,
                                const QJsonObject& contestDef,
                                const QJsonObject& headerData,
                                const QString& myCall,
                                const QString& selectedMode)
{
    m_myCall = myCall;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file for writing: %1").arg(filename);
        return false;
    }

    QTextStream stream(&file);

    // Write header
    stream << generateHeader(contestDef, headerData, selectedMode);

    // Get QSO template from logging.cabrillo section
    QString qsoTemplate;
    if (contestDef.contains("logging") && contestDef["logging"].isObject()) {
        QJsonObject loggingObj = contestDef["logging"].toObject();
        if (loggingObj.contains("cabrillo") && loggingObj["cabrillo"].isObject()) {
            QJsonObject cabrilloObj = loggingObj["cabrillo"].toObject();
            if (cabrilloObj.contains("qsoTemplate"))
                qsoTemplate = cabrilloObj["qsoTemplate"].toString();
        }
    }

    if (qsoTemplate.isEmpty()) {
        m_lastError = "No QSO template found in contest definition (expected at logging.cabrillo.qsoTemplate)";
        file.close();
        return false;
    }

    // Write QSOs
    for (const QsoRecord& qso : qsos) {
        if (qso.getPoints() == 0) continue; // Skip invalid QSOs
        stream << generateQsoLine(qso, qsoTemplate) << "\n";
    }

    stream << "END-OF-LOG:\n";
    file.close();
    return true;
}

QString CabrilloFile::generateHeader(const QJsonObject& contestDef,
                                     const QJsonObject& headerData,
                                     const QString& selectedMode)
{
    QString header;
    header += "START-OF-LOG: 3.0\n";

    QJsonArray requiredHeaders;
    if (contestDef.contains("logging") && contestDef["logging"].isObject()) {
        QJsonObject loggingObj = contestDef["logging"].toObject();
        if (loggingObj.contains("cabrillo") && loggingObj["cabrillo"].isObject()) {
            QJsonObject cabrilloObj = loggingObj["cabrillo"].toObject();
            if (cabrilloObj.contains("requiredHeaders"))
                requiredHeaders = cabrilloObj["requiredHeaders"].toArray();

            QString contest = cabrilloObj["contest"].toString();

            if (!selectedMode.isEmpty() && cabrilloObj.contains("contestMapping")) {
                QJsonObject contestMapping = cabrilloObj["contestMapping"].toObject();
                if (contestMapping.contains(selectedMode))
                    contest = contestMapping[selectedMode].toString();
            }

            if (!contest.isEmpty() && isHeaderRequired("CONTEST", requiredHeaders))
                header += QString("CONTEST: %1\n").arg(contest);
        }
    }

    auto addIfRequired = [&](const QString& cabrKey, const QString& value) {
        if (!value.isEmpty() && isHeaderRequired(cabrKey, requiredHeaders))
            header += QString("%1: %2\n").arg(cabrKey, value);
    };

    addIfRequired("CALLSIGN",              headerData["callsign"].toString());
    addIfRequired("OPERATORS",             headerData["operatorName"].toString());
    addIfRequired("CATEGORY",              headerData["category"].toString());
    addIfRequired("CATEGORY-POWER",        headerData["categoryPower"].toString());
    addIfRequired("CATEGORY-MODE",         headerData["categoryMode"].toString());
    addIfRequired("CATEGORY-OPERATOR",     headerData["categoryOperator"].toString());
    addIfRequired("CATEGORY-BAND",         headerData["categoryBand"].toString());
    addIfRequired("CATEGORY-TRANSMITTER",  headerData["categoryTransmitter"].toString());
    addIfRequired("CATEGORY-ASSISTED",     headerData["categoryAssisted"].toString());
    addIfRequired("CATEGORY-OVERLAY",      headerData["categoryOverlay"].toString());
    addIfRequired("CLUB",                  headerData["club"].toString());
    addIfRequired("NAME",                  headerData["name"].toString());
    addIfRequired("ADDRESS",               headerData["address"].toString());
    addIfRequired("ADDRESS-CITY",          headerData["addressCity"].toString());
    addIfRequired("ADDRESS-STATE-PROVINCE",headerData["addressStateProvince"].toString());
    addIfRequired("ADDRESS-POSTALCODE",    headerData["addressPostalcode"].toString());
    addIfRequired("ADDRESS-COUNTRY",       headerData["addressCountry"].toString());
    addIfRequired("LOCATION",              headerData["location"].toString());
    addIfRequired("EMAIL",                 headerData["email"].toString());
    addIfRequired("CREATED-BY",            QString("ContestLogX %1").arg(QApplication::applicationVersion()));
    addIfRequired("CLAIMED-SCORE",         headerData["claimedScore"].toString());
    addIfRequired("SOAPBOX",               headerData["soapbox"].toString());

    header += "\n";
    return header;
}

QString CabrilloFile::generateQsoLine(const QsoRecord& qso, const QString& qsoTemplate)
{
    QString line = qsoTemplate;
    QDateTime dt = qso.getDateTime();

    QString modeStr = qso.getMode().toUpper();
    QString cabrilloMode;
    if (modeStr == "CW")                                                        cabrilloMode = "CW";
    else if (modeStr == "USB" || modeStr == "LSB" || modeStr == "SSB")         cabrilloMode = "PH";
    else if (modeStr == "FM")                                                   cabrilloMode = "FM";
    else if (modeStr == "RTTY")                                                 cabrilloMode = "RY";
    else if (modeStr == "DIGITAL" || modeStr == "DIGI" || modeStr == "FT8"
             || modeStr == "PSK31" || modeStr == "FT4" || modeStr == "JS8")    cabrilloMode = "DG";
    else                                                                        cabrilloMode = modeStr.left(2);

    QString cabrilloFreq = BandPlan::freq2CabrilloBand(qso.getFrequency().toDouble());
    line.replace("{freq}", formatFrequency(cabrilloFreq));
    line.replace("{mode}", cabrilloMode);
    line.replace("{date}", dt.toString("yyyy-MM-dd"));
    line.replace("{time}", dt.toString("HHmm"));
    line.replace("{mycall}", m_myCall);
    line.replace("{rst_sent}", qso.getRstSent());

    QString nameSent = qso.getExchangeField("NAMEs");
    QString exchSent = qso.getExchangeField("EXCHs");
    if (!nameSent.isEmpty() && !exchSent.isEmpty()) {
        line.replace("{name_sent}", nameSent);
        line.replace("{exch_sent}", exchSent);
        line.replace("{exch_sent}", nameSent + " " + exchSent);
    } else {
        line.replace("{exch_sent}", qso.getExchangeSent());
        line.replace("{name_sent}", "");
    }

    line.replace("{CATs}", qso.getExchangeField("CATs"));
    line.replace("{LOCs}", qso.getExchangeField("LOCs"));
    line.replace("{GRIDs}", qso.getExchangeField("GRIDs"));
    line.replace("{GRIDr}", qso.getExchangeField("GRIDr"));
    line.replace("{call}", qso.getCall());
    line.replace("{rst_rcvd}", qso.getRstReceived());

    QString nameRcvd = qso.getExchangeField("NAMEr");
    QString exchRcvd = qso.getExchangeField("EXCHr");
    if (!nameRcvd.isEmpty() && !exchRcvd.isEmpty()) {
        line.replace("{name_rcvd}", nameRcvd);
        line.replace("{exch_rcvd}", exchRcvd);
        line.replace("{exch_rcvd}", nameRcvd + " " + exchRcvd);
    } else {
        line.replace("{exch_rcvd}", qso.getExchangeReceived());
        line.replace("{name_rcvd}", "");
    }

    line.replace("{CATr}", qso.getExchangeField("CATr"));
    line.replace("{LOCr}", qso.getExchangeField("LOCr"));

    return line;
}

QString CabrilloFile::formatFrequency(double freqKhz)
{
    return QString("%1").arg(static_cast<int>(freqKhz), 5, 10, QChar(' '));
}

QString CabrilloFile::formatFrequency(const QString& freq)
{
    return QString("%1").arg(freq, 5);
}

bool CabrilloFile::isHeaderRequired(const QString& headerName, const QJsonArray& requiredHeaders)
{
    if (requiredHeaders.isEmpty())
        return true;
    for (const QJsonValue& val : requiredHeaders) {
        if (val.toString() == headerName)
            return true;
    }
    return false;
}
