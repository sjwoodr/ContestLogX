/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CALLHISTORY_H
#define CALLHISTORY_H

#include <QString>
#include <QMap>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>

/**
 * @brief Call history manager for contest logging
 * 
 * Manages a persistent JSON file of worked callsigns and their exchange info
 */
class CallHistory
{
public:
    static CallHistory& instance();
    
    // Load/Save operations
    void load();
    void save();
    void clear();
    
    // Query operations
    QMap<QString, QString> getRecord(const QString& callsign) const;
    QSet<QString> getAllCallsigns() const;
    
    // Update operations
    void addOrUpdateRecord(const QString& callsign, const QMap<QString, QString>& fields);
    void deleteRecord(const QString& callsign);
    
    // Get all records
    QJsonArray getAllRecords() const;
    
    // Get all unique field names across all records (CALL is always first)
    QStringList getAllFieldNames() const;
    
    // Check if history is enabled
    bool isEnabled() const;
    void setEnabled(bool enabled);
    
    bool isAutoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);
    
private:
    CallHistory();
    
    QString historyFilePath() const;
    QStringList extractFieldNamesFromRecords() const;
    
    QJsonArray m_records;
    bool m_enabled;
    bool m_autoSaveEnabled;
};

#endif // CALLHISTORY_H
