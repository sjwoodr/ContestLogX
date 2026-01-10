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
    QString version = "unknown";
    if (contestDef.contains("contest")) {
        QJsonObject contest = contestDef["contest"].toObject();
        if (contest.contains("version")) {
            version = contest["version"].toString();
        }
    }
    if (contestName.isEmpty()) {
        DebugLogger::instance().log("ContestEngine", "WARNING: Contest has no name");
    } else {
        DebugLogger::instance().log("ContestEngine", QString("Loading contest: %1 (v%2)").arg(contestName, version));
    }
    
    // Load multipliers into sets for fast lookup
    m_validMultipliers.clear();
    m_validStates.clear();
    m_validProvinces.clear();
    
    // Load from validation.namedMults array (this is the single source of truth)
    if (contestDef.contains("validation")) {
        QJsonObject validation = contestDef["validation"].toObject();
        if (validation.contains("namedMults")) {
            QJsonArray multList = validation["namedMults"].toArray();
            DebugLogger::instance().log("ContestEngine", 
                QString("Loading %1 named multipliers").arg(multList.size()));
            
            for (const QJsonValue& val : multList) {
                QString mult = val.toString().toUpper();
                m_validMultipliers.insert(mult);
            }
        } else {
            DebugLogger::instance().log("ContestEngine", "No namedMults array found in validation");
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
    
    // Log Alaska/Hawaii DXCC treatment
    bool akHiCountDxcc = getAlaskaHawaiiCountDxcc();
    DebugLogger::instance().log("ContestEngine", 
        QString("Alaska/Hawaii count as DXCC: %1").arg(akHiCountDxcc ? "true" : "false"));
    
    // Log US/Canada DXCC counting
    bool usCanadaDxcc = getUsAndCanadaCountDxcc();
    DebugLogger::instance().log("ContestEngine", 
        QString("US/Canada count as DXCC: %1").arg(usCanadaDxcc ? "true" : "false"));
    
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
    // Check exchangeFields first (new format)
    if (m_contestDef.contains("exchangeFields")) {
        QJsonObject exchangeFields = m_contestDef["exchangeFields"].toObject();
        
        // Check sent fields
        if (exchangeFields.contains("sent")) {
            QJsonArray sentFields = exchangeFields["sent"].toArray();
            for (const QJsonValue& val : sentFields) {
                QJsonObject fieldObj = val.toObject();
                if (fieldObj["name"].toString() == fieldName) {
                    return fieldObj["type"].toString();
                }
            }
        }
        
        // Check received fields
        if (exchangeFields.contains("received")) {
            QJsonArray recvFields = exchangeFields["received"].toArray();
            for (const QJsonValue& val : recvFields) {
                QJsonObject fieldObj = val.toObject();
                if (fieldObj["name"].toString() == fieldName) {
                    return fieldObj["type"].toString();
                }
            }
        }
    }
    
    // Fall back to old format
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
    DebugLogger::instance().log("ContestEngine", 
        QString("validateExchange: field='%1' value='%2'").arg(fieldName).arg(value));
    
    if (value.isEmpty()) {
        errorMsg = QString("%1 cannot be empty").arg(getFieldLabel(fieldName));
        return false;
    }
    
    QString type = getFieldType(fieldName);
    DebugLogger::instance().log("ContestEngine", 
        QString("  Field type: %1").arg(type));
    
    if (type == "serial") {
        return validateSerialNumber(value);
    } else if (type == "rst") {
        if (!validateRSTReport(value)) {
            errorMsg = QString("Invalid RST report: %1").arg(value);
            DebugLogger::instance().log("ContestEngine", 
                QString("  RST validation failed: %1").arg(errorMsg));
            return false;
        }
        DebugLogger::instance().log("ContestEngine", 
            QString("  RST validation passed: %1").arg(value));
        return true;
    } else if (type == "multiplier") {
        // Check if it's a valid state/province
        QString upper = value.toUpper();
        if (!m_validMultipliers.contains(upper)) {
            errorMsg = QString("Invalid multiplier: %1").arg(value);
            return false;
        }
    } else if (type == "string" && fieldName == "EXCH") {
        // For EXCH fields, check validation section for special logic
        if (m_contestDef.contains("validation")) {
            QJsonObject validation = m_contestDef["validation"].toObject();
            if (validation.contains("exchangeValidation")) {
                QJsonObject exchVal = validation["exchangeValidation"].toObject();
                QString validationType = exchVal["type"].toString();
                
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Exchange validation type: %1").arg(validationType));
                
                if (validationType == "namedMultOrSerial") {
                    // Check if value is a valid multiplier OR a valid serial number
                    QString upper = value.toUpper();
                    
                    // First check if it's a valid multiplier
                    if (m_validMultipliers.contains(upper)) {
                        DebugLogger::instance().log("ContestEngine", 
                            QString("  '%1' is a valid multiplier").arg(upper));
                        return true;
                    }
                    
                    // Otherwise check if it matches serial number format
                    QString serialFormat = exchVal["serialNumberFormat"].toString();
                    if (!serialFormat.isEmpty()) {
                        QRegularExpression serialRe(serialFormat);
                        if (serialRe.match(value).hasMatch()) {
                            DebugLogger::instance().log("ContestEngine", 
                                QString("  '%1' matches serial format").arg(value));
                            return true;
                        }
                    }
                    
                    // Neither matched
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  '%1' is neither a valid mult nor serial").arg(upper));
                    errorMsg = QString("Invalid mult: %1").arg(value);
                    return false;
                }
            }
        }
        
        // For other string types, just check if non-empty (already done above)
        return true;
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
    double freqKhz = qso.getFrequency().toDouble();  // Already in kHz
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
    // First check explicit dupeChecking definition
    QString dupeScope = getDupeScope();
    
    // If no explicit dupe scope, use multiplier type to infer
    if (dupeScope.isEmpty()) {
        QString multType = getMultiplierType();
        if (multType == "multsOnce") {
            dupeScope = "overall";
        } else if (multType == "multsPerBand") {
            dupeScope = "per_band";
        } else if (multType == "multsPerMode") {
            dupeScope = "per_mode";
        } else if (multType == "multsPerBandAndMode") {
            dupeScope = "per_band_mode";
        }
    }
    
    for (const QsoRecord& existing : existingQsos) {
        // Skip invalid QSOs (out of band or already marked as dupe)
        if (existing.isOutOfBand() || existing.isDupe()) {
            continue;
        }
        
        if (existing.getCall().toUpper() == qso.getCall().toUpper()) {
            if (dupeScope == "overall") {
                return true;
            } else if (dupeScope == "per_band") {
                if (existing.getBand() == qso.getBand()) {
                    return true;
                }
            } else if (dupeScope == "per_mode") {
                if (existing.getMode() == qso.getMode()) {
                    return true;
                }
            } else if (dupeScope == "per_band_mode") {
                if (existing.getBand() == qso.getBand() && existing.getMode() == qso.getMode()) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

QString ContestEngine::getDupeReason(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const
{
    // First check explicit dupeChecking definition
    QString dupeScope = getDupeScope();
    
    // If no explicit dupe scope, use multiplier type to infer
    if (dupeScope.isEmpty()) {
        QString multType = getMultiplierType();
        if (multType == "multsOnce") {
            dupeScope = "overall";
        } else if (multType == "multsPerBand") {
            dupeScope = "per_band";
        } else if (multType == "multsPerMode") {
            dupeScope = "per_mode";
        } else if (multType == "multsPerBandAndMode") {
            dupeScope = "per_band_mode";
        }
    }
    
    for (const QsoRecord& existing : existingQsos) {
        // Skip invalid QSOs (out of band or already marked as dupe)
        if (existing.isOutOfBand() || existing.isDupe()) {
            continue;
        }
        
        if (existing.getCall().toUpper() == qso.getCall().toUpper()) {
            if (dupeScope == "overall") {
                return "contest";
            } else if (dupeScope == "per_band") {
                if (existing.getBand() == qso.getBand()) {
                    return "band";
                }
            } else if (dupeScope == "per_mode") {
                if (existing.getMode() == qso.getMode()) {
                    return "mode";
                }
            } else if (dupeScope == "per_band_mode") {
                if (existing.getBand() == qso.getBand() && existing.getMode() == qso.getMode()) {
                    return "band/mode";
                }
            }
        }
    }
    
    return "";
}

QString ContestEngine::getDupeScope() const
{
    // Check root-level dupeChecking object first
    if (m_contestDef.contains("dupeChecking")) {
        QJsonValue dupeCheckingValue = m_contestDef["dupeChecking"];
        if (dupeCheckingValue.isObject()) {
            QJsonObject dupeChecking = dupeCheckingValue.toObject();
            if (dupeChecking.contains("type")) {
                QString typeStr = dupeChecking["type"].toString();
                // Map dupeChecking type to dupeScope
                if (typeStr == "perBand") {
                    return "per_band";
                } else if (typeStr == "perBandAndMode") {
                    return "per_band_mode";
                } else if (typeStr == "perMode") {
                    return "per_mode";
                } else if (typeStr == "overall") {
                    return "overall";
                }
            }
        } else if (dupeCheckingValue.isString()) {
            // Handle legacy string format
            return dupeCheckingValue.toString();
        }
    }
    
    // Fallback to scoring section (legacy)
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
    // Check if QSO is on a valid band for this contest
    QString band = qso.getBand();
    if (!isValidBand(qso.getFrequency().toDouble())) {  // Already in kHz
        DebugLogger::instance().log("ContestEngine", 
            QString("QSO on band %1 is out of band for contest - 0 points").arg(band));
        return 0;
    }
    
    if (!m_dxccDatabase || !m_dxccDatabase->isLoaded()) {
        DebugLogger::instance().log("ContestEngine", "DXCC database not available for scoring");
        return getPointsForMode(qso.getMode());
    }
    
    // Get DXCC info for both stations (strip portable suffixes first)
    QString theirCall = qso.getCall();
    QString myCallForLookup = m_dxccDatabase->stripPortableSuffixes(myCallsign);
    QString theirCallForLookup = m_dxccDatabase->stripPortableSuffixes(theirCall);
    
    DxccEntity myEntity = m_dxccDatabase->lookupCallsign(myCallForLookup);
    DxccEntity theirEntity = m_dxccDatabase->lookupCallsign(theirCallForLookup);
    
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
            
            // Check for simple perQso scoring first
            if (points.contains("perQso")) {
                int pts = points["perQso"].toInt();
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Points: %1 (perQso)").arg(pts));
                return pts;
            }
            
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
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Getting multipliers for QSO: %1").arg(qso.getCall()));
    
    // Check if callsign itself is a multiplier (for contests like CWops CWT)
    QStringList multCategories = getMultiplierCategories();
    
    // Check multiplier sources in definition
    bool callsignIsMult = false;
    if (m_contestDef.contains("multipliers")) {
        QJsonObject multipliers = m_contestDef["multipliers"].toObject();
        if (multipliers.contains("sources")) {
            QJsonArray sources = multipliers["sources"].toArray();
            for (const QJsonValue& val : sources) {
                QJsonObject source = val.toObject();
                if (source["type"].toString() == "callsign") {
                    callsignIsMult = true;
                    break;
                }
            }
        }
    }
    
    // If callsign is the multiplier, return it
    if (callsignIsMult) {
        QString call = qso.getCall().toUpper();
        mults.append(call);
        DebugLogger::instance().log("ContestEngine", 
            QString("  Callsign is multiplier: %1").arg(call));
        return mults;
    }
    
    // Otherwise, extract multiplier from exchange (state/province/DXCC)
    bool dxccIsMult = multCategories.contains("dxcc");
    
    // Extract multiplier from exchange (state/province)
    QString mult = extractMultiplier(qso);
    if (!mult.isEmpty() && m_validMultipliers.contains(mult.toUpper())) {
        QString multUpper = mult.toUpper();
        mults.append(multUpper);
        
        // Handle Alaska and Hawaii special case
        // AK and HI are in namedMults (like states) but may also count as DXCC mults
        if (multUpper == "AK" || multUpper == "HI") {
            bool akHiCountDxcc = getAlaskaHawaiiCountDxcc();
            
            DebugLogger::instance().log("ContestEngine", 
                QString("  Found AK/HI multiplier '%1' - counts as DXCC: %2").arg(multUpper).arg(akHiCountDxcc ? "yes" : "no"));
            
            // If dxcc is in multiplier categories and AK/HI should count as DXCC, add the DXCC mult
            if (akHiCountDxcc && dxccIsMult) {
                // Get DXCC entity for the callsign to get the proper prefix
                if (m_dxccDatabase) {
                    DxccEntity entity = m_dxccDatabase->lookupCallsign(qso.getCall());
                    QString dxccName = entity.country;
                    
                    // Alaska is DXCC 006, Hawaii is DXCC 110
                    bool isAlaska = (entity.dxcc == 6 || dxccName.contains("Alaska", Qt::CaseInsensitive));
                    bool isHawaii = (entity.dxcc == 110 || dxccName.contains("Hawaii", Qt::CaseInsensitive));
                    
                    if (isAlaska || isHawaii) {
                        QString dxccMult = isAlaska ? "KL7" : "KH6";
                        mults.append(dxccMult);
                        DebugLogger::instance().log("ContestEngine", 
                            QString("  Added DXCC mult '%1' (AK/HI count as both state and DXCC)").arg(dxccMult));
                    }
                }
            }
        }
    }
    
    // Check if we should add DXCC entity as a multiplier
    if (dxccIsMult && m_dxccDatabase) {
        DxccEntity entity = m_dxccDatabase->lookupCallsign(qso.getCall());
        
        // If no state/province multiplier found (i.e., DX station), add DXCC as mult
        if (mult.isEmpty() || !m_validMultipliers.contains(mult.toUpper())) {
            // Use the first prefix as the multiplier identifier
            if (!entity.prefixes.isEmpty()) {
                QString dxccMult = entity.prefixes.first().prefix;
                mults.append(dxccMult);
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Added DXCC mult: %1 (%2 - DXCC %3)")
                        .arg(dxccMult)
                        .arg(entity.country)
                        .arg(entity.dxcc));
            }
        }
    }
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Returning %1 multipliers for %2: %3")
            .arg(mults.size())
            .arg(qso.getCall())
            .arg(mults.join(", ")));
    
    return mults;
}

QList<ContestEngine::MultiplierInfo> ContestEngine::getMultipliersWithCategory(const QsoRecord& qso) const
{
    QList<MultiplierInfo> result;
    
    // Check if callsign itself is a multiplier (for contests like CWops CWT)
    bool callsignIsMult = false;
    if (m_contestDef.contains("multipliers")) {
        QJsonObject multipliers = m_contestDef["multipliers"].toObject();
        if (multipliers.contains("sources")) {
            QJsonArray sources = multipliers["sources"].toArray();
            for (const QJsonValue& val : sources) {
                QJsonObject source = val.toObject();
                if (source["type"].toString() == "callsign") {
                    callsignIsMult = true;
                    break;
                }
            }
        }
    }
    
    // If callsign is the multiplier, return it
    if (callsignIsMult) {
        QString call = qso.getCall().toUpper();
        result.append({call, "callsign"});
        return result;
    }
    
    // Otherwise, check if DXCC is a multiplier category
    QStringList multCategories = getMultiplierCategories();
    bool dxccIsMult = multCategories.contains("dxcc");
    
    // Extract multiplier from exchange (state/province)
    QString mult = extractMultiplier(qso);
    if (!mult.isEmpty() && m_validMultipliers.contains(mult.toUpper())) {
        QString multUpper = mult.toUpper();
        
        // All named multipliers from validation.namedMults are valid
        result.append({multUpper, "namedMults"});
        
        // Handle Alaska and Hawaii special case
        if (multUpper == "AK" || multUpper == "HI") {
            bool akHiCountDxcc = getAlaskaHawaiiCountDxcc();
            DebugLogger::instance().log("ContestEngine", 
                QString("  Found AK/HI multiplier '%1' - counts as DXCC: %2").arg(multUpper).arg(akHiCountDxcc ? "yes" : "no"));
            
            // If dxcc is in multiplier categories and AK/HI should count as DXCC
            if (akHiCountDxcc && dxccIsMult && m_dxccDatabase) {
                DxccEntity entity = m_dxccDatabase->lookupCallsign(qso.getCall());
                if (!entity.primaryPrefix.isEmpty() || !entity.prefixes.isEmpty()) {
                    QString dxccMult = entity.primaryPrefix.isEmpty() ? entity.prefixes.first().prefix : entity.primaryPrefix;
                    result.append({dxccMult, "dxcc"});
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Added DXCC mult '%1' for AK/HI").arg(dxccMult));
                }
            }
        }
    }
    

    // Check if we should add DXCC entity as a multiplier
    if (dxccIsMult && m_dxccDatabase) {
        DxccEntity entity = m_dxccDatabase->lookupCallsign(qso.getCall());
        
        // Check if we already added DXCC due to AK/HI treatment
        bool alreadyAddedDxcc = false;
        QString multUpper = mult.toUpper();
        if ((multUpper == "AK" || multUpper == "HI")) {
            bool akHiCountDxcc = getAlaskaHawaiiCountDxcc();
            if (akHiCountDxcc) {
                alreadyAddedDxcc = true;
            }
        }
        
        // Determine if we should add DXCC for US/Canadian stations
        bool shouldAddDxcc = false;
        if (alreadyAddedDxcc) {
            // Already handled by AK/HI special treatment
            shouldAddDxcc = false;
            DebugLogger::instance().log("ContestEngine", 
                QString("  DXCC: Already added via AK/HI treatment, skipping"));
        } else if (multUpper == "AK" || multUpper == "HI") {
            // This is Alaska/Hawaii but didn't count as DXCC, skip
            shouldAddDxcc = false;
            DebugLogger::instance().log("ContestEngine", 
                QString("  DXCC: AK/HI but alaskaAndHawaiiCountDxcc=false, skipping"));
        } else if (!mult.isEmpty() && m_validMultipliers.contains(mult.toUpper())) {
            // This is a US/Canadian station with a state/province
            // Check if they also count as DXCC
            bool usAndCanadaCountDxcc = getUsAndCanadaCountDxcc();
            shouldAddDxcc = usAndCanadaCountDxcc;
            DebugLogger::instance().log("ContestEngine", 
                QString("  DXCC: US/Canadian mult '%1' - counts as DXCC: %2").arg(mult.toUpper()).arg(usAndCanadaCountDxcc ? "yes" : "no"));
        } else {
            // This is a DX station (no state/province), always add DXCC
            shouldAddDxcc = true;
            DebugLogger::instance().log("ContestEngine", 
                QString("  DXCC: DX station, adding DXCC"));
        }
        
        // Add DXCC if appropriate
        if (shouldAddDxcc && !entity.prefixes.isEmpty()) {
            QString dxccMult = entity.primaryPrefix.isEmpty() ? entity.prefixes.first().prefix : entity.primaryPrefix;
            result.append({dxccMult, "dxcc"});
            DebugLogger::instance().log("ContestEngine", 
                QString("  Added DXCC mult '%1'").arg(dxccMult));
        }
    }
    
    // Check if ITU Regions are a multiplier category
    bool ituRegionIsMult = multCategories.contains("ituRegions");
    if (ituRegionIsMult && m_dxccDatabase) {
        int ituZone = m_dxccDatabase->getItuZone(qso.getCall());
        int ituRegion = m_dxccDatabase->getItuRegion(qso.getCall());
        DebugLogger::instance().log("ContestEngine", QString("ITU lookup for %1 -> Zone: %2 -> Region: %3")
            .arg(qso.getCall()).arg(ituZone).arg(ituRegion));
        if (ituRegion > 0) {
            result.append({QString::number(ituRegion), "ituRegions"});
        }
    } else if (ituRegionIsMult) {
        qDebug() << "ContestEngine: ituRegionIsMult=true but m_dxccDatabase is null";
    }
    
    return result;
}

ContestEngine::QsoMultiplierCredit ContestEngine::getQsoMultiplierCredit(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const
{
    QsoMultiplierCredit credit;
    
    // Get all multiplier info for this QSO
    QList<MultiplierInfo> multsWithCategory = getMultipliersWithCategory(qso);
    
    // Get the multiplier type to determine how to track
    QString multType = getMultiplierType();
    
    for (const MultiplierInfo& multInfo : multsWithCategory) {
        QString trackingKey = multInfo.value;
        
        if (multType == "multsPerBandAndMode") {
            trackingKey = QString("%1_%2_%3").arg(multInfo.value, qso.getBand(), qso.getMode());
        } else if (multType == "multsPerBand") {
            trackingKey = QString("%1_%2").arg(multInfo.value, qso.getBand());
        } else if (multType == "multsPerMode") {
            trackingKey = QString("%1_%2").arg(multInfo.value, qso.getMode());
        }
        
        // Check if this multiplier (not the call, but the actual mult value) was already worked with this tracking method
        bool isNew = true;
        for (const QsoRecord& existingQso : existingQsos) {
            // Skip invalid QSOs
            if (existingQso.isDupe() || existingQso.isOutOfBand()) {
                continue;
            }
            
            QList<MultiplierInfo> existingMults = getMultipliersWithCategory(existingQso);
            for (const MultiplierInfo& existingMult : existingMults) {
                // Check if this is the same mult value and category
                if (existingMult.value == multInfo.value && existingMult.category == multInfo.category) {
                    QString existingKey = existingMult.value;
                    
                    if (multType == "multsPerBandAndMode") {
                        existingKey = QString("%1_%2_%3").arg(existingMult.value, existingQso.getBand(), existingQso.getMode());
                    } else if (multType == "multsPerBand") {
                        existingKey = QString("%1_%2").arg(existingMult.value, existingQso.getBand());
                    } else if (multType == "multsPerMode") {
                        existingKey = QString("%1_%2").arg(existingMult.value, existingQso.getMode());
                    }
                    
                    if (trackingKey == existingKey) {
                        isNew = false;
                        break;
                    }
                }
            }
            if (!isNew) break;
        }
        
        if (isNew) {
            if (multInfo.category == "named" || multInfo.category == "namedMults") {
                credit.namedMultCount++;
            } else if (multInfo.category == "dxcc") {
                credit.dxccMultCount++;
            } else if (multInfo.category == "ituRegions") {
                credit.ituRegionMultCount++;
            }
        }
    }
    
    return credit;
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

QStringList ContestEngine::getMultiplierCategories() const
{
    QStringList categories;
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            if (mults.contains("categories")) {
                QJsonArray categoriesArray = mults["categories"].toArray();
                for (const QJsonValue& val : categoriesArray) {
                    categories.append(val.toString());
                }
            }
        }
    }
    return categories;
}

bool ContestEngine::getAlaskaHawaiiCountDxcc() const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            return mults["alaskaAndHawaiiCountDxcc"].toBool(true);
        }
    }
    return true; // Default: AK/HI count as both state and DXCC mults
}

bool ContestEngine::getUsAndCanadaCountDxcc() const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            return mults["usAndCanadaCountDxcc"].toBool(true);
        }
    }
    return true; // Default: US/Canada stations count as both state/province and DXCC
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
    // First, try the new "bands" array format with frequency ranges
    if (m_contestDef.contains("bands")) {
        QJsonArray bands = m_contestDef["bands"].toArray();
        for (const QJsonValue& val : bands) {
            QJsonObject band = val.toObject();
            double minFreq = band["minFrequency"].toDouble();
            double maxFreq = band["maxFrequency"].toDouble();
            
            if (minFreq > 0 && maxFreq > 0 && freqKhz >= minFreq && freqKhz <= maxFreq) {
                return true;
            }
        }
        // If we have bands array but no valid frequency ranges, check frequencies section
    }
    
    // Fall back to "frequencies" section (newer format)
    if (m_contestDef.contains("frequencies")) {
        QJsonObject frequencies = m_contestDef["frequencies"].toObject();
        for (const QString& bandName : frequencies.keys()) {
            QJsonObject bandFreqs = frequencies[bandName].toObject();
            double startFreq = bandFreqs["start"].toDouble();
            double endFreq = bandFreqs["end"].toDouble();
            
            if (freqKhz >= startFreq && freqKhz <= endFreq) {
                return true;
            }
        }
        return false;
    }
    
    return true; // If no bands specified, all are valid
}

QString ContestEngine::getBandFromFrequency(double freqKhz) const
{
    // First, try the new "bands" array format with frequency ranges
    if (m_contestDef.contains("bands")) {
        QJsonArray bands = m_contestDef["bands"].toArray();
        for (const QJsonValue& val : bands) {
            QJsonObject band = val.toObject();
            double minFreq = band["minFrequency"].toDouble();
            double maxFreq = band["maxFrequency"].toDouble();
            QString bandName = band["name"].toString();
            
            if (minFreq > 0 && maxFreq > 0 && freqKhz >= minFreq && freqKhz <= maxFreq) {
                return bandName;
            }
        }
    }
    
    // Fall back to "frequencies" section (newer format)
    if (m_contestDef.contains("frequencies")) {
        QJsonObject frequencies = m_contestDef["frequencies"].toObject();
        for (const QString& bandName : frequencies.keys()) {
            QJsonObject bandFreqs = frequencies[bandName].toObject();
            double startFreq = bandFreqs["start"].toDouble();
            double endFreq = bandFreqs["end"].toDouble();
            
            if (freqKhz >= startFreq && freqKhz <= endFreq) {
                return bandName;
            }
        }
    }
    
    return "";  // Unknown band
}

bool ContestEngine::isValidMode(const QString& mode) const
{
    QStringList allowed = getAllowedModes();
    if (allowed.isEmpty()) {
        return true;  // No restrictions
    }
    
    QString upperMode = mode.toUpper();
    
    // Direct match
    if (allowed.contains(upperMode)) {
        return true;
    }
    
    // If SSB is allowed, accept LSB and USB as valid
    if (allowed.contains("SSB") && (upperMode == "LSB" || upperMode == "USB")) {
        return true;
    }
    
    return false;
}

QStringList ContestEngine::getAllowedModes() const
{
    QStringList modes;
    
    // First, check if a station class with a specific mode is selected
    QString stationClassMode = getStationClassMode();
    if (!stationClassMode.isEmpty()) {
        modes.append(stationClassMode.toUpper());
        DebugLogger::instance().log("ContestEngine", 
            QString("getAllowedModes: Restricting to station class mode '%1'").arg(stationClassMode));
        return modes;
    }
    
    // Check for modes at the contest level
    if (m_contestDef.contains("contest")) {
        QJsonObject contest = m_contestDef["contest"].toObject();
        if (contest.contains("modes")) {
            QJsonArray contestModes = contest["modes"].toArray();
            for (const QJsonValue& modeVal : contestModes) {
                QString mode = modeVal.toString().toUpper();
                if (!modes.contains(mode)) {
                    modes.append(mode);
                }
            }
            // If we found modes at contest level, return those
            if (!modes.isEmpty()) {
                return modes;
            }
        }
    }
    
    // Fall back to checking per-band modes
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
    // RST: Readability (1-5), Signal (1-9), optional Tone (1-9) for CW
    // Phone: 2 digits (e.g., 59)
    // CW: 3 digits (e.g., 599)
    QRegularExpression re("^[1-5][1-9][1-9]?$");
    return re.match(value).hasMatch();
}

QString ContestEngine::extractMultiplier(const QsoRecord& qso) const
{
    // Parse exchange to find multiplier
    // Look at the received exchange first, then fall back to legacy exchange field
    QString exchange = qso.getExchangeReceived();
    if (exchange.isEmpty()) {
        exchange = qso.getExchange();
    }
    
    exchange = exchange.toUpper();
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Extracting multiplier from exchange: '%1'").arg(exchange));
    
    // Check each word in exchange
    QStringList words = exchange.split(QRegularExpression("\\s+"));
    for (const QString& word : words) {
        QString cleanWord = word.trimmed();
        if (!cleanWord.isEmpty() && m_validMultipliers.contains(cleanWord)) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Found multiplier: '%1'").arg(cleanWord));
            return cleanWord;
        }
    }
    
    DebugLogger::instance().log("ContestEngine", 
        QString("No multiplier found in exchange (checked %1 valid mults)").arg(m_validMultipliers.size()));
    
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
    m_stationClassExchangeName = QString();  // Reset exchange data when class changes
    m_stationClassExchangeId = QString();
    DebugLogger::instance().log("ContestEngine", 
                     QString("Station class set to: %1").arg(classId));
}

void ContestEngine::resetStationClassState()
{
    m_stationClass = QString();
    m_stationClassExchangeName = QString();
    m_stationClassExchangeId = QString();
    DebugLogger::instance().log("ContestEngine", "Station class state reset");
}

QString ContestEngine::getStationClassMode() const
{
    // If no station class is selected, return empty string
    if (m_stationClass.isEmpty()) {
        return QString();
    }
    
    // Look up the mode from the station class definition
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& classVal : classes) {
                QJsonObject classObj = classVal.toObject();
                if (classObj.contains("id") && classObj["id"].toString() == m_stationClass) {
                    if (classObj.contains("mode")) {
                        QString mode = classObj["mode"].toString();
                        DebugLogger::instance().log("ContestEngine", 
                            QString("getStationClassMode: Found mode '%1' for class '%2'").arg(mode, m_stationClass));
                        return mode;
                    }
                    break;
                }
            }
        }
    }
    
    return QString();
}

QStringList ContestEngine::getCallHistoryFieldsToSave() const
{
    // Check if contest defines specific fields to save in call history
    if (m_contestDef.contains("callHistory")) {
        QJsonObject callHistoryDef = m_contestDef["callHistory"].toObject();
        if (callHistoryDef.contains("fieldsToSave")) {
            QJsonArray fieldsArray = callHistoryDef["fieldsToSave"].toArray();
            QStringList fields;
            for (const QJsonValue& val : fieldsArray) {
                fields.append(val.toString());
            }
            return fields;
        }
    }
    
    // Default: save CALL and EXCHr if not explicitly defined
    QStringList defaultFields;
    defaultFields << "CALL" << "EXCHr";
    return defaultFields;
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
                        } else if (type == "fixedValue") {
                            return exchSent["value"].toString();
                        } else if (type == "customInput") {
                            // Return the combined custom exchange data (name + space + id)
                            return m_stationClassExchangeName + " " + m_stationClassExchangeId;
                        }
                    }
                }
            }
        }
    }
    
    return QString();
}

bool ContestEngine::stationClassNeedsInput() const
{
    if (m_stationClass.isEmpty()) {
        return false;
    }
    
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    return classObj.value("needsInput").toBool(false);
                }
            }
        }
    }
    
    return false;
}

QString ContestEngine::getStationClassInputPrompt() const
{
    if (m_stationClass.isEmpty()) {
        return QString();
    }
    
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    // Try to get combined prompt first (legacy)
                    if (classObj.contains("inputPrompt")) {
                        return classObj.value("inputPrompt").toString();
                    }
                    // Return empty for new separate prompt style
                    return QString();
                }
            }
        }
    }
    
    return QString();
}

QString ContestEngine::getStationClassNamePrompt() const
{
    if (m_stationClass.isEmpty()) {
        return QString();
    }
    
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    if (classObj.contains("inputPrompts")) {
                        QJsonObject prompts = classObj["inputPrompts"].toObject();
                        return prompts.value("name").toString();
                    }
                }
            }
        }
    }
    
    return "Enter your first name";
}

QString ContestEngine::getStationClassIdPrompt() const
{
    if (m_stationClass.isEmpty()) {
        return QString();
    }
    
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    if (classObj.contains("inputPrompts")) {
                        QJsonObject prompts = classObj["inputPrompts"].toObject();
                        return prompts.value("id").toString();
                    }
                }
            }
        }
    }
    
    return "Enter ID or location";
}

QJsonObject ContestEngine::getStationClassInputValidation() const
{
    if (m_stationClass.isEmpty()) {
        return QJsonObject();
    }
    
    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& val : classes) {
                QJsonObject classObj = val.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    if (classObj.contains("inputValidation")) {
                        return classObj["inputValidation"].toObject();
                    }
                }
            }
        }
    }
    
    return QJsonObject();
}


QString ContestEngine::getSentExchangeName() const
{
    // Return the stored exchange name directly
    return m_stationClassExchangeName;
}

QString ContestEngine::getSentExchangeId() const
{
    // Return the stored exchange ID directly
    return m_stationClassExchangeId;
}

void ContestEngine::setStationClassExchangeData(const QString& data)
{
    // Legacy method: split combined "Name ID" into separate fields
    int spacePos = data.indexOf(' ');
    if (spacePos > 0) {
        m_stationClassExchangeName = data.left(spacePos).toUpper();
        m_stationClassExchangeId = data.mid(spacePos + 1).toUpper();
    } else {
        m_stationClassExchangeName = data.toUpper();
        m_stationClassExchangeId = QString();
    }
}

QString ContestEngine::getStationClassExchangeData() const
{
    // Legacy method: combine separate fields into "Name ID"
    if (m_stationClassExchangeName.isEmpty()) {
        return m_stationClassExchangeId;
    } else if (m_stationClassExchangeId.isEmpty()) {
        return m_stationClassExchangeName;
    } else {
        return m_stationClassExchangeName + " " + m_stationClassExchangeId;
    }
}

void ContestEngine::updateRunningScore(QList<QsoRecord>& qsos, const QString& myCallsign, bool verbose)
{
    if (verbose) {
        DebugLogger::instance().log("ContestEngine", 
            QString("Updating running score for %1 QSOs").arg(qsos.size()));
    }
    
    // Reset running score
    m_runningScore = ContestScore();
    
    QString multType = getMultiplierType();
    
    // Track multipliers based on type
     QSet<QString> uniqueMultipliers;                    // multsOnce
    QSet<QString> multPerBand;                          // multsPerBand
    QSet<QString> multPerMode;                          // multsPerMode
    QSet<QString> multPerBandAndMode;                   // multsPerBandAndMode
    
    // For multsPerMode scoring: track multipliers per mode
    QSet<QString> cwMultipliers, ssbMultipliers, digitalMultipliers;
    
    // Track multipliers by type (named vs DXCC vs ITU Regions)
    QSet<QString> namedMultsOnce, dxccMultsOnce, ituRegionMultsOnce;
    QSet<QString> namedMultsPerBand, dxccMultsPerBand, ituRegionMultsPerBand;
    QSet<QString> namedMultsPerMode, dxccMultsPerMode, ituRegionMultsPerMode;
    QSet<QString> namedMultsPerBandAndMode, dxccMultsPerBandAndMode, ituRegionMultsPerBandAndMode;
    
    // Track DXCC entities separately (for informational purposes)
    QSet<int> uniqueDxccEntities;
    
    // Count valid QSOs (excluding dupes and out-of-band)
    int validQsoCount = 0;
    
    // Helper lambda to convert frequency (kHz) to band
    auto freqToBand = [](double freqKhz) -> QString {
        if (freqKhz >= 1800 && freqKhz < 2000) return "160m";
        if (freqKhz >= 3500 && freqKhz < 4000) return "80m";
        if (freqKhz >= 7000 && freqKhz < 7300) return "40m";
        if (freqKhz >= 10100 && freqKhz < 10150) return "30m";
        if (freqKhz >= 14000 && freqKhz < 14350) return "20m";
        if (freqKhz >= 18068 && freqKhz < 18168) return "17m";
        if (freqKhz >= 21000 && freqKhz < 21450) return "15m";
        if (freqKhz >= 24890 && freqKhz < 24990) return "12m";
        if (freqKhz >= 28000 && freqKhz < 29700) return "10m";
        if (freqKhz >= 50000 && freqKhz < 54000) return "6m";
        if (freqKhz >= 144000 && freqKhz < 148000) return "2m";
        if (freqKhz >= 420000 && freqKhz < 450000) return "70cm";
        return "";
    };
    
    // Process each QSO
    for (int i = 0; i < qsos.size(); ++i) {
        QsoRecord& qso = qsos[i];  // Non-const reference so we can modify
        QString band = qso.getBand();
        
        // If band is empty, derive it from frequency
        if (band.isEmpty()) {
            double freq = qso.getFrequency().toDouble();
            band = freqToBand(freq);
        }
        
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
        
        // Calculate points for this QSO
        int qsoPoints = calculatePoints(qso, myCallsign);
        qso.setPoints(qsoPoints);  // Update the QSO with its points
        
        // Initialize multiplier counts for this QSO
        int qsoNamedMults = 0;  // Named multipliers (states/provinces)
        int qsoDxccMults = 0;   // DXCC multipliers
        
        // Skip multiplier and band stat tracking for out-of-band or duplicate QSOs
        if (qso.isDupe()) {
            qso.setMultiplierCount(qsoNamedMults);
            qso.setDxccCount(qsoDxccMults);
            continue;
        }
        if (qsoPoints == 0 && !isValidBand(qso.getFrequency().toDouble())) {  // Already in kHz
            qso.setMultiplierCount(qsoNamedMults);
            qso.setDxccCount(qsoDxccMults);
            continue;
        }
        
        // This is a valid QSO
        validQsoCount++;
        
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
        
        m_runningScore.bandStats[band].points += qsoPoints;
        m_runningScore.contactScore += qsoPoints;
        
        // Track DXCC entities (always, for informational purposes)
        if (m_dxccDatabase) {
            DxccEntity entity = m_dxccDatabase->lookupCallsign(qso.getCall());
            if (entity.dxcc > 0) {
                uniqueDxccEntities.insert(entity.dxcc);
            }
        }
        
        // Track multipliers with categories
        QList<MultiplierInfo> multsWithCategory = getMultipliersWithCategory(qso);
        for (const MultiplierInfo& multInfo : multsWithCategory) {
            const QString& mult = multInfo.value;
            const QString& category = multInfo.category;
            
            // Track whether this mult is new for the QSO-specific count
            bool isNew = false;
            
            if (multType == "multsOnce") {
                if (!uniqueMultipliers.contains(mult)) {
                    isNew = true;
                    uniqueMultipliers.insert(mult);
                }
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsOnce.contains(mult)) namedMultsOnce.insert(mult);
                } else if (category == "dxcc") {
                    if (!dxccMultsOnce.contains(mult)) dxccMultsOnce.insert(mult);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsOnce.contains(mult)) ituRegionMultsOnce.insert(mult);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Mult tracking (once): %1 [%2] %3").arg(mult).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            } else if (multType == "multsPerBand") {
                QString key = QString("%1_%2").arg(mult).arg(band);
                if (!multPerBand.contains(key)) {
                    isNew = true;
                    multPerBand.insert(key);
                }
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsPerBand.contains(key)) namedMultsPerBand.insert(key);
                } else if (category == "dxcc") {
                    if (!dxccMultsPerBand.contains(key)) dxccMultsPerBand.insert(key);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsPerBand.contains(key)) ituRegionMultsPerBand.insert(key);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Mult tracking (per band): %1 [%2] %3").arg(key).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            } else if (multType == "multsPerMode") {
                QString key = QString("%1_%2").arg(mult).arg(modeCategory);
                if (!multPerMode.contains(key)) {
                    isNew = true;
                    multPerMode.insert(key);
                    
                    // Add to mode-specific sets only for NEW multipliers
                    if (modeCategory == "CW") {
                        cwMultipliers.insert(mult);
                    } else if (modeCategory == "SSB") {
                        ssbMultipliers.insert(mult);
                    } else if (modeCategory == "DIGITAL") {
                        digitalMultipliers.insert(mult);
                    }
                }
                
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsPerMode.contains(key)) namedMultsPerMode.insert(key);
                } else if (category == "dxcc") {
                    if (!dxccMultsPerMode.contains(key)) dxccMultsPerMode.insert(key);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsPerMode.contains(key)) ituRegionMultsPerMode.insert(key);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Mult tracking (per mode): %1 [%2] %3").arg(key).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            } else if (multType == "multsPerBandAndMode") {
                QString key = QString("%1_%2_%3").arg(mult).arg(band).arg(modeCategory);
                if (!multPerBandAndMode.contains(key)) {
                    isNew = true;
                    multPerBandAndMode.insert(key);
                }
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsPerBandAndMode.contains(key)) namedMultsPerBandAndMode.insert(key);
                } else if (category == "dxcc") {
                    if (!dxccMultsPerBandAndMode.contains(key)) dxccMultsPerBandAndMode.insert(key);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsPerBandAndMode.contains(key)) ituRegionMultsPerBandAndMode.insert(key);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Mult tracking (per band/mode): %1 [%2] %3").arg(key).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            }
            
            // Count this QSO's contribution to named and DXCC mults
            if (isNew) {
                if (category == "named" || category == "namedMults") {
                    qsoNamedMults++;
                } else if (category == "dxcc") {
                    qsoDxccMults++;
                }
            }
        }
        
        // Set the multiplier counts for this QSO
        qso.setMultiplierCount(qsoNamedMults);
        qso.setDxccCount(qsoDxccMults);
    }
    
    // Count multipliers based on type
    if (multType == "multsOnce") {
        m_runningScore.namedMultCount = namedMultsOnce.size();
        m_runningScore.dxccMultCount = dxccMultsOnce.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsOnce.size();
        m_runningScore.multipliers = uniqueMultipliers.size();
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Unique mults (once): %1 total (named:%2 dxcc:%3 itu:%4)")
                    .arg(uniqueMultipliers.size())
                    .arg(m_runningScore.namedMultCount)
                    .arg(m_runningScore.dxccMultCount)
                    .arg(m_runningScore.ituRegionMultCount));
        }
    } else if (multType == "multsPerBand") {
        m_runningScore.namedMultCount = namedMultsPerBand.size();
        m_runningScore.dxccMultCount = dxccMultsPerBand.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsPerBand.size();
        m_runningScore.multipliers = multPerBand.size();
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Mults per band: %1 total (named:%2 dxcc:%3 itu:%4)")
                    .arg(multPerBand.size())
                    .arg(m_runningScore.namedMultCount)
                    .arg(m_runningScore.dxccMultCount)
                    .arg(m_runningScore.ituRegionMultCount));
        }
    } else if (multType == "multsPerMode") {
        m_runningScore.namedMultCount = namedMultsPerMode.size();
        m_runningScore.dxccMultCount = dxccMultsPerMode.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsPerMode.size();
        m_runningScore.multipliers = multPerMode.size();
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Mults per mode: %1 total (named:%2 dxcc:%3 itu:%4)")
                    .arg(multPerMode.size())
                    .arg(m_runningScore.namedMultCount)
                    .arg(m_runningScore.dxccMultCount)
                    .arg(m_runningScore.ituRegionMultCount));
        }
    } else if (multType == "multsPerBandAndMode") {
        m_runningScore.namedMultCount = namedMultsPerBandAndMode.size();
        m_runningScore.dxccMultCount = dxccMultsPerBandAndMode.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsPerBandAndMode.size();
        m_runningScore.multipliers = multPerBandAndMode.size();
        DebugLogger::instance().log("ContestEngine", 
            QString("Mults per band/mode: %1 total (named:%2 dxcc:%3 itu:%4)")
                .arg(multPerBandAndMode.size())
                .arg(m_runningScore.namedMultCount)
                .arg(m_runningScore.dxccMultCount)
                .arg(m_runningScore.ituRegionMultCount));
    }
    
    // Set DXCC count (for informational purposes)
    m_runningScore.dxccCount = uniqueDxccEntities.size();
    
    // Calculate final contest score
    // Check if contest uses category-based scoring (states + provinces + dxcc)
    m_runningScore.bonusPoints = 0; // Hard-coded to 0 for now
    
    QStringList multCategories = getMultiplierCategories();
    
    if (multType == "multsPerMode") {
        // For multsPerMode: multipliers are counted per mode, but final score is points × total_mults
        // Total points = sum of all points
        // Total mults = sum of unique mults across all modes
        int totalMults = cwMultipliers.size() + ssbMultipliers.size() + digitalMultipliers.size();
        m_runningScore.contestScore = (m_runningScore.contactScore * totalMults) + m_runningScore.bonusPoints;
        
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Per-mode scoring: %1 pts × %2 mults (CW:%3 + SSB:%4 + Digital:%5) = %6")
                    .arg(m_runningScore.contactScore).arg(totalMults)
                    .arg(cwMultipliers.size()).arg(ssbMultipliers.size()).arg(digitalMultipliers.size())
                    .arg(m_runningScore.contestScore));
        }
    } else if (multType == "multsPerBandAndMode") {
        // For multsPerBandAndMode: multipliers counted per band/mode combo, but final score is points × total_mults
        // Total mults = count of unique mult_band_mode combinations
        int totalMults = multPerBandAndMode.size();
        m_runningScore.contestScore = (m_runningScore.contactScore * totalMults) + m_runningScore.bonusPoints;
        
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Per-band/mode scoring: %1 pts × %2 mults = %3")
                    .arg(m_runningScore.contactScore).arg(totalMults).arg(m_runningScore.contestScore));
        }
    } else if (!multCategories.isEmpty()) {
        // For multsOnce and multsPerBand: use simple points × mults formula
        int totalMultipliers = m_runningScore.namedMultCount + m_runningScore.dxccMultCount + m_runningScore.ituRegionMultCount;
        m_runningScore.contestScore = (m_runningScore.contactScore * totalMultipliers) + m_runningScore.bonusPoints;
    } else {
        // Traditional scoring: points * mults
        m_runningScore.contestScore = (m_runningScore.contactScore * m_runningScore.multipliers) + m_runningScore.bonusPoints;
    }
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Running score updated: %1 QSOs, %2 points, %3 mults (%4 named+%5 dxcc+%6 itu) = %7 score")
            .arg(validQsoCount)
            .arg(m_runningScore.contactScore)
            .arg(m_runningScore.multipliers)
            .arg(m_runningScore.namedMultCount)
            .arg(m_runningScore.dxccMultCount)
            .arg(m_runningScore.ituRegionMultCount)
            .arg(m_runningScore.contestScore));
    
    // Debug: Log band stats
    DebugLogger::instance().log("ContestEngine", 
        QString("Band stats contains %1 bands").arg(m_runningScore.bandStats.size()));
    for (auto it = m_runningScore.bandStats.begin(); it != m_runningScore.bandStats.end(); ++it) {
        DebugLogger::instance().log("ContestEngine", 
            QString("  %1: CW=%2 SSB=%3 Digi=%4 Pts=%5")
                .arg(it.key())
                .arg(it.value().cwQsos)
                .arg(it.value().ssbQsos)
                .arg(it.value().digitalQsos)
                .arg(it.value().points));
    }
}

void ContestEngine::resetScore()
{
    m_runningScore = ContestScore();
    DebugLogger::instance().log("ContestEngine", "Score reset");
}

void ContestEngine::setRestrictedMode(const QString& mode)
{
    m_restrictedMode = mode;
    if (!mode.isEmpty()) {
        DebugLogger::instance().log("ContestEngine", 
            QString("Restricted mode set to: %1").arg(mode));
    }
}

QString ContestEngine::getRestrictedMode() const
{
    return m_restrictedMode;
}
