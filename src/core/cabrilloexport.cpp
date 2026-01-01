#include "cabrilloexport.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

CabrilloExport::CabrilloExport()
{
}

bool CabrilloExport::exportToFile(const QString& filename,
                                   const QList<QsoRecord>& qsos,
                                   const QJsonObject& contestDef,
                                   const QJsonObject& headerData,
                                   const QString& myCall)
{
    m_myCall = myCall;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file for writing: %1").arg(filename);
        return false;
    }
    
    QTextStream stream(&file);
    
    // Write header
    QString header = generateHeader(contestDef, headerData);
    stream << header;
    
    // Get QSO template from logging.cabrillo section
    QString qsoTemplate;
    if (contestDef.contains("logging") && contestDef["logging"].isObject()) {
        QJsonObject loggingObj = contestDef["logging"].toObject();
        if (loggingObj.contains("cabrillo") && loggingObj["cabrillo"].isObject()) {
            QJsonObject cabrilloObj = loggingObj["cabrillo"].toObject();
            if (cabrilloObj.contains("qsoTemplate")) {
                qsoTemplate = cabrilloObj["qsoTemplate"].toString();
            }
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
        QString qsoLine = generateQsoLine(qso, qsoTemplate);
        stream << qsoLine << "\n";
    }
    
    // Write end marker
    stream << "END-OF-LOG:\n";
    
    file.close();
    return true;
}

QString CabrilloExport::generateHeader(const QJsonObject& contestDef, const QJsonObject& headerData)
{
    QString header;
    header += "START-OF-LOG: 3.0\n";
    
    // Get required headers from contest definition
    QJsonArray requiredHeaders;
    if (contestDef.contains("logging") && contestDef["logging"].isObject()) {
        QJsonObject loggingObj = contestDef["logging"].toObject();
        if (loggingObj.contains("cabrillo") && loggingObj["cabrillo"].isObject()) {
            QJsonObject cabrilloObj = loggingObj["cabrillo"].toObject();
            if (cabrilloObj.contains("requiredHeaders")) {
                requiredHeaders = cabrilloObj["requiredHeaders"].toArray();
            }
            // Contest
            QString contest = cabrilloObj["contest"].toString();
            if (!contest.isEmpty() && isHeaderRequired("CONTEST", requiredHeaders)) {
                header += QString("CONTEST: %1\n").arg(contest);
            }
        }
    }
    
    // Helper lambda to add header if required
    auto addIfRequired = [&](const QString& key, const QString& cabrKey, const QString& value) {
        if (!value.isEmpty() && isHeaderRequired(cabrKey, requiredHeaders)) {
            header += QString("%1: %2\n").arg(cabrKey, value);
        }
    };
    
    // Add headers based on requirements
    addIfRequired("callsign", "CALLSIGN", headerData["callsign"].toString());
    addIfRequired("operatorName", "OPERATORS", headerData["operatorName"].toString());
    addIfRequired("category", "CATEGORY", headerData["category"].toString());
    addIfRequired("categoryPower", "CATEGORY-POWER", headerData["categoryPower"].toString());
    addIfRequired("categoryMode", "CATEGORY-MODE", headerData["categoryMode"].toString());
    addIfRequired("categoryOperator", "CATEGORY-OPERATOR", headerData["categoryOperator"].toString());
    addIfRequired("categoryBand", "CATEGORY-BAND", headerData["categoryBand"].toString());
    addIfRequired("categoryTransmitter", "CATEGORY-TRANSMITTER", headerData["categoryTransmitter"].toString());
    addIfRequired("categoryAssisted", "CATEGORY-ASSISTED", headerData["categoryAssisted"].toString());
    addIfRequired("categoryOverlay", "CATEGORY-OVERLAY", headerData["categoryOverlay"].toString());
    addIfRequired("club", "CLUB", headerData["club"].toString());
    addIfRequired("name", "NAME", headerData["name"].toString());
    addIfRequired("address", "ADDRESS", headerData["address"].toString());
    addIfRequired("addressCity", "ADDRESS-CITY", headerData["addressCity"].toString());
    addIfRequired("addressStateProvince", "ADDRESS-STATE-PROVINCE", headerData["addressStateProvince"].toString());
    addIfRequired("addressPostalcode", "ADDRESS-POSTALCODE", headerData["addressPostalcode"].toString());
    addIfRequired("addressCountry", "ADDRESS-COUNTRY", headerData["addressCountry"].toString());
    addIfRequired("location", "LOCATION", headerData["location"].toString());
    addIfRequired("email", "EMAIL", headerData["email"].toString());
    
    // CREATED-BY is auto-generated - get from the app version
    QString createdBy = QString("ContestLogX %1").arg("0.0.3");
    addIfRequired("createdBy", "CREATED-BY", createdBy);
    
    addIfRequired("claimedScore", "CLAIMED-SCORE", headerData["claimedScore"].toString());
    addIfRequired("soapbox", "SOAPBOX", headerData["soapbox"].toString());
    
    header += "\n";
    return header;
}

QString CabrilloExport::generateQsoLine(const QsoRecord& qso, const QString& qsoTemplate)
{
    QString line = qsoTemplate;
    QDateTime dt = qso.getDateTime();
    
    // Map application modes to Cabrillo modes
    QString modeStr = qso.getMode().toUpper();
    QString cabrilloMode;
    if (modeStr == "CW") {
        cabrilloMode = "CW";
    } else if (modeStr == "USB" || modeStr == "LSB" || modeStr == "SSB") {
        cabrilloMode = "PH";
    } else if (modeStr == "RTTY") {
        cabrilloMode = "RY";
    } else if (modeStr == "FT8" || modeStr == "PSK31" || modeStr == "FT4" || modeStr == "JS8" || modeStr == "DIGI") {
        cabrilloMode = "RY";  // Digital modes use RY
    } else {
        cabrilloMode = modeStr;  // Use as-is if unknown
    }
    
    // Replace template variables
    line.replace("{freq}", formatFrequency(qso.getFrequency().toDouble()));
    line.replace("{mode}", cabrilloMode);
    line.replace("{date}", dt.toString("yyyy-MM-dd"));
    line.replace("{time}", dt.toString("HHmm"));
    line.replace("{mycall}", m_myCall);  // My station's callsign
    line.replace("{rst_sent}", qso.getRstSent());
    
    // Handle exchange fields - support both combined and split formats
    QString nameSent = qso.getExchangeField("NAMEs");
    QString exchSent = qso.getExchangeField("EXCHs");
    
    if (!nameSent.isEmpty() && !exchSent.isEmpty()) {
        // Split fields available - use them
        line.replace("{name_sent}", nameSent);
        line.replace("{exch_sent}", exchSent);
        // Also support combined format for legacy templates
        line.replace("{exch_sent}", nameSent + " " + exchSent);
    } else {
        // Fall back to combined exchange
        line.replace("{exch_sent}", qso.getExchangeSent());
        line.replace("{name_sent}", "");
    }
    
    line.replace("{call}", qso.getCall());  // The other station's callsign
    line.replace("{rst_rcvd}", qso.getRstReceived());
    
    // Handle received exchange fields
    QString nameRcvd = qso.getExchangeField("NAMEr");
    QString exchRcvd = qso.getExchangeField("EXCHr");
    
    if (!nameRcvd.isEmpty() && !exchRcvd.isEmpty()) {
        // Split fields available - use them
        line.replace("{name_rcvd}", nameRcvd);
        line.replace("{exch_rcvd}", exchRcvd);
        // Also support combined format for legacy templates
        line.replace("{exch_rcvd}", nameRcvd + " " + exchRcvd);
    } else {
        // Fall back to combined exchange
        line.replace("{exch_rcvd}", qso.getExchangeReceived());
        line.replace("{name_rcvd}", "");
    }
    
    return line;
}

QString CabrilloExport::formatFrequency(double freqKhz)
{
    // Cabrillo format is "kHz" as integer, right-aligned in 5 chars
    return QString("%1").arg(static_cast<int>(freqKhz), 5, 10, QChar(' '));
}

bool CabrilloExport::isHeaderRequired(const QString& headerName, const QJsonArray& requiredHeaders)
{
    if (requiredHeaders.isEmpty()) {
        return true; // If no required headers specified, include everything
    }
    
    for (const QJsonValue& val : requiredHeaders) {
        if (val.toString() == headerName) {
            return true;
        }
    }
    return false;
}