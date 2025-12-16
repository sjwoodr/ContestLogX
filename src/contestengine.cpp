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
    DebugLogger::instance().log("ContestEngine", "loadContest() called");
    
    if (contestDef.isEmpty()) {
        DebugLogger::instance().log("ContestEngine", "ERROR: Contest definition is empty");
        return false;
    }
    
    m_contestDef = contestDef;
    
    QString contestName = getContestName();
    if (contestName.isEmpty()) {
        DebugLogger::instance().log("ContestEngine", "WARNING: Contest has no name");
    } else {
        DebugLogger::instance().log("ContestEngine", QString("Loading contest: %1").arg(contestName));
    }
    
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
                DebugLogger::instance().log("ContestEngine", 
                    QString("Loading %1 multipliers").arg(multList.size()));
                
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
            } else {
                DebugLogger::instance().log("ContestEngine", "No multipliers array found in exchangeValidation");
            }
        } else {
            DebugLogger::instance().log("ContestEngine", "No exchangeValidation found in validation");
        }
    } else {
        DebugLogger::instance().log("ContestEngine", "No validation section found");
    }
    
    // Validate precedence array covers all defined scoring rules
    if (contestDef.contains("scoring")) {
        QJsonObject scoring = contestDef["scoring"].toObject();
        if (scoring.contains("precedence") && scoring.contains("points")) {
            QJsonArray precArray = scoring["precedence"].toArray();
            QStringList precedence;
            for (const QJsonValue& val : precArray) {
                precedence.append(val.toString());
            }
            
            QJsonObject points = scoring["points"].toObject();
            QStringList definedRules = points.keys();
            for (const QString& rule : definedRules) {
                if (!precedence.contains(rule)) {
                    DebugLogger::instance().log("ContestEngine", 
                        QString("WARNING: Scoring rule '%1' is defined in points but not in precedence array - it will be ignored!")
                            .arg(rule));
                }
            }
        }
    }
    
    // Log Alaska/Hawaii treatment
    QString akHiTreatment = getAlaskaHawaiiTreatment();
    DebugLogger::instance().log("ContestEngine", 
        QString("Alaska/Hawaii treatment: %1").arg(akHiTreatment));
    
    DebugLogger::instance().log("ContestEngine", 
                     QString("Contest loaded: %1, Multipliers: %2, States: %3, Provinces: %4")
                     .arg(contestName)
                     .arg(m_validMultipliers.size())
                     .arg(m_validStates.size())
                     .arg(m_validProvinces.size()));
    
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
            bool sameDxccEntity = (myDxcc == theirDxcc && myDxcc != 0);
            bool sameContinent = (myContinent == theirContinent && !myContinent.isEmpty());
            
            DebugLogger::instance().log("ContestEngine", 
                QString("  Relationship: sameDxcc=%1 sameContinent=%2 mode=%3")
                    .arg(sameDxccEntity).arg(sameContinent).arg(mode));
            
            // Get precedence order from contest definition (defaults to legacy order)
            QStringList precedence;
            if (scoring.contains("precedence")) {
                QJsonArray precArray = scoring["precedence"].toArray();
                for (const QJsonValue& val : precArray) {
                    precedence.append(val.toString());
                }
                
                // Warn about defined points not in precedence
                QStringList definedRules = points.keys();
                for (const QString& rule : definedRules) {
                    if (!precedence.contains(rule)) {
                        DebugLogger::instance().log("ContestEngine", 
                            QString("WARNING: Scoring rule '%1' is defined in points but not in precedence array - it will be ignored!")
                                .arg(rule));
                    }
                }
            } else {
                // Default precedence for backward compatibility
                precedence << "sameDxccEntity" << "sameCountry" 
                          << "differentDxccEntity" << "differentCountry"
                          << "sameContinent" << "differentContinent";
            }
            
            // Check each scoring rule in precedence order
            for (const QString& rule : precedence) {
                bool ruleApplies = false;
                QString debugName = rule;
                
                // Determine if this rule applies based on relationship
                if (rule == "sameDxccEntity" || rule == "sameCountry") {
                    ruleApplies = sameDxccEntity;
                } else if (rule == "differentDxccEntity" || rule == "differentCountry") {
                    ruleApplies = !sameDxccEntity;
                } else if (rule == "sameContinent") {
                    ruleApplies = sameContinent;
                } else if (rule == "differentContinent") {
                    ruleApplies = !sameContinent;
                }
                
                // If rule applies and points are defined, return them
                if (ruleApplies && points.contains(rule)) {
                    QJsonObject rulePoints = points[rule].toObject();
                    if (rulePoints.contains(mode)) {
                        int pts = rulePoints[mode].toInt();
                        DebugLogger::instance().log("ContestEngine", 
                            QString("  Points: %1 (%2)").arg(pts).arg(rule));
                        return pts;
                    }
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
        QString multUpper = mult.toUpper();
        mults.append(multUpper);
        
        // Handle Alaska and Hawaii special case
        if (multUpper == "AK" || multUpper == "HI") {
            QString treatment = getAlaskaHawaiiTreatment();
            
            DebugLogger::instance().log("ContestEngine", 
                QString("  Found AK/HI multiplier '%1' - treatment: %2").arg(multUpper).arg(treatment));
            
            // If treatment is 'none', don't count AK/HI as multipliers at all
            if (treatment == "none") {
                mults.clear();
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Removed '%1' (treatment is 'none' - not a multiplier)").arg(multUpper));
                return mults;
            }
            
            // Get DXCC entity for the callsign
            if (m_dxccDatabase) {
                DxccEntity entity = m_dxccDatabase->lookupCallsign(qso.getCall());
                QString dxccName = entity.country;
                
                DebugLogger::instance().log("ContestEngine", 
                    QString("  DXCC lookup: %1 = %2 (DXCC %3)").arg(qso.getCall()).arg(dxccName).arg(entity.dxcc));
                
                // Alaska is DXCC 006, Hawaii is DXCC 110
                bool isAlaska = (entity.dxcc == 6 || dxccName.contains("Alaska", Qt::CaseInsensitive));
                bool isHawaii = (entity.dxcc == 110 || dxccName.contains("Hawaii", Qt::CaseInsensitive));
                
                if ((isAlaska || isHawaii) && treatment == "both") {
                    // Count as both state and DXCC - add DXCC mult
                    QString dxccMult = isAlaska ? "KL7" : "KH6";
                    mults.append(dxccMult);
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Added DXCC mult '%1' (both state and DXCC)").arg(dxccMult));
                } else if ((isAlaska || isHawaii) && treatment == "dxcc") {
                    // Count only as DXCC, replace state with DXCC mult
                    mults.clear();
                    QString dxccMult = isAlaska ? "KL7" : "KH6";
                    mults.append(dxccMult);
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Replaced with DXCC mult '%1' (dxcc only)").arg(dxccMult));
                } else if (treatment == "states") {
                    // Keep only the state mult (already in list)
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Keeping state mult '%1' only").arg(multUpper));
                }
            }
        }
    }
    
    return mults;
}

QString ContestEngine::getMultiplierType() const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            return mults["type"].toString("multsOnce");
        }
    }
    return "multsOnce";
}

QString ContestEngine::getAlaskaHawaiiTreatment() const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            return mults["alaskaAndHawaiiAre"].toString("both");
        }
    }
    return "both"; // Default: count as both state and DXCC
}

bool ContestEngine::isNewMultiplier(const QString& mult, const QString& band, const QString& mode, const QList<QsoRecord>& existingQsos) const
{
    QString multType = getMultiplierType();
    QString multUpper = mult.toUpper();
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Checking if %1 is new multiplier (type=%2, band=%3, mode=%4)")
            .arg(multUpper).arg(multType).arg(band).arg(mode));
    
    for (const QsoRecord& existingQso : existingQsos) {
        QStringList existingMults = getMultipliers(existingQso);
        
        for (const QString& existingMult : existingMults) {
            if (existingMult.toUpper() == multUpper) {
                // Found same multiplier, now check scope
                if (multType == "multsOnce") {
                    // Already worked this mult
                    return false;
                } else if (multType == "multsPerBand") {
                    // Check if same band
                    if (existingQso.getBand() == band) {
                        return false;
                    }
                } else if (multType == "multsPerMode") {
                    // Check if same mode
                    if (existingQso.getMode() == mode) {
                        return false;
                    }
                } else if (multType == "multsPerBandAndMode") {
                    // Check if same band AND mode
                    if (existingQso.getBand() == band && existingQso.getMode() == mode) {
                        return false;
                    }
                }
            }
        }
    }
    
    return true; // New multiplier for this scope
}

int ContestEngine::calculateTotalScore(const QList<QsoRecord>& qsos, int& totalQsos, int& totalMults) const
{
    int totalPoints = 0;
    QString multType = getMultiplierType();
    
    totalQsos = qsos.size();
    
    // Build multiplier tracking structure based on type
    QSet<QString> uniqueMultipliers;                          // multsOnce
    QSet<QString> multPerBand;                                 // multsPerBand: mult_band
    QSet<QString> multPerMode;                                 // multsPerMode: mult_mode
    QSet<QString> multPerBandAndMode;                          // multsPerBandAndMode: mult_band_mode
    
    for (const QsoRecord& qso : qsos) {
        // TODO: Pass station callsign for proper scoring
        totalPoints += calculatePoints(qso, "");
        
        QStringList mults = getMultipliers(qso);
        QString band = qso.getBand();
        QString mode = qso.getMode();
        
        for (const QString& mult : mults) {
            if (multType == "multsOnce") {
                uniqueMultipliers.insert(mult);
            } else if (multType == "multsPerBand") {
                multPerBand.insert(QString("%1_%2").arg(mult).arg(band));
            } else if (multType == "multsPerMode") {
                multPerMode.insert(QString("%1_%2").arg(mult).arg(mode));
            } else if (multType == "multsPerBandAndMode") {
                multPerBandAndMode.insert(QString("%1_%2_%3").arg(mult).arg(band).arg(mode));
            }
        }
    }
    
    // Count multipliers based on type
    if (multType == "multsOnce") {
        totalMults = uniqueMultipliers.size();
    } else if (multType == "multsPerBand") {
        totalMults = multPerBand.size();
    } else if (multType == "multsPerMode") {
        totalMults = multPerMode.size();
    } else if (multType == "multsPerBandAndMode") {
        totalMults = multPerBandAndMode.size();
    }
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Total score: %1 points × %2 mults (type=%3) = %4")
            .arg(totalPoints).arg(totalMults).arg(multType).arg(totalPoints * totalMults));
    
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

void ContestEngine::updateRunningScore(const QList<QsoRecord>& qsos, const QString& myCallsign)
{
    DebugLogger::instance().log("ContestEngine", 
        QString("Updating running score for %1 QSOs").arg(qsos.size()));
    
    // Reset running score
    m_runningScore = ContestScore();
    
    QString multType = getMultiplierType();
    
    // Track multipliers based on type
    QSet<QString> uniqueMultipliers;                    // multsOnce
    QSet<QString> multPerBand;                          // multsPerBand
    QSet<QString> multPerMode;                          // multsPerMode
    QSet<QString> multPerBandAndMode;                   // multsPerBandAndMode
    
    // Process each QSO
    for (const QsoRecord& qso : qsos) {
        QString band = qso.getBand();
        QString mode = qso.getMode().toUpper();
        
        // Normalize mode for counting
        QString modeCategory;
        if (mode == "CW") {
            modeCategory = "CW";
        } else if (mode == "SSB" || mode == "USB" || mode == "LSB" || mode == "FM") {
            modeCategory = "SSB";
        } else if (mode == "RTTY" || mode == "PSK" || mode == "FT8" || mode == "FT4" || mode == "DIGITAL") {
            modeCategory = "DIGITAL";
        } else {
            modeCategory = "SSB"; // Default unknown modes to SSB
        }
        
        // Initialize band stats if needed
        if (!m_runningScore.bandStats.contains(band)) {
            m_runningScore.bandStats[band] = BandModeStats();
        }
        
        // Update QSO count for this band/mode
        if (modeCategory == "CW") {
            m_runningScore.bandStats[band].cwQsos++;
        } else if (modeCategory == "SSB") {
            m_runningScore.bandStats[band].ssbQsos++;
        } else if (modeCategory == "DIGITAL") {
            m_runningScore.bandStats[band].digitalQsos++;
        }
        
        // Calculate points for this QSO
        int qsoPoints = calculatePoints(qso, myCallsign);
        m_runningScore.bandStats[band].points += qsoPoints;
        m_runningScore.contactScore += qsoPoints;
        
        // Track multipliers
        QStringList mults = getMultipliers(qso);
        for (const QString& mult : mults) {
            if (multType == "multsOnce") {
                uniqueMultipliers.insert(mult);
            } else if (multType == "multsPerBand") {
                multPerBand.insert(QString("%1_%2").arg(mult).arg(band));
            } else if (multType == "multsPerMode") {
                multPerMode.insert(QString("%1_%2").arg(mult).arg(mode));
            } else if (multType == "multsPerBandAndMode") {
                multPerBandAndMode.insert(QString("%1_%2_%3").arg(mult).arg(band).arg(mode));
            }
        }
    }
    
    // Count multipliers based on type
    if (multType == "multsOnce") {
        m_runningScore.multipliers = uniqueMultipliers.size();
    } else if (multType == "multsPerBand") {
        m_runningScore.multipliers = multPerBand.size();
    } else if (multType == "multsPerMode") {
        m_runningScore.multipliers = multPerMode.size();
    } else if (multType == "multsPerBandAndMode") {
        m_runningScore.multipliers = multPerBandAndMode.size();
    }
    
    // Calculate final contest score
    m_runningScore.bonusPoints = 0; // Hard-coded to 0 for now
    m_runningScore.contestScore = (m_runningScore.contactScore * m_runningScore.multipliers) + m_runningScore.bonusPoints;
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Running score updated: %1 QSOs, %2 points, %3 mults = %4 score")
            .arg(qsos.size())
            .arg(m_runningScore.contactScore)
            .arg(m_runningScore.multipliers)
            .arg(m_runningScore.contestScore));
}
