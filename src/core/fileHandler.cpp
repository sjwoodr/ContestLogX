/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "fileHandler.h"
#include "clxFile.h"
#include "adifFile.h"
#include "cabrilloFile.h"
#include "csvFile.h"
#include "stationInfo.h"
#include "debugLogger.h"
#include <QFileInfo>

FileHandler::FileHandler()
{
}

// ---------------------------------------------------------------------------
// Router
// ---------------------------------------------------------------------------

bool FileHandler::load(const QString& filename, QList<QsoRecord>& qsos)
{
    QFileInfo fileInfo(filename);
    QString ext = fileInfo.suffix().toLower();

    if (ext == "clx") {
        return loadClx(filename, qsos);
    } else if (ext == "csv") {
        CsvFile csv;
        bool ok = csv.load(filename, qsos);
        if (!ok) m_lastError = csv.lastError();
        return ok;
    } else if (ext == "adi" || ext == "adif") {
        AdifFile adif;
        bool ok = adif.load(filename, qsos);
        if (!ok) m_lastError = adif.lastError();
        return ok;
    } else if (ext == "wl") {
        m_lastError = "Legacy .wl binary format is not supported. "
                      "Try exporting as ADIF from Windows ContestLogX.";
        return false;
    } else if (ext == "log" || ext == "cbr" || ext == "cab") {
        CabrilloFile cabrillo;
        bool ok = cabrillo.load(filename, qsos, m_contestDefinition);
        if (!ok) m_lastError = cabrillo.lastError();
        return ok;
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
        CsvFile csv;
        bool ok = csv.save(filename, qsos);
        if (!ok) m_lastError = csv.lastError();
        return ok;
    } else if (ext == "adi" || ext == "adif") {
        AdifFile adif;
        adif.setStationCallsign(m_stationCallsign);
        bool ok = adif.save(filename, qsos);
        if (!ok) m_lastError = adif.lastError();
        return ok;
    } else if (ext == "wl") {
        m_lastError = "Legacy .wl binary format is not supported for saving. "
                      "Use .clx format instead.";
        return false;
    } else {
        // Default to CLX
        return saveClx(filename, qsos);
    }
}

// ---------------------------------------------------------------------------
// CLX format — delegates to ClxFile
// ---------------------------------------------------------------------------

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

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                                     QString& contestFile, QString& stationClass,
                                     QString& contestVersion)
{
    QString dummy;
    return loadClxWithContest(filename, qsos, contestFile, stationClass, contestVersion, dummy);
}

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                                     QString& contestFile, QString& stationClass,
                                     QString& contestVersion, QString& stationClassExchange)
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

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                                     QString& contestFile, QString& stationClass,
                                     QString& contestVersion,
                                     QString& stationClassExchangeName,
                                     QString& stationClassExchangeId)
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
    stationClassExchangeName = clxFile.contest().category("station_class_exchange_name");
    stationClassExchangeId   = clxFile.contest().category("station_class_exchange_id");

    // Fallback: split combined field if separate fields not found
    if (stationClassExchangeName.isEmpty() && stationClassExchangeId.isEmpty()) {
        QString combined = clxFile.contest().category("station_class_exchange");
        if (!combined.isEmpty()) {
            int spacePos = combined.indexOf(' ');
            if (spacePos > 0) {
                stationClassExchangeName = combined.left(spacePos).toUpper();
                stationClassExchangeId   = combined.mid(spacePos + 1).toUpper();
            } else {
                stationClassExchangeName = combined.toUpper();
            }
        }
    }
    return true;
}

bool FileHandler::loadClxWithContest(const QString& filename, QList<QsoRecord>& qsos,
                                     QString& contestFile, QString& stationClass,
                                     QString& contestVersion,
                                     QString& stationClassExchangeName,
                                     QString& stationClassExchangeId,
                                     QMap<QString, QString>& userPromptValues)
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
    stationClassExchangeName = clxFile.contest().category("station_class_exchange_name");
    stationClassExchangeId   = clxFile.contest().category("station_class_exchange_id");

    // Fallback: split combined field if separate fields not found
    if (stationClassExchangeName.isEmpty() && stationClassExchangeId.isEmpty()) {
        QString combined = clxFile.contest().category("station_class_exchange");
        if (!combined.isEmpty()) {
            int spacePos = combined.indexOf(' ');
            if (spacePos > 0) {
                stationClassExchangeName = combined.left(spacePos).toUpper();
                stationClassExchangeId   = combined.mid(spacePos + 1).toUpper();
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

bool FileHandler::saveClx(const QString& filename, const QList<QsoRecord>& qsos)
{
    ClxFile clxFile;
    for (const QsoRecord& qso : qsos)
        clxFile.addQso(qso);
    if (!clxFile.save(filename)) {
        m_lastError = clxFile.lastError();
        return false;
    }
    return true;
}

// Helper: populate ClxFile contest metadata from contestDef JSON
static void applyContestMeta(ClxFile& clxFile, const QJsonObject& contestDef,
                              const QString& contestFile,
                              const QString& stationClass,
                              const QString& stationClassExchangeName,
                              const QString& stationClassExchangeId,
                              const QMap<QString, QString>& userPromptValues)
{
    if (contestDef.isEmpty())
        return;

    QString contestName = "General DXCC Logging";

    if (contestDef.contains("contest")) {
        QJsonObject contestObj = contestDef["contest"].toObject();
        if (contestObj.contains("name"))
            contestName = contestObj["name"].toString();
        if (contestObj.contains("version"))
            clxFile.contest().setContestVersion(contestObj["version"].toString());
    }

    if (contestName == "General DXCC Logging" && contestDef.contains("name")) {
        QString topLevelName = contestDef["name"].toString();
        if (!topLevelName.isEmpty())
            contestName = topLevelName;
    }

    clxFile.contest().setName(contestName);
    clxFile.contest().setType(contestName);
    clxFile.contest().setContestFile(contestFile);

    if (!stationClass.isEmpty())
        clxFile.contest().setCategory("station_class", stationClass);
    if (!stationClassExchangeName.isEmpty())
        clxFile.contest().setCategory("station_class_exchange_name", stationClassExchangeName);
    if (!stationClassExchangeId.isEmpty())
        clxFile.contest().setCategory("station_class_exchange_id", stationClassExchangeId);

    for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it) {
        if (!it.value().isEmpty())
            clxFile.contest().setCategory("userPrompt_" + it.key(), it.value());
    }

    // Persist the contest mode for any userPrompt with "restrictMode": true.
    // This populates ContestInfo::mode() so the load path can restore setRestrictedMode()
    // independently of the userPromptValues mechanism.
    if (contestDef.contains("userPrompts")) {
        QJsonArray prompts = contestDef["userPrompts"].toArray();
        for (const QJsonValue& pv : prompts) {
            QJsonObject p = pv.toObject();
            if (p["restrictMode"].toBool(false)) {
                QString id = p["id"].toString();
                QString modeValue = userPromptValues.value(id);
                if (!modeValue.isEmpty()) {
                    clxFile.contest().setMode(modeValue);
                    break;
                }
            }
        }
    }
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                                     const QString& contestFile, const QJsonObject& contestDef,
                                     const QString& stationClass,
                                     const QString& stationClassExchange)
{
    ClxFile clxFile;
    applyContestMeta(clxFile, contestDef, contestFile, stationClass, QString(), QString(), {});
    if (!stationClassExchange.isEmpty())
        clxFile.contest().setCategory("station_class_exchange", stationClassExchange);
    for (const QsoRecord& qso : qsos) clxFile.addQso(qso);
    if (!clxFile.save(filename)) { m_lastError = clxFile.lastError(); return false; }
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                                     const QString& contestFile, const QJsonObject& contestDef,
                                     const QString& stationClass,
                                     const QString& stationClassExchangeName,
                                     const QString& stationClassExchangeId)
{
    ClxFile clxFile;
    applyContestMeta(clxFile, contestDef, contestFile, stationClass,
                     stationClassExchangeName, stationClassExchangeId, {});
    for (const QsoRecord& qso : qsos) clxFile.addQso(qso);
    if (!clxFile.save(filename)) { m_lastError = clxFile.lastError(); return false; }
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                                     const QString& contestFile, const QJsonObject& contestDef,
                                     const QString& stationClass,
                                     const QString& stationClassExchangeName,
                                     const QString& stationClassExchangeId,
                                     const QMap<QString, QString>& userPromptValues)
{
    ClxFile clxFile;
    applyContestMeta(clxFile, contestDef, contestFile, stationClass,
                     stationClassExchangeName, stationClassExchangeId, userPromptValues);
    for (const QsoRecord& qso : qsos) clxFile.addQso(qso);
    if (!clxFile.save(filename)) { m_lastError = clxFile.lastError(); return false; }
    return true;
}

bool FileHandler::saveClxWithContest(const QString& filename, const QList<QsoRecord>& qsos,
                                     const QString& contestFile, const QJsonObject& contestDef,
                                     const QString& stationClass,
                                     const QString& stationClassExchangeName,
                                     const QString& stationClassExchangeId,
                                     const QMap<QString, QString>& userPromptValues,
                                     const StationInfo& stationInfo)
{
    ClxFile clxFile;
    clxFile.station() = stationInfo;
    applyContestMeta(clxFile, contestDef, contestFile, stationClass,
                     stationClassExchangeName, stationClassExchangeId, userPromptValues);

    if (m_useContestMemories) {
        clxFile.setUseContestMemories(true);
        clxFile.setCwMemories(m_contestCwMemories);
        clxFile.setSsbMemories(m_contestSsbMemories);
    }

    for (const QsoRecord& qso : qsos) clxFile.addQso(qso);
    if (m_scoreSet) clxFile.setComputedScore(m_contactScore, m_finalScore);
    if (!clxFile.save(filename)) { m_lastError = clxFile.lastError(); return false; }
    return true;
}
