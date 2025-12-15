#include "contestengine.h"
#include "debuglogger.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QtMath>

ContestEngine::ContestEngine(QObject *parent)
    : QObject(parent)
    , m_dxccDatabase(nullptr)
{
}

ContestEngine::~ContestEngine()
{
}

bool ContestEngine::loadContest(const QJsonObject& contestDef)
{
    m_contestDef = contestDef;
    
    // Load multipliers into sets for fast lookup
    m_validMultipliers.clear();
    m_validStates.clear();
    m_validProvinces.clear();
    
    // Try to load from validation.exchangeValidation.multipliers
    if (contestDef.contains("validation")) {
        QJsonObject validation = contestDef["validation"].toObject();
        if (validation.contains("exchangeValidation")) {
            QJsonObject exchVal = validation["exchangeValidation"].toObject();
            if (exchVal.contains("multipliers")) {
                QJsonArray multList = exchVal["multipliers"].toArray();
                for (const QJsonValue& val : multList) {
                    QString mult = val.toString().toUpper();
                    m_validMultipliers.insert(mult);
                    
                    // Also add to states or provinces based on length/pattern
                    if (mult.length() == 2) {
                        m_validStates.insert(mult);
                    } else {
                        m_validProvinces.insert(mult);
                    }
                }
            }
        }
    }
    
    DebugLogger::instance().log("ContestEngine", 
                     QString("Contest loaded: %1, Multipliers: %2")
                     .arg(getContestName())
                     .arg(m_validMultipliers.size()));
    
    return true;
}

QString ContestEngine::getContestName() const
{
    if (m_contestDef.contains("contest")) {
        QJsonObject contest = m_contestDef["contest"].toObject();
        return contest["name"].toString();
    }
    return m_contestDef["name"].toString();
}

QStringList ContestEngine::getExchangeFields() const
{
    QStringList fields;
    if (m_contestDef.contains("exchangeFields")) {
        QJsonObject exchangeFields = m_contestDef["exchangeFields"].toObject();
        
        // Get sent fields
        if (exchangeFields.contains("sent")) {
            QJsonArray sentFields = exchangeFields["sent"].toArray();
            for (const QJsonValue& val : sentFields) {
                QJsonObject fieldObj = val.toObject();
                QString fieldName = fieldObj["name"].toString();
                if (!fieldName.isEmpty()) {
                    fields.append(fieldName);
                }
            }
        }
        
        // Get received fields
        if (exchangeFields.contains("received")) {
            QJsonArray recvFields = exchangeFields["received"].toArray();
            for (const QJsonValue& val : recvFields) {
                QJsonObject fieldObj = val.toObject();
                QString fieldName = fieldObj["name"].toString();
                if (!fieldName.isEmpty() && !fields.contains(fieldName)) {
                    fields.append(fieldName);
                }
            }
        }
    }
    return fields;
}

QStringList ContestEngine::getLogColumns() const
{
    QStringList columns;
    columns << "SEQ" << "DATE" << "TIME" << "FREQ" << "CALL" 
            << "SNT" << "RCV" << "QTH" << "M" << "CW" << "PHO" << "P" << "COUNTRY" << "C" << "PREF" << "TIME-OFF";
    
    // Add exchange fields
    QStringList exchFields = getExchangeFields();
    for (const QString& field : exchFields) {
        if (!columns.contains(field.toUpper())) {
            columns.append(field.toUpper());
        }
    }
    
    return columns;
}

QString ContestEngine::getFieldLabel(const QString& fieldName) const
{
    // Map field names to user-friendly labels
    static QMap<QString, QString> labels = {
        {"rst_sent", "RST Sent"},
        {"rst_received", "RST Rcvd"},
        {"serial_sent", "Serial Sent"},
        {"serial_received", "Serial Rcvd"},
        {"state", "State"},
        {"province", "Province"},
        {"grid_square", "Grid"},
        {"name", "Name"},
        {"exchange", "Exchange"}
    };
    
    return labels.value(fieldName, fieldName);
}

QString ContestEngine::getFieldType(const QString& fieldName) const
{
    if (m_contestDef.contains("exchange")) {
        QJsonObject exchange = m_contestDef["exchange"].toObject();
        if (exchange.contains("fields")) {
            QJsonObject fields = exchange["fields"].toObject();
            if (fields.contains(fieldName)) {
                QJsonObject fieldDef = fields[fieldName].toObject();
                return fieldDef["type"].toString();
            }
        }
    }
    return "text";
}

int ContestEngine::getFieldMaxLength(const QString& fieldName) const
{
    if (m_contestDef.contains("exchange")) {
        QJsonObject exchange = m_contestDef["exchange"].toObject();
        if (exchange.contains("fields")) {
            QJsonObject fields = exchange["fields"].toObject();
            if (fields.contains(fieldName)) {
                QJsonObject fieldDef = fields[fieldName].toObject();
                if (fieldDef.contains("maxLength")) {
                    return fieldDef["maxLength"].toInt();
                }
            }
        }
    }
    return 0; // 0 means no limit
}

bool ContestEngine::validateExchange(const QString& fieldName, const QString& value, QString& errorMsg) const
{
    if (value.isEmpty()) {
        errorMsg = QString("%1 cannot be empty").arg(getFieldLabel(fieldName));
        return false;
    }
    
    QString type = getFieldType(fieldName);
    
    if (type == "serial") {
        return validateSerialNumber(value);
    } else if (type == "rst") {
        return validateRSTReport(value);
    } else if (type == "multiplier") {
        // Check if it's a valid state/province
        QString upper = value.toUpper();
        if (!m_validMultipliers.contains(upper)) {
            errorMsg = QString("Invalid multiplier: %1").arg(value);
            return false;
        }
    }
    
    return true;
}

bool ContestEngine::validateQso(const QsoRecord& qso, QString& errorMsg) const
{
    // Validate callsign
    if (qso.getCall().isEmpty()) {
        errorMsg = "Callsign is required";
        return false;
    }
    
    // Validate band/mode
    double freqKhz = qso.getFrequency().toDouble() * 1000;
    if (!isValidBand(freqKhz)) {
        errorMsg = "Invalid band for this contest";
        return false;
    }
    
    if (!isValidMode(qso.getMode())) {
        errorMsg = QString("Invalid mode: %1").arg(qso.getMode());
        return false;
    }
    
    // Validate exchange fields
    QStringList exchFields = getExchangeFields();
    for (const QString& field : exchFields) {
        QString value;
        if (field == "RST") {
            value = qso.getRstReceived();
        } else if (field == "EXCH") {
            value = qso.getExchangeReceived();
        } else {
            value = qso.getExchangeField(field);
        }
        
        QString err;
        if (!validateExchange(field, value, err)) {
            errorMsg = err;
            return false;
        }
    }
    
    return true;
}

bool ContestEngine::isDupe(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const
{
    QString dupeScope = getDupeScope();
    
    for (const QsoRecord& existing : existingQsos) {
        if (existing.getCall().toUpper() == qso.getCall().toUpper()) {
            if (dupeScope == "overall") {
                return true;
            } else if (dupeScope == "per_band") {
                double existFreq = existing.getFrequency().toDouble();
                double qsoFreq = qso.getFrequency().toDouble();
                if (qAbs(existFreq - qsoFreq) < 0.1) {
                    return true;
                }
            } else if (dupeScope == "per_mode") {
                if (existing.getMode() == qso.getMode()) {
                    return true;
                }
            } else if (dupeScope == "per_band_mode") {
                double existFreq = existing.getFrequency().toDouble();
                double qsoFreq = qso.getFrequency().toDouble();
                if (existing.getMode() == qso.getMode() && qAbs(existFreq - qsoFreq) < 0.1) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

QString ContestEngine::getDupeScope() const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("dupeChecking")) {
            return scoring["dupeChecking"].toString();
        }
    }
    return "overall";
}

int ContestEngine::calculatePoints(const QsoRecord& qso, const QString& myCallsign) const
{
    if (!m_dxccDatabase || !m_dxccDatabase->isLoaded()) {
        DebugLogger::instance().log("ContestEngine", "DXCC database not available for scoring");
        return getPointsForMode(qso.getMode());
    }
    
    // Get DXCC info for both stations
    QString theirCall = qso.getCall();
    DxccEntity myEntity = m_dxccDatabase->lookupCallsign(myCallsign);
    DxccEntity theirEntity = m_dxccDatabase->lookupCallsign(theirCall);
    
    QString myCountry = myEntity.country;
    QString theirCountry = theirEntity.country;
    QString myContinent = myEntity.continent;
    QString theirContinent = theirEntity.continent;
    int myDxcc = myEntity.dxcc;
    int theirDxcc = theirEntity.dxcc;
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Scoring QSO: %1 -> %2 | My: %3/%4/%5 | Their: %6/%7/%8")
            .arg(myCallsign).arg(theirCall)
            .arg(myCountry).arg(myContinent).arg(myDxcc)
            .arg(theirCountry).arg(theirContinent).arg(theirDxcc));
    
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("points")) {
            QJsonObject points = scoring["points"].toObject();
            QString mode = qso.getMode().toUpper();
            
            // Normalize mode names
            if (mode == "SSB" || mode == "USB" || mode == "LSB" || mode == "FM") {
                mode = "SSB";
            } else if (mode == "RTTY" || mode == "PSK" || mode == "FT8" || mode == "FT4") {
                mode = "DIGITAL";
            }
            
            // Determine relationship
            bool sameCountry = (myDxcc == theirDxcc && myDxcc != 0);
            bool sameContinent = (myContinent == theirContinent && !myContinent.isEmpty());
            
            DebugLogger::instance().log("ContestEngine", 
                QString("  Relationship: sameCountry=%1 sameContinent=%2 mode=%3")
                    .arg(sameCountry).arg(sameContinent).arg(mode));
            
            // Check for same country scoring
            if (sameCountry && points.contains("sameCountry")) {
                QJsonObject sameCountryPoints = points["sameCountry"].toObject();
                if (sameCountryPoints.contains(mode)) {
                    int pts = sameCountryPoints[mode].toInt();
                    DebugLogger::instance().log("ContestEngine", QString("  Points: %1 (same country)").arg(pts));
                    return pts;
                }
            }
            
            // Check for same continent scoring
            if (sameContinent && points.contains("sameContinent")) {
                QJsonObject sameContPoints = points["sameContinent"].toObject();
                if (sameContPoints.contains(mode)) {
                    int pts = sameContPoints[mode].toInt();
                    DebugLogger::instance().log("ContestEngine", QString("  Points: %1 (same continent)").arg(pts));
                    return pts;
                }
            }
            
            // Different continent scoring
            if (!sameContinent && points.contains("differentContinent")) {
                QJsonObject diffContPoints = points["differentContinent"].toObject();
                if (diffContPoints.contains(mode)) {
                    int pts = diffContPoints[mode].toInt();
                    DebugLogger::instance().log("ContestEngine", QString("  Points: %1 (different continent)").arg(pts));
                    return pts;
                }
            }
        }
    }
    
    int pts = getPointsForMode(qso.getMode());
    DebugLogger::instance().log("ContestEngine", QString("  Points: %1 (default)").arg(pts));
    return pts;
}

QStringList ContestEngine::getMultipliers(const QsoRecord& qso) const
{
    QStringList mults;
    
    // Extract multiplier from exchange
    QString mult = extractMultiplier(qso);
    if (!mult.isEmpty() && m_validMultipliers.contains(mult.toUpper())) {
        mults.append(mult.toUpper());
    }
    
    return mults;
}

int ContestEngine::calculateTotalScore(const QList<QsoRecord>& qsos, int& totalQsos, int& totalMults) const
{
    int totalPoints = 0;
    QSet<QString> uniqueMultipliers;
    
    totalQsos = qsos.size();
    
    for (const QsoRecord& qso : qsos) {
        // TODO: Pass station callsign for proper scoring
        totalPoints += calculatePoints(qso, "");
        
        QStringList mults = getMultipliers(qso);
        for (const QString& mult : mults) {
            uniqueMultipliers.insert(mult);
        }
    }
    
    totalMults = uniqueMultipliers.size();
    
    return totalPoints * totalMults;
}

bool ContestEngine::isValidBand(double freqKhz) const
{
    if (m_contestDef.contains("bands")) {
        QJsonArray bands = m_contestDef["bands"].toArray();
        for (const QJsonValue& val : bands) {
            QJsonObject band = val.toObject();
            double minFreq = band["minFrequency"].toDouble();
            double maxFreq = band["maxFrequency"].toDouble();
            
            if (freqKhz >= minFreq && freqKhz <= maxFreq) {
                return true;
            }
        }
        return false;
    }
    return true; // If no bands specified, all are valid
}

bool ContestEngine::isValidMode(const QString& mode) const
{
    QStringList allowed = getAllowedModes();
    return allowed.isEmpty() || allowed.contains(mode.toUpper());
}

QStringList ContestEngine::getAllowedModes() const
{
    QStringList modes;
    if (m_contestDef.contains("bands")) {
        QJsonArray bands = m_contestDef["bands"].toArray();
        for (const QJsonValue& val : bands) {
            QJsonObject band = val.toObject();
            if (band.contains("modes")) {
                QJsonArray bandModes = band["modes"].toArray();
                for (const QJsonValue& modeVal : bandModes) {
                    QString mode = modeVal.toString().toUpper();
                    if (!modes.contains(mode)) {
                        modes.append(mode);
                    }
                }
            }
        }
    }
    return modes;
}

bool ContestEngine::validateSerialNumber(const QString& value) const
{
    QRegularExpression re("^\\d{1,4}$");
    return re.match(value).hasMatch();
}

bool ContestEngine::validateState(const QString& value) const
{
    return m_validStates.contains(value.toUpper());
}

bool ContestEngine::validateProvince(const QString& value) const
{
    return m_validProvinces.contains(value.toUpper());
}

bool ContestEngine::validateGridSquare(const QString& value) const
{
    QRegularExpression re("^[A-R]{2}[0-9]{2}([A-X]{2})?$", QRegularExpression::CaseInsensitiveOption);
    return re.match(value).hasMatch();
}

bool ContestEngine::validateRSTReport(const QString& value) const
{
    QRegularExpression re("^[1-5]{2,3}$");
    return re.match(value).hasMatch();
}

QString ContestEngine::extractMultiplier(const QsoRecord& qso) const
{
    // Parse exchange to find multiplier
    // This is simplified - real implementation would parse based on contest definition
    QString exchange = qso.getExchange().toUpper();
    
    // Check each word in exchange
    QStringList words = exchange.split(QRegularExpression("\\s+"));
    for (const QString& word : words) {
        if (m_validMultipliers.contains(word)) {
            return word;
        }
    }
    
    return QString();
}

int ContestEngine::getPointsForMode(const QString& mode) const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("points")) {
            QJsonObject points = scoring["points"].toObject();
            
            QString modeUpper = mode.toUpper();
            if (modeUpper == "CW" && points.contains("cw")) {
                return points["cw"].toInt();
            } else if ((modeUpper == "SSB" || modeUpper == "USB" || modeUpper == "LSB") 
                       && points.contains("phone")) {
                return points["phone"].toInt();
            } else if (points.contains("digital")) {
                return points["digital"].toInt();
            }
        }
    }
    return 1; // Default 1 point
}

bool ContestEngine::needsStationClass() const
{
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        return stationClasses["enabled"].toBool(false);
    }
    return false;
}

QString ContestEngine::getStationClassPrompt() const
{
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        return stationClasses["prompt"].toString();
    }
    return QString();
}

QStringList ContestEngine::getStationClassOptions() const
{
    QStringList options;
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                QString id = classObj["id"].toString();
                QString name = classObj["name"].toString();
                QString desc = classObj["description"].toString();
                options.append(QString("%1|%2|%3").arg(id, name, desc));
            }
        }
    }
    return options;
}

void ContestEngine::setStationClass(const QString& classId)
{
    m_stationClass = classId;
    DebugLogger::instance().log("ContestEngine", 
                     QString("Station class set to: %1").arg(classId));
}

QString ContestEngine::getDefaultSentExchange(const QString& stationQth, int serialNumber) const
{
    if (m_stationClass.isEmpty()) {
        return QString();
    }
    
    // Find the station class definition
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    // Found the class, check exchangeSent
                    if (classObj.contains("exchangeSent")) {
                        QJsonObject exchSent = classObj["exchangeSent"].toObject();
                        QString type = exchSent["type"].toString();
                        
                        if (type == "state_province") {
                            return stationQth.toUpper();
                        } else if (type == "serial") {
                            return QString::number(serialNumber);
                        }
                    }
                }
            }
        }
    }
    
    return QString();
}
