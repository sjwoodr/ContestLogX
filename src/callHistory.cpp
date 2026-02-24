/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "callHistory.h"
#include "debugLogger.h"
#include "settings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

CallHistory& CallHistory::instance()
{
    static CallHistory inst;
    return inst;
}

CallHistory::CallHistory()
    : m_enabled(false), m_autoSaveEnabled(false)
{
    load();
}

QString CallHistory::historyFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dataDir + "/history.json";
}

void CallHistory::load()
{
    QString filePath = historyFilePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        m_records = QJsonArray();
        m_enabled = Settings::instance().getCallHistoryEnabled();
        m_autoSaveEnabled = Settings::instance().getCallHistoryAutoSaveEnabled();
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        DebugLogger::instance().log("CallHistory", 
            QString("Failed to open history file: %1").arg(filePath));
        m_records = QJsonArray();
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        m_records = QJsonArray();
        DebugLogger::instance().log("CallHistory", "History file is not a valid JSON array");
        return;
    }
    
    m_records = doc.array();
    m_enabled = Settings::instance().getCallHistoryEnabled();
    m_autoSaveEnabled = Settings::instance().getCallHistoryAutoSaveEnabled();
    
    DebugLogger::instance().log("CallHistory", 
        QString("Loaded %1 call history records").arg(m_records.size()));
}

void CallHistory::save()
{
    QString filePath = historyFilePath();
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        DebugLogger::instance().log("CallHistory", 
            QString("Failed to open history file for writing: %1").arg(filePath));
        return;
    }
    
    QJsonDocument doc(m_records);
    file.write(doc.toJson());
    file.close();
    
    DebugLogger::instance().log("CallHistory", 
        QString("Saved %1 call history records").arg(m_records.size()));
}

void CallHistory::clear()
{
    m_records = QJsonArray();
    QString filePath = historyFilePath();
    QFile file(filePath);
    file.remove();
    
    DebugLogger::instance().log("CallHistory", "Call history cleared");
}

QMap<QString, QString> CallHistory::getRecord(const QString& callsign) const
{
    QMap<QString, QString> result;
    
    QString upperCall = callsign.toUpper();
    for (const QJsonValue& val : m_records) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();
        if (obj["CALL"].toString().toUpper() == upperCall) {
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                result[it.key()] = it.value().toString();
            }
            return result;
        }
    }
    
    return result;
}

QSet<QString> CallHistory::getAllCallsigns() const
{
    QSet<QString> callsigns;
    for (const QJsonValue& val : m_records) {
        if (val.isObject()) {
            QString call = val.toObject()["CALL"].toString();
            if (!call.isEmpty()) {
                callsigns.insert(call);
            }
        }
    }
    return callsigns;
}

void CallHistory::addOrUpdateRecord(const QString& callsign, const QMap<QString, QString>& fields)
{
    if (callsign.isEmpty()) return;
    
    QString upperCall = callsign.toUpper();
    
    // Find existing record
    int existingIndex = -1;
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].isObject() && 
            m_records[i].toObject()["CALL"].toString().toUpper() == upperCall) {
            existingIndex = i;
            break;
        }
    }
    
    QJsonObject recordObj;
    recordObj["CALL"] = upperCall;
    
    // If updating, preserve existing fields not in the new fields map
    if (existingIndex != -1) {
        QJsonObject existing = m_records[existingIndex].toObject();
        for (auto it = existing.begin(); it != existing.end(); ++it) {
            if (it.key() != "CALL") {
                recordObj[it.key()] = it.value();
            }
        }
    }
    
    // Merge in new fields
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        recordObj[it.key()] = it.value();
    }
    
    if (existingIndex != -1) {
        m_records[existingIndex] = recordObj;
    } else {
        m_records.append(recordObj);
    }
    
    DebugLogger::instance().log("CallHistory", 
        QString("Added/updated record for %1").arg(callsign));
}

void CallHistory::deleteRecord(const QString& callsign)
{
    QString upperCall = callsign.toUpper();
    
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].isObject() && 
            m_records[i].toObject()["CALL"].toString().toUpper() == upperCall) {
            m_records.removeAt(i);
            DebugLogger::instance().log("CallHistory", 
                QString("Deleted record for %1").arg(callsign));
            return;
        }
    }
}

QJsonArray CallHistory::getAllRecords() const
{
    return m_records;
}

QStringList CallHistory::getAllFieldNames() const
{
    QSet<QString> fieldSet;
    fieldSet.insert("CALL");  // CALL is always first
    
    for (const QJsonValue& val : m_records) {
        if (val.isObject()) {
            QJsonObject obj = val.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it.key() != "CALL") {
                    fieldSet.insert(it.key());
                }
            }
        }
    }
    
    QStringList fields = QStringList(fieldSet.begin(), fieldSet.end());
    // Ensure CALL is first
    fields.removeOne("CALL");
    fields.sort();
    fields.prepend("CALL");
    
    return fields;
}

bool CallHistory::isEnabled() const
{
    return m_enabled;
}

void CallHistory::setEnabled(bool enabled)
{
    m_enabled = enabled;
    Settings::instance().setCallHistoryEnabled(enabled);
}

bool CallHistory::isAutoSaveEnabled() const
{
    return m_autoSaveEnabled;
}

void CallHistory::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;
    Settings::instance().setCallHistoryAutoSaveEnabled(enabled);
}
