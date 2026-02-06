/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef CONTESTENGINE_H
#define CONTESTENGINE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QList>
#include <QSet>
#include "qsorecord.h"
#include "DxccDatabase.h"

class ContestEngine : public QObject
{
    Q_OBJECT

public:
    explicit ContestEngine(QObject *parent = nullptr);
    ~ContestEngine();
    
    // Load contest definition
    bool loadContest(const QJsonObject& contestDef);
    QJsonObject getContestDefinition() const { return m_contestDef; }
    QString getContestName() const;
    
    // Field information
    QStringList getExchangeFields() const;
    QStringList getLogColumns() const;
    QString getFieldLabel(const QString& fieldName) const;
    QString getFieldType(const QString& fieldName) const;
    int getFieldMaxLength(const QString& fieldName) const;
    
    // Validation
    bool validateExchange(const QString& fieldName, const QString& value, QString& errorMsg) const;
    bool validateQso(const QsoRecord& qso, QString& errorMsg) const;
    
    // Duplicate checking
    bool isDupe(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const;
    QString getDupeReason(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const;
    QString getDupeScope() const; // "overall", "per_band", "per_mode", "per_band_mode"
    
    // Scoring
    int calculatePoints(const QsoRecord& qso, const QString& myCallsign) const;
    struct MultiplierInfo {
        QString value;      // The multiplier value (e.g., "OH", "ON", "DL")
        QString category;   // The category (e.g., "states", "provinces", "dxcc")
        
        bool operator==(const MultiplierInfo& other) const {
            return value == other.value && category == other.category;
        }
    };
    
    struct QsoMultiplierCredit {
        int namedMultCount = 0;  // Count of named multipliers (states/provinces) credited for this QSO
        int dxccMultCount = 0;   // Count of DXCC entities credited for this QSO
        int ituRegionMultCount = 0;  // Count of ITU regions credited for this QSO
        int gridSquareMultCount = 0;  // Count of grid squares credited for this QSO
    };
    
    QStringList getMultipliers(const QsoRecord& qso) const;
    QList<MultiplierInfo> getMultipliersWithCategory(const QsoRecord& qso) const;
    QsoMultiplierCredit getQsoMultiplierCredit(const QsoRecord& qso, const QList<QsoRecord>& existingQsos) const;
    bool isNewMultiplier(const QString& mult, const QString& band, const QString& mode, const QList<QsoRecord>& existingQsos) const;
    int calculateTotalScore(const QList<QsoRecord>& qsos, int& totalQsos, int& totalMults) const;
    QString getMultiplierType() const;
    QStringList getMultiplierCategories() const;
    bool getAlaskaHawaiiCountDxcc() const;
    bool getUsAndCanadaCountDxcc() const;

    // Worked multiplier tracking (for multiplier widget display)
    QSet<QString> getWorkedNamedMults() const { return m_workedNamedMults; }
    QSet<QString> getWorkedNamedMultsPerBand() const { return m_workedNamedMultsPerBand; }
    QSet<QString> getWorkedNamedMultsPerMode() const { return m_workedNamedMultsPerMode; }
    QSet<QString> getWorkedNamedMultsPerBandAndMode() const { return m_workedNamedMultsPerBandAndMode; }
    QStringList getNamedMultiplierList() const;
    QSet<QString> getEffectiveValidMults() const;
    QStringList getEffectiveNamedMultiplierList() const;

    // Running score tracking
    struct BandModeStats {
        int cwQsos = 0;
        int ssbQsos = 0;
        int digitalQsos = 0;
        int points = 0;
    };
    
    struct ContestScore {
        QMap<QString, BandModeStats> bandStats;  // Key: band name (e.g., "20m")
        int contactScore = 0;
        int multipliers = 0;
        int namedMultCount = 0;   // Named multiplier count (for category scoring)
        int dxccMultCount = 0;    // DXCC multipliers (for category scoring)
        int ituRegionMultCount = 0;   // ITU Region multipliers (for category scoring)
        int namedCallPrefixCount = 0;  // Call prefix multipliers (for category scoring)
        int gridSquareMultCount = 0;  // Grid square multipliers (for category scoring)
        int objectiveMultiplierCount = 0;  // Objective Multiplier count for WFD-style contests
        int dxccCount = 0;        // Total unique DXCC entities worked (for info)
        int bonusPoints = 0;
        int contestScore = 0;
        QMap<QString, int> objectiveMultiplierDetails;  // Maps OM code to point value (e.g., "ALT_POWER" -> 1)
    };
    
    void updateRunningScore(QList<QsoRecord>& qsos, const QString& myCallsign, bool verbose = false);
    ContestScore getRunningScore() const { return m_runningScore; }
    QMap<QString, BandModeStats> getBandBreakdown() const { return m_runningScore.bandStats; }
    void resetScore();
    
    // DXCC
    void setDxccDatabase(DxccDatabase* dxcc) { m_dxccDatabase = dxcc; }
    DxccDatabase* dxccDatabase() const { return m_dxccDatabase; }
    
    // Band/Mode validation
    bool isValidBand(double freqKhz) const;
    bool isValidMode(const QString& mode) const;
    QStringList getAllowedModes() const;
    QString getBandFromFrequency(double freqKhz) const;
    
    // Station class support
    bool needsStationClass() const;
    QString getStationClassPrompt() const;
    QStringList getStationClassOptions() const;
    void setStationClass(const QString& classId);
    QString getStationClass() const { return m_stationClass; }
    QString getDefaultSentExchange(const QString& stationQth, int serialNumber) const;
    // Separate storage for name and exchange (replaces setStationClassExchangeData)
    void setStationClassExchangeName(const QString& name) { 
        m_stationClassExchangeName = name.toUpper(); 
    }
    QString getStationClassExchangeName() const { return m_stationClassExchangeName; }
    void setStationClassExchangeId(const QString& id) { 
        m_stationClassExchangeId = id.toUpper(); 
    }
    QString getStationClassExchangeId() const { return m_stationClassExchangeId; }
    // Legacy combined method for backwards compatibility
    void setStationClassExchangeData(const QString& data);
    QString getStationClassExchangeData() const;
    QString getSentExchangeName() const;
    QString getSentExchangeId() const;
    bool stationClassNeedsInput() const;
    QString getStationClassInputPrompt() const;
    QString getStationClassNamePrompt() const;
    QString getStationClassIdPrompt() const;
    QJsonObject getStationClassInputValidation() const;  // Get input validation rules
    void resetStationClassState();  // Reset station class and exchange data
    QString getStationClassMode() const;  // Get mode from selected station class
    void setRestrictedMode(const QString& mode);  // Restrict logging to a specific mode
    QString getRestrictedMode() const;  // Get the restricted mode if any
    
    // Call history field configuration
    QStringList getCallHistoryFieldsToSave() const;
    
    // User prompt handling
    void setUserPromptValue(const QString& promptId, const QString& value);
    QString getUserPromptValue(const QString& promptId) const;
    QMap<QString, QString> getUserPromptValues() const;
    
    // Objective Multiplier handling (for WFD and similar contests)
    QMap<QString, int> getObjectiveMultiplierOptions() const;  // Get all OM options with point values
    QStringList getSelectedObjectiveMultipliers() const;  // Get list of selected OM codes
    int calculateObjectiveMultiplierCount() const;  // Sum point values of selected OMs

private:
    bool validateSerialNumber(const QString& value) const;
    bool validateState(const QString& value) const;
    bool validateProvince(const QString& value) const;
    bool validateGridSquare(const QString& value) const;
    bool validateRSTReport(const QString& value) const;
    bool isFieldRequired(const QString& fieldName) const;
    
    QString extractMultiplier(const QsoRecord& qso) const;
    int getPointsForMode(const QString& mode) const;
    
    QJsonObject m_contestDef;
    QSet<QString> m_validStates;
    QSet<QString> m_validProvinces;
    QSet<QString> m_validMultipliers;
    QSet<QString> m_validCallPrefixes;  // Call prefixes like YB0, YB1, 7A, 8B, etc.
    QSet<QString> m_inStateMults;  // Subset of namedMults for in-state exchanges (e.g., MN counties)
    QString m_stationClass;
    QString m_stationClassExchangeName;
    QString m_stationClassExchangeId;
    QString m_restrictedMode;  // Restrict logging to a specific mode when loading from file
    QMap<QString, QString> m_userPromptValues;  // Store user prompt responses (e.g., grid square)
    DxccDatabase* m_dxccDatabase;
    ContestScore m_runningScore;

    // Worked named multiplier sets (persisted across updateRunningScore for widget display)
    QSet<QString> m_workedNamedMults;              // multsOnce: {"OH", "HEN", ...}
    QSet<QString> m_workedNamedMultsPerBand;        // multsPerBand: {"OH_20m", "HEN_40m", ...}
    QSet<QString> m_workedNamedMultsPerMode;        // multsPerMode: {"OH_CW", ...}
    QSet<QString> m_workedNamedMultsPerBandAndMode; // multsPerBandAndMode: {"OH_20m_CW", ...}
};

#endif // CONTESTENGINE_H
