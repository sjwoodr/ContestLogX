/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "contestEngine.h"
#include "debugLogger.h"
#include "utils/callsignUtils.h"
#include <QJsonArray>
#include <QJsonDocument>
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
    m_validCallPrefixes.clear();
    m_inStateMults.clear();
    m_multAliases.clear();
    m_namedMultAliases.clear();

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
        
        // Load namedCallPrefixes (e.g., YB0-YB9, 7A-7I, 8A-8I for YBDX)
        if (validation.contains("namedCallPrefixes")) {
            QJsonArray prefixList = validation["namedCallPrefixes"].toArray();
            DebugLogger::instance().log("ContestEngine",
                QString("Loading %1 call prefixes").arg(prefixList.size()));

            for (const QJsonValue& val : prefixList) {
                QString prefix = val.toString().toUpper();
                m_validCallPrefixes.insert(prefix);
            }
        }

        // Load inStateMults (subset of namedMults for filtered exchange validation)
        if (validation.contains("inStateMults")) {
            QJsonArray inStateList = validation["inStateMults"].toArray();
            DebugLogger::instance().log("ContestEngine",
                QString("Loading %1 in-state multipliers").arg(inStateList.size()));

            for (const QJsonValue& val : inStateList) {
                m_inStateMults.insert(val.toString().toUpper());
            }
        }

        // Load namedMultAliases — unconditional 1:1 mapping (e.g., "5" → "05")
        if (validation.contains("namedMultAliases")) {
            QJsonObject aliasObj = validation["namedMultAliases"].toObject();
            for (auto it = aliasObj.begin(); it != aliasObj.end(); ++it) {
                m_namedMultAliases.insert(it.key().toUpper(), it.value().toString().toUpper());
            }
            DebugLogger::instance().log("ContestEngine",
                QString("Loaded %1 named mult alias(es)").arg(m_namedMultAliases.size()));
        }
    } else {
        DebugLogger::instance().log("ContestEngine", "No validation section found");
    }

    // Load multAliases — maps received exchange values to a different mult value
    // based on the operator's userPrompt answer.
    //
    // Two trigger forms:
    //   - promptValue (string)        — fires when getUserPromptValue(promptId) == promptValue
    //   - promptValueIn (array)       — fires when getUserPromptValue(promptId) is in the array
    //                                   (overrides promptValue if non-empty)
    //
    // Two mapping forms:
    //   - mapsTo (string)             — replace rawMult with this fixed string
    //                                   (e.g., FL counties → "FL" for in-state FL ops in FQP)
    //   - mapByPrefix (int)           — take rawMult.left(N) as the mult
    //                                   (e.g., 7QP 5-letter county codes → 2-char state prefix:
    //                                    WYALB → WY, ORDES → OR, AZAPH → AZ for 7th-area ops)
    //                                   Wins over mapsTo when both are present.
    if (contestDef.contains("multAliases")) {
        QJsonArray aliases = contestDef["multAliases"].toArray();
        for (const QJsonValue& v : aliases) {
            QJsonObject obj = v.toObject();
            MultAlias alias;
            alias.promptId    = obj["promptId"].toString();
            alias.promptValue = obj["promptValue"].toString();
            if (obj.contains("promptValueIn")) {
                QJsonArray arr = obj["promptValueIn"].toArray();
                for (const QJsonValue& av : arr) {
                    alias.promptValueIn.append(av.toString());
                }
            }
            alias.sourceList  = obj["sourceList"].toString();
            if (obj.contains("sourceValues")) {
                QJsonArray arr = obj["sourceValues"].toArray();
                for (const QJsonValue& av : arr) {
                    alias.sourceValues.append(av.toString().toUpper());
                }
            }
            alias.mapsTo      = obj["mapsTo"].toString().toUpper();
            alias.mapByPrefix = obj["mapByPrefix"].toInt(0);
            // A rule is well-formed if it has promptId and either a static
            // mapsTo or a non-zero mapByPrefix.
            const bool hasMapping = !alias.mapsTo.isEmpty() || alias.mapByPrefix > 0;
            if (!alias.promptId.isEmpty() && hasMapping) {
                m_multAliases.append(alias);
            }
        }
        DebugLogger::instance().log("ContestEngine",
            QString("Loaded %1 mult alias rule(s)").arg(m_multAliases.size()));
    }

    // Load bonusStations — array of groups, each with a point value and list of callsigns
    m_bonusStationGroups.clear();
    if (contestDef.contains("bonusStations")) {
        QJsonArray groups = contestDef["bonusStations"].toArray();
        for (const QJsonValue& gv : groups) {
            QJsonObject obj = gv.toObject();
            BonusStationGroup group;
            group.name       = obj["name"].toString();
            group.pointsEach = obj["pointsEach"].toInt(0);
            // Support both new "type" field and legacy "oneTimeOnly" bool
            if (obj.contains("type"))
                group.type = obj["type"].toString();
            else
                group.type = obj["oneTimeOnly"].toBool(true) ? "bonusOnce" : "bonusPerBandAndMode";
            for (const QJsonValue& sv : obj["stations"].toArray())
                group.stations.insert(sv.toString().toUpper());
            if (group.pointsEach > 0 && !group.stations.isEmpty())
                m_bonusStationGroups.append(group);
        }
        DebugLogger::instance().log("ContestEngine",
            QString("Loaded %1 bonus station group(s)").arg(m_bonusStationGroups.size()));
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

    // Reset score and worked-mult tracking so a freshly loaded contest starts clean.
    // For log-file loads, updateRunningScore() will repopulate these immediately after.
    resetScore();

    // Cache derived properties so hot-path scoring functions avoid repeated JSON parsing
    cacheContestProperties();

    return true;
}

void ContestEngine::cacheContestProperties()
{
    m_dxccCache.clear();

    // Multiplier type
    m_cachedMultType = "multsOnce";
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            m_cachedMultType = mults["type"].toString("multsOnce");

            m_cachedAkHiCountDxcc       = mults["alaskaAndHawaiiCountDxcc"].toBool(true);
            m_cachedUsAndCanadaCountDxcc = mults["usAndCanadaCountDxcc"].toBool(true);
            m_cachedIncludeWaeEntities  = mults["includeWaeEntities"].toBool(false);

            // Multiplier categories (may be station-class specific)
            // JSON key is "stationClassMultipliers"; also accept legacy "stationClassCategories"
            // Resolves from m_stationClass first, then falls back to userPrompt values
            m_cachedMultCategories.clear();
            {
                QString effectiveClass = m_stationClass;
                for (const QString& key : {"stationClassMultipliers", "stationClassCategories"}) {
                    if (mults.contains(key)) {
                        QJsonObject scCats = mults[key].toObject();
                        // If formal station class matches, use it
                        if (!effectiveClass.isEmpty() && scCats.contains(effectiveClass)) {
                            for (const QJsonValue& v : scCats[effectiveClass].toArray())
                                m_cachedMultCategories.append(v.toString());
                            break;
                        }
                        // Fallback: check if any userPrompt value matches a key
                        if (effectiveClass.isEmpty()) {
                            for (const QString& scKey : scCats.keys()) {
                                for (const QString& pv : m_userPromptValues.values()) {
                                    if (pv == scKey) {
                                        for (const QJsonValue& v : scCats[scKey].toArray())
                                            m_cachedMultCategories.append(v.toString());
                                        effectiveClass = scKey;
                                        break;
                                    }
                                }
                                if (!m_cachedMultCategories.isEmpty()) break;
                            }
                            if (!m_cachedMultCategories.isEmpty()) break;
                        }
                    }
                }
            }
            if (m_cachedMultCategories.isEmpty() && mults.contains("categories")) {
                for (const QJsonValue& v : mults["categories"].toArray())
                    m_cachedMultCategories.append(v.toString());
            }
        }
    }
    m_cachedDxccIsMult = m_cachedMultCategories.contains("dxcc");
    m_cachedEadx100IsMult = m_cachedMultCategories.contains("eadx100");

    // Load optional eadx100Excludes — entity prefixes that should be skipped
    // when crediting eadx100 mults (e.g., King of Spain excludes EA/EA6/EA8/EA9
    // because those entities are tracked via the namedMults province list,
    // not the EADX-100 country list).
    m_cachedEadx100Excludes.clear();
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("multipliers")) {
            QJsonObject mults = scoring["multipliers"].toObject();
            if (mults.contains("eadx100Excludes")) {
                QJsonArray arr = mults["eadx100Excludes"].toArray();
                for (const QJsonValue& v : arr)
                    m_cachedEadx100Excludes.insert(v.toString().toUpper());
            }
        }
    }
    if (m_cachedEadx100IsMult) {
        DebugLogger::instance().log("ContestEngine",
            QString("eadx100 multiplier category enabled (%1 prefix exclusion(s))")
                .arg(m_cachedEadx100Excludes.size()));
    }

    // Score multiplier (e.g., power category: QRP ×3, Low ×2, High ×1)
    m_cachedScoreMultiplierPromptId.clear();
    m_cachedScoreMultiplierValues.clear();
    if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("scoreMultiplier")) {
            QJsonObject sm = scoring["scoreMultiplier"].toObject();
            m_cachedScoreMultiplierPromptId = sm["promptId"].toString();
            QJsonObject vals = sm["values"].toObject();
            for (auto it = vals.begin(); it != vals.end(); ++it)
                m_cachedScoreMultiplierValues[it.key()] = it.value().toInt(1);
        }
    }

    // Whether the callsign itself is a multiplier
    m_cachedCallsignIsMult = false;
    if (m_contestDef.contains("multipliers")) {
        QJsonObject multipliers = m_contestDef["multipliers"].toObject();
        for (const QJsonValue& val : multipliers["sources"].toArray()) {
            if (val.toObject()["type"].toString() == "callsign") {
                m_cachedCallsignIsMult = true;
                break;
            }
        }
    }

    // Dupe scope
    m_cachedDupeScope = "overall";
    if (m_contestDef.contains("dupeChecking")) {
        QJsonValue dcv = m_contestDef["dupeChecking"];
        if (dcv.isObject()) {
            QString t = dcv.toObject()["type"].toString();
            if      (t == "perBand")        m_cachedDupeScope = "per_band";
            else if (t == "perBandAndMode") m_cachedDupeScope = "per_band_mode";
            else if (t == "perMode")        m_cachedDupeScope = "per_mode";
            else if (t == "overall")        m_cachedDupeScope = "overall";
        } else if (dcv.isString()) {
            m_cachedDupeScope = dcv.toString();
        }
    } else if (m_contestDef.contains("scoring")) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("dupeChecking"))
            m_cachedDupeScope = scoring["dupeChecking"].toString();
    }
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
    
    // Check if field is required before validating empty values
    if (value.isEmpty()) {
        bool isRequired = isFieldRequired(fieldName);
        DebugLogger::instance().log("ContestEngine", 
            QString("  Field '%1' is required: %2").arg(fieldName).arg(isRequired ? "true" : "false"));
        if (isRequired) {
            errorMsg = QString("%1 cannot be empty").arg(getFieldLabel(fieldName));
            return false;
        } else {
            // Field is optional and empty, so it's valid
            DebugLogger::instance().log("ContestEngine", 
                QString("  Field '%1' is optional, allowing empty value").arg(fieldName));
            return true;
        }
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
    } else if (type == "string" && (fieldName == "EXCH" || fieldName == "EXCHr" || fieldName == "EXCHs")) {
        // For EXCH fields, check validation section for special logic
        if (m_contestDef.contains("validation")) {
            QJsonObject validation = m_contestDef["validation"].toObject();
            if (validation.contains("exchangeValidation")) {
                QJsonObject exchVal = validation["exchangeValidation"].toObject();
                QString validationType = exchVal["type"].toString();

                DebugLogger::instance().log("ContestEngine",
                    QString("  Exchange validation type: %1").arg(validationType));

                if (validationType == "namedMultOrSerial") {
                    // Check if value is a valid multiplier, alias, OR a valid serial number
                    QString upper = value.toUpper();

                    // First check if it's a valid multiplier or alias
                    if (m_validMultipliers.contains(upper) || m_namedMultAliases.contains(upper)) {
                        DebugLogger::instance().log("ContestEngine",
                            QString("  '%1' is a valid multiplier").arg(upper));
                        return true;
                    }

                    // Otherwise check if it matches serial number format
                    QString serialFormat = exchVal["serialNumberFormat"].toString();
                    if (serialFormat.isEmpty())
                        serialFormat = exchVal["serialFormat"].toString();
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
                } else if ((validationType == "nameAndMultiplier" || validationType == "namedMultOnly")
                           && fieldName == "EXCHr") {
                    // Validate received exchange against effective mults (filtered by station type)
                    // Also accept namedMultAlias keys (e.g., "DC" → "MD", "5" → "05")
                    QString upper = value.toUpper();
                    QSet<QString> effectiveMults = getEffectiveValidMults();
                    if (effectiveMults.contains(upper) || m_namedMultAliases.contains(upper)) {
                        DebugLogger::instance().log("ContestEngine",
                            QString("  '%1' is a valid exchange").arg(upper));
                        return true;
                    }
                    errorMsg = QString("Invalid exchange: %1").arg(value);
                    DebugLogger::instance().log("ContestEngine",
                        QString("  '%1' not in effective mults (%2 entries) or aliases (%3)")
                            .arg(upper).arg(effectiveMults.size()).arg(m_namedMultAliases.size()));
                    return false;
                } else if (validationType == "namedMultOrDxcc" && fieldName == "EXCHr") {
                    // Validate received exchange against effective mults, with a DXCC prefix
                    // fallback for station types that use the full mult list (e.g., FL in-state ops
                    // who may receive a DXCC entity prefix from a DX station).
                    // Station types restricted to inStateMults (WVE/DX operators in FQP) do NOT
                    // get the DXCC fallback, preventing them from entering DXCC prefixes instead
                    // of the required FL county abbreviation.
                    QString upper = value.toUpper();
                    QSet<QString> effectiveMults = getEffectiveValidMults();
                    if (effectiveMults.contains(upper) || m_namedMultAliases.contains(upper)) {
                        DebugLogger::instance().log("ContestEngine",
                            QString("  '%1' is a valid named mult").arg(upper));
                        return true;
                    }
                    // DXCC fallback — only for unrestricted operators (full namedMults).
                    // Uses exact prefix lookup (not fuzzy callsign matching) so that
                    // strings like "MOO" don't accidentally match via prefix stripping.
                    if (effectiveMults == m_validMultipliers &&
                        m_dxccDatabase && m_dxccDatabase->isLoaded()) {
                        if (m_dxccDatabase->isKnownPrefix(upper)) {
                            DebugLogger::instance().log("ContestEngine",
                                QString("  '%1' is a known DXCC prefix").arg(upper));
                            return true;
                        }
                    }
                    errorMsg = QString("Invalid exchange: %1").arg(value);
                    DebugLogger::instance().log("ContestEngine",
                        QString("  '%1' not in effective mults and not a known DXCC prefix").arg(upper));
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
    
    // Validate RECEIVED exchange fields only (not sent, which are auto-generated)
    if (m_contestDef.contains("exchangeFields")) {
        QJsonObject exchangeFields = m_contestDef["exchangeFields"].toObject();
        if (exchangeFields.contains("received")) {
            QJsonArray recvFields = exchangeFields["received"].toArray();
            for (const QJsonValue& val : recvFields) {
                QJsonObject fieldObj = val.toObject();
                QString fieldName = fieldObj["name"].toString();
                
                // Get the value from the QsoRecord
                QString value;
                if (fieldName == "RST") {
                    value = qso.getRstReceived();
                } else if (fieldName == "EXCH") {
                    value = qso.getExchangeReceived();
                } else {
                    value = qso.getExchangeField(fieldName);
                }
                
                QString err;
                if (!validateExchange(fieldName, value, err)) {
                    errorMsg = err;
                    return false;
                }
            }
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
    
    for (int i = 0; i < existingQsos.count(); ++i) {
        const QsoRecord& existing = existingQsos[i];
        // Skip only out-of-band QSOs, but NOT dupes - we want to detect dupes even if they're already marked as dupes
        // Also skip QSOs with invalid band data (empty band string)
        if (existing.isOutOfBand() || existing.getBand().isEmpty()) {
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
        // Skip only out-of-band QSOs, but NOT dupes - we want to detect dupes even if they're already marked as dupes
        if (existing.isOutOfBand()) {
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
    return m_cachedDupeScope;
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
    
    DxccEntity myEntity = dxccLookup(myCallForLookup);
    DxccEntity theirEntity = dxccLookup(theirCallForLookup);
    
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

        // Check if the partner station is an invalid contact for this station class
        // (e.g., W/VE stations may not work other W/VE stations in ARRL DX)
        if (scoring.contains("invalidPartners")) {
            QJsonObject invalidPartners = scoring["invalidPartners"].toObject();
            // Resolve effective class: formal station class or userPrompt value
            QString effectiveClass = m_stationClass;
            if (effectiveClass.isEmpty()) {
                for (const QString& ipKey : invalidPartners.keys()) {
                    if (ipKey.startsWith("_")) continue;
                    for (const QString& pv : m_userPromptValues.values()) {
                        if (pv == ipKey) { effectiveClass = ipKey; break; }
                    }
                    if (!effectiveClass.isEmpty()) break;
                }
            }
            if (!effectiveClass.isEmpty() && invalidPartners.contains(effectiveClass)) {
                QString theirPrefix = theirEntity.primaryPrefix;
                for (const QJsonValue& v : invalidPartners[effectiveClass].toArray()) {
                    if (v.toString() == theirPrefix) {
                        DebugLogger::instance().log("ContestEngine",
                            QString("  Partner %1 (%2) is invalid for station class %3 - 0 points")
                                .arg(theirCall).arg(theirPrefix).arg(effectiveClass));
                        return 0;
                    }
                }
            }
        }

        // Check if worked station's callsign suffix overrides mode-based points
        // (e.g., VAQP: /M contacts are worth 3 pts regardless of mode)
        if (scoring.contains("mobilePoints") && scoring.contains("mobileSuffixes")) {
            int mobilePoints = scoring["mobilePoints"].toInt();
            QString callUpper = theirCall.toUpper();
            for (const QJsonValue& sfx : scoring["mobileSuffixes"].toArray()) {
                if (callUpper.endsWith(sfx.toString().toUpper())) {
                    DebugLogger::instance().log("ContestEngine",
                        QString("  Points: %1 (mobile suffix)").arg(mobilePoints));
                    return mobilePoints;
                }
            }
        }

        if (scoring.contains("points")) {
            QJsonObject points = scoring["points"].toObject();

            // Check for simple perQso scoring first
            if (points.contains("perQso")) {
                int pts = points["perQso"].toInt();
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Points: %1 (perQso)").arg(pts));
                return pts;
            }
            
            // Check for band-based scoring (e.g., ARRL VHF Contest, JIDX)
            if (points.contains("byBand")) {
                QJsonObject byBand = points["byBand"].toObject();
                QString band = qso.getBand();

                // Try direct band lookup first (e.g., {"160m": 4, "80m": 2})
                if (byBand.contains(band)) {
                    QJsonValue val = byBand[band];
                    if (val.isDouble()) {
                        int pts = val.toInt();
                        DebugLogger::instance().log("ContestEngine",
                            QString("  Points: %1 (byBand direct)").arg(pts));
                        return pts;
                    }
                }

                // Fall back to month-nested lookup (e.g., {"january": {"6m": 1}})
                QString contestMonth = m_userPromptValues.value("contestMonth", "january").toLower();
                DebugLogger::instance().log("ContestEngine",
                    QString("  Looking for byBand/%1 for band '%2'").arg(contestMonth, band));

                if (byBand.contains(contestMonth)) {
                    QJsonObject monthPoints = byBand[contestMonth].toObject();
                    if (monthPoints.contains(band)) {
                        int pts = monthPoints[band].toInt();
                        DebugLogger::instance().log("ContestEngine",
                            QString("  Points: %1 (byBand/%2)").arg(pts).arg(contestMonth));
                        return pts;
                    }
                } else {
                    DebugLogger::instance().log("ContestEngine",
                        QString("  byBand/%1 not found in points").arg(contestMonth));
                }
            }
            
            QString mode = qso.getMode().toUpper();
            
            // Normalize mode names
            if (mode == "SSB" || mode == "USB" || mode == "LSB" || mode == "FM") {
                mode = "SSB";
            } else if (mode == "RTTY" || mode == "PSK" || mode == "FT8" || mode == "FT4" ||
                       mode == "DIGI" || mode == "DIGITAL" || mode == "USB-D" || mode == "LSB-D") {
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
                } else if (rule.startsWith("bothIn") && rule.length() == 8) {
                    // "bothInXX" rule: matches when BOTH stations are located in
                    // continent XX (e.g., bothInNA for the CQ WPX NA-NA exception
                    // worth 2/4 pts vs. the 1/2 pts other intra-continent QSOs earn).
                    QString cont = rule.mid(6);
                    ruleApplies = (myContinent == cont && theirContinent == cont
                                   && !myContinent.isEmpty());
                } else {
                    // Generic prefix-match rule: check for a "<ruleName>Prefixes" array
                    QString prefixKey = rule + "Prefixes";
                    if (scoring.contains(prefixKey)) {
                        QJsonArray prefixList = scoring[prefixKey].toArray();
                        for (const auto& v : prefixList) {
                            if (v.toString() == theirEntity.primaryPrefix) {
                                ruleApplies = true;
                                break;
                            }
                        }
                    }
                }

                // Check if a condition restricts this rule to specific station types
                if (ruleApplies && scoring.contains("scoringRuleConditions")) {
                    QJsonObject conditions = scoring["scoringRuleConditions"].toObject();
                    if (conditions.contains(rule)) {
                        QJsonObject cond = conditions[rule].toObject();
                        QString promptId = cond["promptId"].toString();
                        QJsonArray vals = cond["values"].toArray();
                        QString actual = m_userPromptValues.value(promptId);
                        bool met = false;
                        for (const QJsonValue& v : vals)
                            if (v.toString() == actual) { met = true; break; }
                        if (!met) {
                            ruleApplies = false;
                            DebugLogger::instance().log("ContestEngine",
                                QString("  Rule '%1' skipped: condition not met (prompt '%2' = '%3')")
                                    .arg(rule, promptId, actual));
                        }
                    }
                }
                
                // If rule applies and points are defined, return them
                if (ruleApplies && points.contains(rule)) {
                    QJsonObject rulePoints = points[rule].toObject();

                    // Check for per-band point tiers within a rule (e.g., CQ WPX:
                    // sameContinent worth 1 pt on 28/21/14 MHz but 2 pts on 7/3.5/1.8 MHz).
                    // Band keys use the existing "20m"/"40m"/etc. naming.
                    if (rulePoints.contains("byBand")) {
                        QJsonObject byBand = rulePoints["byBand"].toObject();
                        if (byBand.contains(band)) {
                            QJsonObject bandPoints = byBand[band].toObject();
                            if (bandPoints.contains(mode)) {
                                int pts = bandPoints[mode].toInt();
                                DebugLogger::instance().log("ContestEngine",
                                    QString("  Points: %1 (%2, byBand %3)").arg(pts).arg(rule, band));
                                return pts;
                            }
                        }
                    }

                    // Check for per-prompt point overrides (e.g., sameDxccEntity = 1 for DX, 2 for OK/OM)
                    if (rulePoints.contains("byPrompt")) {
                        QJsonObject byPrompt = rulePoints["byPrompt"].toObject();
                        for (const QString& promptId : byPrompt.keys()) {
                            QString actual = m_userPromptValues.value(promptId);
                            QJsonObject promptVals = byPrompt[promptId].toObject();
                            if (promptVals.contains(actual)) {
                                QJsonObject overridePoints = promptVals[actual].toObject();
                                if (overridePoints.contains(mode)) {
                                    int pts = overridePoints[mode].toInt();
                                    DebugLogger::instance().log("ContestEngine",
                                        QString("  Points: %1 (%2, byPrompt %3=%4)").arg(pts).arg(rule, promptId, actual));
                                    return pts;
                                }
                            }
                        }
                    }

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
                    DxccEntity entity = dxccLookup(qso.getCall());
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
        DxccEntity entity = dxccLookup(qso.getCall());
        
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

    // Check if the partner station is invalid for this station class (no mults awarded)
    if (m_cachedDxccIsMult && m_dxccDatabase) {
        QJsonObject scoring = m_contestDef["scoring"].toObject();
        if (scoring.contains("invalidPartners")) {
            QJsonObject invalidPartners = scoring["invalidPartners"].toObject();
            // Resolve effective class: formal station class or userPrompt value
            QString effectiveClass = m_stationClass;
            if (effectiveClass.isEmpty()) {
                for (const QString& ipKey : invalidPartners.keys()) {
                    if (ipKey.startsWith("_")) continue;
                    for (const QString& pv : m_userPromptValues.values()) {
                        if (pv == ipKey) { effectiveClass = ipKey; break; }
                    }
                    if (!effectiveClass.isEmpty()) break;
                }
            }
            if (!effectiveClass.isEmpty() && invalidPartners.contains(effectiveClass)) {
                DxccEntity theirEntity = dxccLookup(qso.getCall());
                QString theirPrefix = theirEntity.primaryPrefix;
                for (const QJsonValue& v : invalidPartners[effectiveClass].toArray()) {
                    if (v.toString() == theirPrefix) {
                        DebugLogger::instance().log("ContestEngine",
                            QString("  Partner %1 (%2) is invalid for station class %3 - no mults")
                                .arg(qso.getCall()).arg(theirPrefix).arg(effectiveClass));
                        return result;  // empty
                    }
                }
            }
        }
    }

    if (m_cachedCallsignIsMult) {
        result.append({qso.getCall().toUpper(), "callsign"});
        return result;
    }

    // Extract named multiplier from exchange (state/province/DX)
    QString mult = extractMultiplier(qso);
    if (!mult.isEmpty() && m_validMultipliers.contains(mult.toUpper())) {
        const QString multUpper = mult.toUpper();
        result.append({multUpper, "namedMults"});

        // AK/HI may also count as DXCC
        if (m_cachedDxccIsMult && m_cachedAkHiCountDxcc
                && (multUpper == "AK" || multUpper == "HI") && m_dxccDatabase) {
            DxccEntity entity = dxccLookup(qso.getCall());
            if (!entity.primaryPrefix.isEmpty() || !entity.prefixes.isEmpty()) {
                QString dxccMult = entity.primaryPrefix.isEmpty()
                                   ? entity.prefixes.first().prefix : entity.primaryPrefix;
                result.append({dxccMult, "dxcc"});
            }
        }
    }

    // DXCC multiplier
    if (m_cachedDxccIsMult && m_dxccDatabase) {
        const QString multUpper = mult.toUpper();
        bool alreadyAddedDxcc = (multUpper == "AK" || multUpper == "HI") && m_cachedAkHiCountDxcc;

        bool shouldAddDxcc = false;
        if (!alreadyAddedDxcc) {
            if (multUpper == "AK" || multUpper == "HI") {
                // AK/HI not counting as DXCC — skip
            } else if (!mult.isEmpty() && m_validMultipliers.contains(multUpper)) {
                shouldAddDxcc = m_cachedUsAndCanadaCountDxcc;
            } else {
                // Exchange not a named mult. If usAndCanadaCountDxcc is false, still check
                // whether the callsign itself resolves to a US/Canada entity — e.g., a VE
                // station sending power (not a state/province) in ARRL DX W/VE class.
                if (!m_cachedUsAndCanadaCountDxcc) {
                    DxccEntity callEntity = dxccLookup(qso.getCall());
                    QString prefix = callEntity.primaryPrefix.isEmpty() && !callEntity.prefixes.isEmpty()
                                     ? callEntity.prefixes.first().prefix : callEntity.primaryPrefix;
                    bool isUsOrCanada = (prefix == "K" || prefix == "VE");
                    shouldAddDxcc = !isUsOrCanada;
                } else {
                    shouldAddDxcc = true; // DX station
                }
            }
        }

        if (shouldAddDxcc) {
            DxccEntity entity = dxccLookup(qso.getCall());
            if (!entity.prefixes.isEmpty()) {
                if (!entity.waeOnly || m_cachedIncludeWaeEntities) {
                    QString dxccMult = entity.primaryPrefix.isEmpty()
                                       ? entity.prefixes.first().prefix : entity.primaryPrefix;
                    result.append({dxccMult, "dxcc"});
                }
            }
        }
    }

    // EADX-100 multiplier — independent of dxcc/namedMults. Used by URE-sponsored
    // contests (King of Spain, etc.) that score against URE's curated entity list
    // rather than ARRL DXCC. Entries listed in eadx100Excludes are skipped (the
    // contest tracks those via namedMults instead — KoS uses this to avoid double-
    // counting EA/EA6/EA8/EA9 since Spanish stations contribute province mults).
    if (m_cachedEadx100IsMult && m_eadxDatabase && m_eadxDatabase->isLoaded()) {
        QString eadxPrefix = m_eadxDatabase->getEntityPrefix(qso.getCall());
        if (!eadxPrefix.isEmpty() && !m_cachedEadx100Excludes.contains(eadxPrefix.toUpper())) {
            result.append({eadxPrefix, "eadx100"});
        }
    }

    // Named call-prefix multiplier (e.g. YBDX contest)
    if (m_cachedMultCategories.contains("namedCallPrefixes") && !m_validCallPrefixes.isEmpty()) {
        QString call = qso.getCall().toUpper();
        
        // Handle portable/slash notation: "YB1AR/2" -> YB2, "YC2DO/3" -> YC3, "YB0/KY1A" -> YB0, "W4WOD/2" -> no credit
        if (call.contains('/')) {
            int slashPos = call.indexOf('/');
            QString beforeSlash = call.left(slashPos);
            QString afterSlash = call.mid(slashPos + 1).trimmed();
            
            DebugLogger::instance().log("ContestEngine", 
                QString("  Call has slash notation: %1, prefix: '%2', suffix: '%3'").arg(call).arg(beforeSlash).arg(afterSlash));
            
            // Try to match the suffix against valid prefixes
            if (!afterSlash.isEmpty()) {
                // First, try to find the base call's prefix
                QString basePrefix;
                int longestBaseMatch = 0;
                for (const QString& prefix : m_validCallPrefixes) {
                    if (beforeSlash.startsWith(prefix) && prefix.length() > longestBaseMatch) {
                        basePrefix = prefix;
                        longestBaseMatch = prefix.length();
                    }
                }
                
                // ONLY process slash notation if the base call has a valid prefix
                // This ensures we don't accidentally credit wrong prefixes (e.g., W4WOD/3 shouldn't get YC3)
                if (!basePrefix.isEmpty()) {
                    // Try to match suffix with same family first
                    // e.g., if YC2DO and suffix is /3, try YC3
                    if (basePrefix.length() >= 2 && afterSlash.length() <= 2) {
                        QString family = basePrefix.left(2);  // e.g., "YC", "YB"
                        QString candidatePrefix = family + afterSlash;
                        if (m_validCallPrefixes.contains(candidatePrefix)) {
                            result.append({candidatePrefix, "namedCallPrefixes"});
                            DebugLogger::instance().log("ContestEngine", 
                                QString("  Slash notation matched prefix '%1' (region variant of %2) from %3").arg(candidatePrefix).arg(basePrefix).arg(call));
                            return result;
                        }
                    }
                    
                    // If region variant didn't match, use base prefix (e.g., W4WOD/2 -> W4, or YB0/KY1A -> YB0)
                    result.append({basePrefix, "namedCallPrefixes"});
                    DebugLogger::instance().log("ContestEngine", 
                        QString("  Using base prefix '%1' (suffix '%2' is region/call modifier) from %3").arg(basePrefix).arg(afterSlash).arg(call));
                    return result;
                }
                
                // Base call doesn't have a valid prefix - no multiplier credit for this call
                DebugLogger::instance().log("ContestEngine", 
                    QString("  No valid base prefix for %1, suffix '%2' ignored (not crediting unknown prefix)").arg(beforeSlash).arg(afterSlash));
            }
        }
        
        // No slash or slash didn't match - try normal prefix matching
        // Match from longest to shortest prefix (e.g., try "YB0", "YB" before "Y")
        QString matchedPrefix;
        int longestMatch = 0;
        
        for (const QString& prefix : m_validCallPrefixes) {
            if (call.startsWith(prefix) && prefix.length() > longestMatch) {
                matchedPrefix = prefix;
                longestMatch = prefix.length();
            }
        }
        
        if (!matchedPrefix.isEmpty()) {
            result.append({matchedPrefix, "namedCallPrefixes"});
            DebugLogger::instance().log("ContestEngine", 
                QString("  Found call prefix multiplier '%1' from %2").arg(matchedPrefix).arg(call));
        } else {
            DebugLogger::instance().log("ContestEngine", 
                QString("  No matching call prefix found for %1").arg(call));
        }
    }
    
    // WPX-style prefix multiplier (CQ WPX contest). Unlike namedCallPrefixes,
    // the prefix is extracted dynamically from each callsign per CQ WPX rules
    // (handles portable designators, /<digit> call-area changes, license-class
    // suffix stripping, and zero-padding for callsigns without numbers).
    // Routed into the same accounting bucket as namedCallPrefixCount downstream.
    if (m_cachedMultCategories.contains("wpxPrefix")) {
        QString wpxPrefix = CallsignUtils::extractWpxPrefix(qso.getCall());
        if (!wpxPrefix.isEmpty()) {
            result.append({wpxPrefix, "wpxPrefix"});
            DebugLogger::instance().log("ContestEngine",
                QString("  Found WPX prefix multiplier '%1' from %2").arg(wpxPrefix).arg(qso.getCall()));
        }
    }

    // Check if Grid Squares are a multiplier category
    bool gridSquareIsMult = m_cachedMultCategories.contains("gridSquares");
    if (gridSquareIsMult) {
        // Extract grid square from exchange fields (typically in "GRID" or "GRID_RCV" field)
        QString gridSquare;
        
        // Try to find grid square in exchange fields
        if (qso.getExchangeFields().contains("GRID")) {
            gridSquare = qso.getExchangeField("GRID").toUpper();
        } else if (qso.getExchangeFields().contains("GRID_RCV")) {
            gridSquare = qso.getExchangeField("GRID_RCV").toUpper();
        } else if (qso.getExchangeFields().contains("GRIDr")) {
            gridSquare = qso.getExchangeField("GRIDr").toUpper();
        }
        
        if (!gridSquare.isEmpty()) {
            // Validate grid square format (basic: should be 4-6 characters)
            if (gridSquare.length() >= 4) {
                result.append({gridSquare, "gridSquares"});
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Found grid square multiplier '%1'").arg(gridSquare));
            }
        } else {
            DebugLogger::instance().log("ContestEngine", 
                QString("  Grid squares are multipliers but no grid found in exchange for %1").arg(qso.getCall()));
        }
    }
    
    return result;
}

ContestEngine::QsoMultiplierCredit ContestEngine::getQsoMultiplierCredit(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const
{
    QsoMultiplierCredit credit;
    
    // Get all multiplier info for this QSO
    QList<MultiplierInfo> multsWithCategory = getMultipliersWithCategory(qso);
    
    for (const MultiplierInfo& multInfo : multsWithCategory) {
        const QString trackingKey = buildMultTrackingKey(multInfo.value, qso.getBand(), qso.getMode());

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
                    if (trackingKey == buildMultTrackingKey(existingMult.value, existingQso.getBand(), existingQso.getMode())) {
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
            } else if (multInfo.category == "dxcc" || multInfo.category == "eadx100") {
                // dxcc and eadx100 cover ~99% the same entity set, so they share
                // a bucket. A contest uses one or the other, not both.
                credit.dxccMultCount++;
            } else if (multInfo.category == "ituRegions") {
                credit.ituRegionMultCount++;
            } else if (multInfo.category == "gridSquares") {
                credit.gridSquareMultCount++;
            }
        }
    }

    return credit;
}


DxccEntity ContestEngine::dxccLookup(const QString& call) const
{
    auto it = m_dxccCache.find(call);
    if (it != m_dxccCache.end())
        return it.value();
    DxccEntity entity = m_dxccDatabase->lookupCallsign(call);
    m_dxccCache.insert(call, entity);
    return entity;
}

QString ContestEngine::buildMultTrackingKey(const QString& multValue, const QString& band, const QString& mode) const
{
    if (m_cachedMultType == "multsPerBandAndMode") return multValue + "_" + band + "_" + mode;
    if (m_cachedMultType == "multsPerBand")        return multValue + "_" + band;
    if (m_cachedMultType == "multsPerMode")        return multValue + "_" + mode;
    return multValue; // multsOnce or unrecognised type
}

QString ContestEngine::getMultiplierType() const
{
    return m_cachedMultType;
}

QStringList ContestEngine::getMultiplierCategories() const
{
    return m_cachedMultCategories;
}

QString ContestEngine::getNamedMultsLabel() const
{
    if (m_contestDef.contains("scoring")) {
        QJsonObject mults = m_contestDef["scoring"].toObject()["multipliers"].toObject();
        if (mults.contains("namedMultsLabels")) {
            QJsonObject labels = mults["namedMultsLabels"].toObject();
            // Check formal station class first
            if (!m_stationClass.isEmpty() && labels.contains(m_stationClass))
                return labels[m_stationClass].toString();
            // Fall back to matching a userPrompt value
            for (const QString& pv : m_userPromptValues.values()) {
                if (labels.contains(pv))
                    return labels[pv].toString();
            }
        }
        // Fall back to a single label
        if (mults.contains("namedMultsLabel"))
            return mults["namedMultsLabel"].toString();
    }
    return QStringLiteral("Named Multipliers");
}

bool ContestEngine::getAlaskaHawaiiCountDxcc() const
{
    return m_cachedAkHiCountDxcc;
}

bool ContestEngine::getUsAndCanadaCountDxcc() const
{
    return m_cachedUsAndCanadaCountDxcc;
}

QStringList ContestEngine::getNamedMultiplierList() const
{
    QStringList result;
    if (m_contestDef.contains("validation")) {
        QJsonObject validation = m_contestDef["validation"].toObject();
        if (validation.contains("namedMults")) {
            QJsonArray multList = validation["namedMults"].toArray();
            for (const QJsonValue& val : multList) {
                result.append(val.toString().toUpper());
            }
        }
    }
    return result;
}

QSet<QString> ContestEngine::getEffectiveValidMults() const
{
    if (!m_inStateMults.isEmpty() && m_contestDef.contains("validation")) {
        QJsonObject validation = m_contestDef["validation"].toObject();
        if (validation.contains("receivedExchangeFilter")) {
            QJsonObject filter = validation["receivedExchangeFilter"].toObject();
            QString promptId = filter["promptId"].toString();
            QString promptValue = getUserPromptValue(promptId);
            QJsonObject rules = filter["rules"].toObject();
            DebugLogger::instance().log("MultiplierWidget",
                QString("getEffectiveValidMults: promptId='%1', promptValue='%2', hasRule=%3, inStateMults=%4, validMults=%5")
                    .arg(promptId, promptValue)
                    .arg(rules.contains(promptValue) ? "true" : "false")
                    .arg(m_inStateMults.size())
                    .arg(m_validMultipliers.size()));
            if (rules.contains(promptValue)) {
                QString target = rules[promptValue].toString();
                if (target == "inStateMults") {
                    DebugLogger::instance().log("MultiplierWidget",
                        QString("  -> returning inStateMults (%1 entries)").arg(m_inStateMults.size()));
                    return m_inStateMults;
                }
            }
        }
    }
    DebugLogger::instance().log("MultiplierWidget",
        QString("  -> returning full validMultipliers (%1 entries)").arg(m_validMultipliers.size()));
    return m_validMultipliers;
}

QStringList ContestEngine::getEffectiveNamedMultiplierList() const
{
    // If the active station class uses a different multiplier category (e.g. W/VE in
    // ARRL DX uses "dxcc" not "namedMults"), there is no fixed named-mult list to
    // display in the widget.
    if (!getMultiplierCategories().contains("namedMults")) {
        return QStringList();
    }

    // Check for station-class-specific named mult display list
    if (m_contestDef.contains("validation")) {
        QJsonObject validation = m_contestDef["validation"].toObject();
        if (validation.contains("namedMultsByStationClass")) {
            QJsonObject byClass = validation["namedMultsByStationClass"].toObject();
            // Check formal station class first
            if (!m_stationClass.isEmpty() && byClass.contains(m_stationClass)) {
                QStringList result;
                for (const QJsonValue& val : byClass[m_stationClass].toArray())
                    result.append(val.toString().toUpper());
                return result;
            }
            // Fall back to matching a userPrompt value
            for (const QString& pv : m_userPromptValues.values()) {
                if (byClass.contains(pv)) {
                    QStringList result;
                    for (const QJsonValue& val : byClass[pv].toArray())
                        result.append(val.toString().toUpper());
                    return result;
                }
            }
        }
    }

    QSet<QString> effective = getEffectiveValidMults();
    if (effective != m_validMultipliers) {
        // Operator is restricted to inStateMults (e.g., WVE/DX in FQP, out-of-state in MNQP)
        QStringList result;
        if (m_contestDef.contains("validation")) {
            QJsonObject validation = m_contestDef["validation"].toObject();
            if (validation.contains("inStateMults")) {
                QJsonArray inStateList = validation["inStateMults"].toArray();
                for (const QJsonValue& val : inStateList)
                    result.append(val.toString().toUpper());
            }
        }
        return result;
    }

    // Full namedMults path. If a multAlias is active for this operator, exclude the
    // aliased-source values from the display — they map to something else and would
    // just clutter the widget (e.g., FL counties shown to FL in-state ops who only
    // earn credit for the aliased "FL" state mult, not individual county entries;
    // 7QP 5-letter county codes shown to 7th-area ops who get prefix-aliased to
    // 2-letter state codes).
    QSet<QString> aliasedSources;
    for (const MultAlias& alias : m_multAliases) {
        const QString currentValue = getUserPromptValue(alias.promptId);
        bool triggered = false;
        if (!alias.promptValueIn.isEmpty()) {
            triggered = alias.promptValueIn.contains(currentValue);
        } else {
            triggered = (currentValue == alias.promptValue);
        }
        if (!triggered) continue;

        if (!alias.sourceValues.isEmpty()) {
            for (const QString& v : alias.sourceValues)
                aliasedSources.insert(v);
        } else if (alias.sourceList == "inStateMults") {
            aliasedSources += m_inStateMults;
        } else if (alias.sourceList == "namedMults") {
            aliasedSources += m_validMultipliers;
        }
    }

    if (aliasedSources.isEmpty())
        return getNamedMultiplierList();

    // Return namedMults in JSON order, skipping anything covered by an active alias
    QStringList result;
    if (m_contestDef.contains("validation")) {
        QJsonObject validation = m_contestDef["validation"].toObject();
        if (validation.contains("namedMults")) {
            QJsonArray multList = validation["namedMults"].toArray();
            for (const QJsonValue& val : multList) {
                QString upper = val.toString().toUpper();
                if (!aliasedSources.contains(upper))
                    result.append(upper);
            }
        }
    }
    return result;
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
    
    // Handle Objective Multipliers (WFD-style contests)
    if (multType == "objectiveMultipliers") {
        // Points come from QSOs, multiplier is OM count + 1
        for (const QsoRecord& qso : qsos) {
            totalPoints += calculatePoints(qso, "");
        }
        
        int omCount = calculateObjectiveMultiplierCount();
        totalMults = omCount + 1;  // Always add 1 even if no OMs claimed
        
        DebugLogger::instance().log("ContestEngine", 
            QString("Total score (objectiveMultipliers): %1 points × (%2 OM + 1) = %3")
                .arg(totalPoints).arg(omCount).arg(totalPoints * totalMults));
        
        return totalPoints * totalMults;
    }
    
    // Build multiplier tracking structure based on type (traditional multipliers)
    QSet<QString> uniqueMultipliers;                          // multsOnce
    QSet<QString> multPerBand;                                 // multsPerBand: mult_band
    QSet<QString> multPerMode;                                 // multsPerMode: mult_mode
    QSet<QString> multPerBandAndMode;                          // multsPerBandAndMode: mult_band_mode
    
    for (const QsoRecord& qso : qsos) {
        // TODO: Pass station callsign for proper scoring
        totalPoints += calculatePoints(qso, "");

        QList<MultiplierInfo> mults = getMultipliersWithCategory(qso);
        QString band = qso.getBand();
        QString mode = qso.getMode();

        for (const MultiplierInfo& multInfo : mults) {
            // Include category in the key so that the same exchange string in
            // different categories (e.g. "OH" as Ohio/namedMult vs Finland/dxcc)
            // are counted as distinct multipliers rather than deduplicated.
            QString key = QString("%1:%2").arg(multInfo.category).arg(multInfo.value);
            if (multType == "multsOnce") {
                uniqueMultipliers.insert(key);
            } else if (multType == "multsPerBand") {
                multPerBand.insert(QString("%1_%2").arg(key).arg(band));
            } else if (multType == "multsPerMode") {
                multPerMode.insert(QString("%1_%2").arg(key).arg(mode));
            } else if (multType == "multsPerBandAndMode") {
                multPerBandAndMode.insert(QString("%1_%2_%3").arg(key).arg(band).arg(mode));
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

    // If any digital mode is allowed, accept USB-D, LSB-D, DIGI, DIGITAL
    if (upperMode == "USB-D" || upperMode == "LSB-D" || upperMode == "DIGI" || upperMode == "DIGITAL") {
        for (const QString& a : allowed) {
            if (a == "RTTY" || a == "FT8" || a == "FT4" || a == "DIGI" || a == "DIGITAL" || a == "PSK") {
                return true;
            }
        }
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

QStringList ContestEngine::getAllowedBands() const
{
    if (m_contestDef.contains("contest")) {
        QJsonObject contest = m_contestDef["contest"].toObject();
        if (contest.contains("bands")) {
            QStringList bands;
            for (const QJsonValue& val : contest["bands"].toArray())
                bands.append(val.toString());
            return bands;
        }
    }
    return {};
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
    if (re.match(value).hasMatch())
        return true;

    // Digital (FT8/FT4) signal reports: optional sign + 1-2 digits (e.g., -15, +05, -02)
    QRegularExpression digitalRe("^[+-]?\\d{1,2}$");
    return digitalRe.match(value).hasMatch();
}

bool ContestEngine::isFieldRequired(const QString& fieldName) const
{
    // Check exchangeFields in contest definition
    if (m_contestDef.contains("exchangeFields")) {
        QJsonObject exchangeFields = m_contestDef["exchangeFields"].toObject();
        
        // Check both sent and received fields
        QJsonArray allFields;
        if (exchangeFields.contains("sent")) {
            for (const QJsonValue& v : exchangeFields["sent"].toArray())
                allFields.append(v);
        }
        if (exchangeFields.contains("received")) {
            for (const QJsonValue& v : exchangeFields["received"].toArray())
                allFields.append(v);
        }
        
        DebugLogger::instance().log("ContestEngine", 
            QString("isFieldRequired: checking field '%1' against %2 fields").arg(fieldName).arg(allFields.size()));
        
        for (const QJsonValue& val : allFields) {
            QJsonObject fieldObj = val.toObject();
            QString name = fieldObj["name"].toString();
            if (name == fieldName) {
                bool required = fieldObj.contains("required") ? fieldObj["required"].toBool() : true;
                DebugLogger::instance().log("ContestEngine", 
                    QString("  Found field '%1': required=%2").arg(name).arg(required ? "true" : "false"));
                return required;
            }
        }
        DebugLogger::instance().log("ContestEngine", 
            QString("  Field '%1' not found in exchangeFields, defaulting to required=true").arg(fieldName));
    } else {
        DebugLogger::instance().log("ContestEngine", 
            QString("isFieldRequired: no exchangeFields found in contest definition"));
    }
    
    // If not found in exchangeFields, default to required
    return true;
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
        if (cleanWord.isEmpty()) continue;

        // Direct match against valid multipliers
        if (m_validMultipliers.contains(cleanWord)) {
            QString resolved = applyMultAlias(cleanWord);
            DebugLogger::instance().log("ContestEngine",
                QString("Found multiplier: '%1'%2").arg(cleanWord,
                    resolved != cleanWord ? QString(" -> aliased to '%1'").arg(resolved) : QString()));
            return resolved;
        }

        // Check 1:1 named mult aliases (e.g., "5" → "05")
        if (m_namedMultAliases.contains(cleanWord)) {
            QString canonical = m_namedMultAliases.value(cleanWord);
            DebugLogger::instance().log("ContestEngine",
                QString("Found multiplier via alias: '%1' -> '%2'").arg(cleanWord, canonical));
            return canonical;
        }
    }

    DebugLogger::instance().log("ContestEngine",
        QString("No multiplier found in exchange (checked %1 valid mults, %2 aliases)")
            .arg(m_validMultipliers.size()).arg(m_namedMultAliases.size()));
    
    return QString();
}

QString ContestEngine::applyMultAlias(const QString& rawMult) const
{
    for (const MultAlias& alias : m_multAliases) {
        // Trigger match: promptValueIn (any-of) wins over promptValue (single)
        // when both are present, but if promptValueIn is empty we use the
        // legacy single-value comparison.
        const QString currentValue = getUserPromptValue(alias.promptId);
        bool triggered = false;
        if (!alias.promptValueIn.isEmpty()) {
            triggered = alias.promptValueIn.contains(currentValue);
        } else {
            triggered = (currentValue == alias.promptValue);
        }
        if (!triggered) continue;

        // sourceValues (inline exact-match list) wins over sourceList
        // (named list reference) when present. Lets one alias rule target
        // a specific subset of values without needing a named list.
        bool inSource = false;
        if (!alias.sourceValues.isEmpty()) {
            inSource = alias.sourceValues.contains(rawMult);
        } else {
            const QSet<QString>* sourceSet = nullptr;
            if (alias.sourceList == "inStateMults")
                sourceSet = &m_inStateMults;
            else if (alias.sourceList == "namedMults")
                sourceSet = &m_validMultipliers;
            inSource = (sourceSet && sourceSet->contains(rawMult));
        }

        if (inSource) {
            // mapByPrefix wins over mapsTo when present — extracts the
            // first N characters from rawMult (e.g., 7QP's 5-letter county
            // code WYALB with mapByPrefix=2 → WY).
            if (alias.mapByPrefix > 0 && rawMult.size() >= alias.mapByPrefix)
                return rawMult.left(alias.mapByPrefix);
            return alias.mapsTo;
        }
    }
    return rawMult;
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

QString ContestEngine::getStationClassExchangeType() const
{
    if (m_stationClass.isEmpty())
        return QString();

    if (m_contestDef.contains("stationClasses")) {
        QJsonObject stationClasses = m_contestDef["stationClasses"].toObject();
        if (stationClasses.contains("classes")) {
            QJsonArray classes = stationClasses["classes"].toArray();
            for (const QJsonValue& classVal : classes) {
                QJsonObject classObj = classVal.toObject();
                if (classObj["id"].toString() == m_stationClass) {
                    if (classObj.contains("exchangeSent"))
                        return classObj["exchangeSent"].toObject()["type"].toString();
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

bool ContestEngine::stationClassPromptsForCallsign() const
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
                    return classObj.value("promptForCallsign").toBool(false);
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

    // Clear worked named mult tracking sets
    m_workedNamedMults.clear();
    m_workedNamedMultsPerBand.clear();
    m_workedNamedMultsPerMode.clear();
    m_workedNamedMultsPerBandAndMode.clear();

    QString multType = getMultiplierType();
    
    // Track multipliers based on type
     QSet<QString> uniqueMultipliers;                    // multsOnce
    QSet<QString> multPerBand;                          // multsPerBand
    QSet<QString> multPerMode;                          // multsPerMode
    QSet<QString> multPerBandAndMode;                   // multsPerBandAndMode
    
    // For multsPerMode scoring: track multipliers per mode
    QSet<QString> cwMultipliers, ssbMultipliers, digitalMultipliers;
    
     // Track multipliers by type (named vs DXCC vs ITU Regions vs namedCallPrefixes vs gridSquares)
    QSet<QString> namedMultsOnce, dxccMultsOnce, ituRegionMultsOnce, namedCallPrefixesOnce, gridSquaresOnce;
    QSet<QString> namedMultsPerBand, dxccMultsPerBand, ituRegionMultsPerBand, namedCallPrefixesPerBand, gridSquaresPerBand;
    QSet<QString> namedMultsPerMode, dxccMultsPerMode, ituRegionMultsPerMode, namedCallPrefixesPerMode, gridSquaresPerMode;
    QSet<QString> namedMultsPerBandAndMode, dxccMultsPerBandAndMode, ituRegionMultsPerBandAndMode, namedCallPrefixesPerBandAndMode, gridSquaresPerBandAndMode;
    
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
        } else if (mode == "SSB" || mode == "USB" || mode == "LSB" || mode == "FM" || mode == "AM") {
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
        int qsoGridSquareMults = 0;  // Grid square multipliers
        
        // Skip multiplier and band stat tracking for out-of-band or duplicate QSOs
        if (qso.isDupe()) {
            qso.setPoints(0);
            qso.setMultiplierCount(qsoNamedMults);
            qso.setDxccCount(qsoDxccMults);
            qso.setGridSquareMultiplierCount(qsoGridSquareMults);
            continue;
        }
        if (qsoPoints == 0 && !isValidBand(qso.getFrequency().toDouble())) {  // Already in kHz
            qso.setMultiplierCount(qsoNamedMults);
            qso.setDxccCount(qsoDxccMults);
            qso.setGridSquareMultiplierCount(qsoGridSquareMults);
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
            DxccEntity entity = dxccLookup(qso.getCall());
            if (entity.dxcc > 0) {
                uniqueDxccEntities.insert(entity.dxcc);
            }
        }
        
        // For objectiveMultipliers, skip QSO-based multiplier extraction (no mults per QSO)
        if (multType == "objectiveMultipliers") {
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            qso.setGridSquareMultiplierCount(0);
            continue;
        }
        
        // Track multipliers with categories
        QList<MultiplierInfo> multsWithCategory = getMultipliersWithCategory(qso);
        for (const MultiplierInfo& multInfo : multsWithCategory) {
            const QString& mult = multInfo.value;
            const QString& category = multInfo.category;
            
            // Track whether this mult is new for the QSO-specific count
            bool isNew = false;
            
            if (multType == "multsOnce") {
                QString catMult = category + ":" + mult;
                if (!uniqueMultipliers.contains(catMult)) {
                    isNew = true;
                    uniqueMultipliers.insert(catMult);
                }
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsOnce.contains(mult)) namedMultsOnce.insert(mult);
                    m_workedNamedMults.insert(mult);
                } else if (category == "dxcc" || category == "eadx100") {
                    if (!dxccMultsOnce.contains(mult)) dxccMultsOnce.insert(mult);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsOnce.contains(mult)) ituRegionMultsOnce.insert(mult);
                } else if (category == "namedCallPrefixes" || category == "wpxPrefix") {
                    if (!namedCallPrefixesOnce.contains(mult)) namedCallPrefixesOnce.insert(mult);
                } else if (category == "gridSquares") {
                    if (!gridSquaresOnce.contains(mult)) gridSquaresOnce.insert(mult);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine",
                        QString("  Mult tracking (once): %1 [%2] %3").arg(mult).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            } else if (multType == "multsPerBand") {
                QString key = buildMultTrackingKey(category + ":" + mult, band, modeCategory);
                QString oldKey = buildMultTrackingKey(mult, band, modeCategory);
                if (!multPerBand.contains(key)) {
                    isNew = true;
                    multPerBand.insert(key);
                }
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsPerBand.contains(oldKey)) namedMultsPerBand.insert(oldKey);
                    m_workedNamedMultsPerBand.insert(oldKey);
                } else if (category == "dxcc" || category == "eadx100") {
                    if (!dxccMultsPerBand.contains(oldKey)) dxccMultsPerBand.insert(oldKey);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsPerBand.contains(oldKey)) ituRegionMultsPerBand.insert(oldKey);
                } else if (category == "namedCallPrefixes" || category == "wpxPrefix") {
                    if (!namedCallPrefixesPerBand.contains(oldKey)) namedCallPrefixesPerBand.insert(oldKey);
                } else if (category == "gridSquares") {
                    if (!gridSquaresPerBand.contains(oldKey)) gridSquaresPerBand.insert(oldKey);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine",
                        QString("  Mult tracking (per band): %1 [%2] %3").arg(key).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            } else if (multType == "multsPerMode") {
                QString catMult = category + ":" + mult;
                QString key = buildMultTrackingKey(catMult, band, modeCategory);
                QString oldKey = buildMultTrackingKey(mult, band, modeCategory);
                if (!multPerMode.contains(key)) {
                    isNew = true;
                    multPerMode.insert(key);

                    // Add to mode-specific sets only for NEW multipliers
                    if (modeCategory == "CW") {
                        cwMultipliers.insert(catMult);
                    } else if (modeCategory == "SSB") {
                        ssbMultipliers.insert(catMult);
                    } else if (modeCategory == "DIGITAL") {
                        digitalMultipliers.insert(catMult);
                    }
                }

                if (category == "named" || category == "namedMults") {
                    if (!namedMultsPerMode.contains(oldKey)) namedMultsPerMode.insert(oldKey);
                    m_workedNamedMultsPerMode.insert(oldKey);
                } else if (category == "dxcc" || category == "eadx100") {
                    if (!dxccMultsPerMode.contains(oldKey)) dxccMultsPerMode.insert(oldKey);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsPerMode.contains(oldKey)) ituRegionMultsPerMode.insert(oldKey);
                } else if (category == "namedCallPrefixes" || category == "wpxPrefix") {
                    if (!namedCallPrefixesPerMode.contains(oldKey)) namedCallPrefixesPerMode.insert(oldKey);
                } else if (category == "gridSquares") {
                    if (!gridSquaresPerMode.contains(oldKey)) gridSquaresPerMode.insert(oldKey);
                }
                if (verbose) {
                    DebugLogger::instance().log("ContestEngine",
                        QString("  Mult tracking (per mode): %1 [%2] %3").arg(key).arg(category).arg(isNew ? "NEW" : "DUPE"));
                }
            } else if (multType == "multsPerBandAndMode") {
                QString key = buildMultTrackingKey(category + ":" + mult, band, modeCategory);
                QString oldKey = buildMultTrackingKey(mult, band, modeCategory);
                if (!multPerBandAndMode.contains(key)) {
                    isNew = true;
                    multPerBandAndMode.insert(key);
                }
                if (category == "named" || category == "namedMults") {
                    if (!namedMultsPerBandAndMode.contains(oldKey)) namedMultsPerBandAndMode.insert(oldKey);
                    m_workedNamedMultsPerBandAndMode.insert(oldKey);
                } else if (category == "dxcc" || category == "eadx100") {
                    if (!dxccMultsPerBandAndMode.contains(oldKey)) dxccMultsPerBandAndMode.insert(oldKey);
                } else if (category == "ituRegions") {
                    if (!ituRegionMultsPerBandAndMode.contains(oldKey)) ituRegionMultsPerBandAndMode.insert(oldKey);
                } else if (category == "namedCallPrefixes" || category == "wpxPrefix") {
                    if (!namedCallPrefixesPerBandAndMode.contains(oldKey)) namedCallPrefixesPerBandAndMode.insert(oldKey);
                } else if (category == "gridSquares") {
                    if (!gridSquaresPerBandAndMode.contains(oldKey)) gridSquaresPerBandAndMode.insert(oldKey);
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
                } else if (category == "dxcc" || category == "eadx100") {
                    qsoDxccMults++;
                } else if (category == "gridSquares") {
                    qsoGridSquareMults++;
                }
            }
        }
        
        // Set the multiplier counts for this QSO
        qso.setMultiplierCount(qsoNamedMults);
        qso.setDxccCount(qsoDxccMults);
        qso.setGridSquareMultiplierCount(qsoGridSquareMults);
    }
    
     // Count multipliers based on type
    if (multType == "objectiveMultipliers") {
        // For objective multipliers (WFD): no multipliers to extract from QSOs
        // Instead, get the count from user-selected OMs
        int omCount = calculateObjectiveMultiplierCount();
        m_runningScore.objectiveMultiplierCount = omCount;
        
        // Build the details map
        QMap<QString, int> omOptions = getObjectiveMultiplierOptions();
        QStringList selected = getSelectedObjectiveMultipliers();
        for (const QString& omCode : selected) {
            if (omOptions.contains(omCode)) {
                m_runningScore.objectiveMultiplierDetails[omCode] = omOptions[omCode];
            }
        }
        
        m_runningScore.multipliers = omCount + 1;  // Always add 1
        m_runningScore.contestScore = (m_runningScore.contactScore * m_runningScore.multipliers) + m_runningScore.bonusPoints;
        
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Objective Multipliers: %1 points (from %2 OMs) = %3 score")
                    .arg(m_runningScore.contactScore)
                    .arg(omCount)
                    .arg(m_runningScore.contestScore));
        }
    } else if (multType == "multsOnce") {
        m_runningScore.namedMultCount = namedMultsOnce.size();
        m_runningScore.dxccMultCount = dxccMultsOnce.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsOnce.size();
        m_runningScore.namedCallPrefixCount = namedCallPrefixesOnce.size();
        m_runningScore.gridSquareMultCount = gridSquaresOnce.size();
        m_runningScore.multipliers = uniqueMultipliers.size();
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Unique mults (once): %1 total (named:%2 dxcc:%3 itu:%4 prefixes:%5 grid:%6)")
                    .arg(uniqueMultipliers.size())
                    .arg(m_runningScore.namedMultCount)
                    .arg(m_runningScore.dxccMultCount)
                    .arg(m_runningScore.ituRegionMultCount)
                    .arg(m_runningScore.namedCallPrefixCount)
                    .arg(m_runningScore.gridSquareMultCount));
        }
    } else if (multType == "multsPerBand") {
        m_runningScore.namedMultCount = namedMultsPerBand.size();
        m_runningScore.dxccMultCount = dxccMultsPerBand.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsPerBand.size();
        m_runningScore.namedCallPrefixCount = namedCallPrefixesPerBand.size();
        m_runningScore.gridSquareMultCount = gridSquaresPerBand.size();
        m_runningScore.multipliers = multPerBand.size();
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Mults per band: %1 total (named:%2 dxcc:%3 itu:%4 prefixes:%5 grid:%6)")
                    .arg(multPerBand.size())
                    .arg(m_runningScore.namedMultCount)
                    .arg(m_runningScore.dxccMultCount)
                    .arg(m_runningScore.ituRegionMultCount)
                    .arg(m_runningScore.namedCallPrefixCount)
                    .arg(m_runningScore.gridSquareMultCount));
        }
    } else if (multType == "multsPerMode") {
        m_runningScore.namedMultCount = namedMultsPerMode.size();
        m_runningScore.dxccMultCount = dxccMultsPerMode.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsPerMode.size();
        m_runningScore.namedCallPrefixCount = namedCallPrefixesPerMode.size();
        m_runningScore.gridSquareMultCount = gridSquaresPerMode.size();
        m_runningScore.multipliers = multPerMode.size();
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Mults per mode: %1 total (named:%2 dxcc:%3 itu:%4 prefixes:%5 grid:%6)")
                    .arg(multPerMode.size())
                    .arg(m_runningScore.namedMultCount)
                    .arg(m_runningScore.dxccMultCount)
                    .arg(m_runningScore.ituRegionMultCount)
                    .arg(m_runningScore.namedCallPrefixCount)
                    .arg(m_runningScore.gridSquareMultCount));
        }
    } else if (multType == "multsPerBandAndMode") {
        m_runningScore.namedMultCount = namedMultsPerBandAndMode.size();
        m_runningScore.dxccMultCount = dxccMultsPerBandAndMode.size();
        m_runningScore.ituRegionMultCount = ituRegionMultsPerBandAndMode.size();
        m_runningScore.namedCallPrefixCount = namedCallPrefixesPerBandAndMode.size();
        m_runningScore.gridSquareMultCount = gridSquaresPerBandAndMode.size();
        m_runningScore.multipliers = multPerBandAndMode.size();
        DebugLogger::instance().log("ContestEngine", 
            QString("Mults per band/mode: %1 total (named:%2 dxcc:%3 itu:%4 prefixes:%5 grid:%6)")
                .arg(multPerBandAndMode.size())
                .arg(m_runningScore.namedMultCount)
                .arg(m_runningScore.dxccMultCount)
                .arg(m_runningScore.ituRegionMultCount)
                .arg(m_runningScore.namedCallPrefixCount)
                .arg(m_runningScore.gridSquareMultCount));
    }
    
    // Set DXCC count (for informational purposes)
    m_runningScore.dxccCount = uniqueDxccEntities.size();
    
    // Calculate final contest score
    // Check if contest uses category-based scoring (states + provinces + dxcc)
    // Calculate bonus points from bonusStation groups
    m_runningScore.bonusPoints = 0;
    for (const BonusStationGroup& group : m_bonusStationGroups) {
        QSet<QString> seen;
        for (const QsoRecord& qso : qsos) {
            QString call = qso.getCall().toUpper();
            if (!group.stations.contains(call))
                continue;

            // Build dedup key based on group type
            QString key;
            if (group.type == "bonusOnce") {
                key = call;
            } else if (group.type == "bonusPerBand") {
                key = call + "_" + qso.getBand();
            } else if (group.type == "bonusPerMode") {
                key = call + "_" + qso.getMode().toUpper();
            } else { // bonusPerBandAndMode
                key = call + "_" + qso.getBand() + "_" + qso.getMode().toUpper();
            }

            if (!seen.contains(key)) {
                seen.insert(key);
                m_runningScore.bonusPoints += group.pointsEach;
            }
        }
    }
    
    QStringList multCategories = getMultiplierCategories();
    
    // Skip score calculation if already handled by objectiveMultipliers
    if (multType == "objectiveMultipliers") {
        // Score already calculated at line 2074
        if (verbose) {
            DebugLogger::instance().log("ContestEngine", 
                QString("Objective Multipliers scoring already complete"));
        }
    } else if (multType == "multsPerMode") {
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
        int totalMultipliers = m_runningScore.namedMultCount + m_runningScore.dxccMultCount + m_runningScore.ituRegionMultCount + m_runningScore.namedCallPrefixCount + m_runningScore.gridSquareMultCount;
        m_runningScore.contestScore = (m_runningScore.contactScore * totalMultipliers) + m_runningScore.bonusPoints;
    } else if (m_runningScore.multipliers > 0) {
        // Traditional scoring: points * mults
        m_runningScore.contestScore = (m_runningScore.contactScore * m_runningScore.multipliers) + m_runningScore.bonusPoints;
    } else {
        // No multipliers defined: score is just the sum of points
        m_runningScore.contestScore = m_runningScore.contactScore + m_runningScore.bonusPoints;
    }
    
    // Apply prompt-driven score multiplier (e.g., power category: QRP ×3, Low ×2, High ×1)
    m_runningScore.scoreMultiplier = 1;
    if (!m_cachedScoreMultiplierPromptId.isEmpty()) {
        QString promptValue = getUserPromptValue(m_cachedScoreMultiplierPromptId);
        if (m_cachedScoreMultiplierValues.contains(promptValue)) {
            m_runningScore.scoreMultiplier = m_cachedScoreMultiplierValues[promptValue];
            if (m_runningScore.scoreMultiplier != 1)
                m_runningScore.contestScore *= m_runningScore.scoreMultiplier;
        }
    }

    DebugLogger::instance().log("ContestEngine",
        QString("Running score updated: %1 QSOs, %2 points, %3 mults (%4 named+%5 dxcc+%6 itu+%7 prefixes) × %8 power = %9 score")
            .arg(validQsoCount)
            .arg(m_runningScore.contactScore)
            .arg(m_runningScore.multipliers)
            .arg(m_runningScore.namedMultCount)
            .arg(m_runningScore.dxccMultCount)
            .arg(m_runningScore.ituRegionMultCount)
            .arg(m_runningScore.namedCallPrefixCount)
            .arg(m_runningScore.scoreMultiplier)
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
    m_workedNamedMults.clear();
    m_workedNamedMultsPerBand.clear();
    m_workedNamedMultsPerMode.clear();
    m_workedNamedMultsPerBandAndMode.clear();
    DebugLogger::instance().log("ContestEngine", "Score reset");
}

void ContestEngine::rescoreAll(QList<QsoRecord>& qsos, const QString& myCallsign)
{
    m_dxccCache.clear();  // Fresh cache for this scoring pass

    // Determine dupe scope once (avoids re-deriving it for every QSO)
    QString dupeScope = getDupeScope();
    if (dupeScope.isEmpty()) {
        QString multType = getMultiplierType();
        if (multType == "multsOnce")          dupeScope = "overall";
        else if (multType == "multsPerBand")  dupeScope = "per_band";
        else if (multType == "multsPerMode")  dupeScope = "per_mode";
        else if (multType == "multsPerBandAndMode") dupeScope = "per_band_mode";
    }

    // Incremental dupe tracking: callUpper -> set of scope keys already worked.
    // Scope key is "" for overall, band for per_band, mode for per_mode, band_mode for per_band_mode.
    QHash<QString, QSet<QString>> workedCalls;

    for (int i = 0; i < qsos.count(); ++i) {
        QsoRecord& qso = qsos[i];

        // Out-of-band check
        if (!isValidBand(qso.getFrequency().toDouble())) {
            qso.setOutOfBand(true);
            qso.setDupe(false);
            qso.setComment("Out of band for contest");
            qso.setPoints(0);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            continue;
        }

        qso.setOutOfBand(false);
        qso.setDupe(false);
        qso.setComment("");

        // O(1) dupe check against the incremental hash
        const QString callUpper = qso.getCall().toUpper();
        QString scopeKey;
        if (dupeScope == "per_band")           scopeKey = qso.getBand();
        else if (dupeScope == "per_mode")      scopeKey = qso.getMode();
        else if (dupeScope == "per_band_mode") scopeKey = qso.getBand() + "_" + qso.getMode();
        // "overall" keeps scopeKey as ""

        const bool isDupe = workedCalls.contains(callUpper)
                            && workedCalls[callUpper].contains(scopeKey);

        if (isDupe) {
            qso.setDupe(true);
            qso.setPoints(0);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            QString reason;
            if (dupeScope == "overall")         reason = "contest";
            else if (dupeScope == "per_band")   reason = "band";
            else if (dupeScope == "per_mode")   reason = "mode";
            else if (dupeScope == "per_band_mode") reason = "band/mode";
            qso.setComment(QString("Duplicate contact for %1").arg(reason));
            continue;
        }

        // Register this QSO in the dupe tracking hash
        workedCalls[callUpper].insert(scopeKey);

        // Points — updateRunningScore will re-derive this, but we set it here so
        // the per-QSO column in the table is correct after replaceAll.
        qso.setPoints(calculatePoints(qso, myCallsign));
    }

    // One O(n) pass: sets m_workedNamedMults*, per-QSO mult counts, and final totals.
    updateRunningScore(qsos, myCallsign, false);
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

void ContestEngine::setUserPromptValue(const QString& promptId, const QString& value)
{
    m_userPromptValues[promptId] = value;
    DebugLogger::instance().log("ContestEngine",
        QString("User prompt '%1' set to: '%2'").arg(promptId, value));

    // Re-cache contest properties since multiplier categories and invalidPartners
    // may depend on userPrompt values (e.g., stationClassMultipliers in SPDX)
    if (!m_contestDef.isEmpty())
        cacheContestProperties();
}

QString ContestEngine::getUserPromptValue(const QString& promptId) const
{
    return m_userPromptValues.value(promptId, QString());
}

QMap<QString, QString> ContestEngine::getUserPromptValues() const
{
    return m_userPromptValues;
}

QMap<QString, int> ContestEngine::getObjectiveMultiplierOptions() const
{
    QMap<QString, int> omOptions;
    
    // Look for objectiveMultipliers prompt in userPrompts array
    if (m_contestDef.contains("userPrompts")) {
        QJsonArray prompts = m_contestDef["userPrompts"].toArray();
        for (const QJsonValue& promptVal : prompts) {
            QJsonObject prompt = promptVal.toObject();
            if (prompt["id"].toString() == "objectiveMultipliers") {
                QJsonArray options = prompt["options"].toArray();
                for (const QJsonValue& optVal : options) {
                    QJsonObject opt = optVal.toObject();
                    QString value = opt["value"].toString();
                    int points = opt["points"].toInt();
                    if (!value.isEmpty()) {
                        omOptions[value] = points;
                    }
                }
                break;
            }
        }
    }
    
    return omOptions;
}

QStringList ContestEngine::getSelectedObjectiveMultipliers() const
{
    QString selectedJson = m_userPromptValues.value("objectiveMultipliers", "[]");
    QStringList selected;
    
    // Parse JSON array from stored value
    QJsonDocument doc = QJsonDocument::fromJson(selectedJson.toUtf8());
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue& val : arr) {
            selected.append(val.toString());
        }
    }
    
    return selected;
}

int ContestEngine::calculateObjectiveMultiplierCount() const
{
    int totalPoints = 0;
    QMap<QString, int> omOptions = getObjectiveMultiplierOptions();
    QStringList selected = getSelectedObjectiveMultipliers();
    
    for (const QString& omCode : selected) {
        if (omOptions.contains(omCode)) {
            totalPoints += omOptions[omCode];
        }
    }
    
    DebugLogger::instance().log("ContestEngine", 
        QString("Objective Multiplier Count: %1 (from %2 selected OMs)")
            .arg(totalPoints).arg(selected.size()));
    
    return totalPoints;
}
