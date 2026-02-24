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

#ifndef SUPERCHECKPARTIAL_H
#define SUPERCHECKPARTIAL_H

#include <QString>
#include <QStringList>
#include <QSet>

class SuperCheckPartial
{
public:
    static SuperCheckPartial& instance();
    
    // Load SCP database from file
    bool loadDatabase(const QString& filePath);
    
    // Search for callsigns matching the given prefix
    QStringList search(const QString& prefix, int maxResults = 20) const;
    
    // Get the file path to the SCP database
    QString getDataFilePath() const;
    
    // Check if database is loaded
    bool isLoaded() const { return !m_callsigns.isEmpty(); }
    
    // Get database size
    int getDatabaseSize() const { return m_callsigns.size(); }
    
    // Download latest master.scp file from supercheckpartial.com
    static bool downloadLatestDatabase(const QString& targetPath, QString& errorMessage);

private:
    SuperCheckPartial();
    ~SuperCheckPartial() = default;
    
    // Prevent copying
    SuperCheckPartial(const SuperCheckPartial&) = delete;
    SuperCheckPartial& operator=(const SuperCheckPartial&) = delete;
    
    QSet<QString> m_callsigns;
};

#endif // SUPERCHECKPARTIAL_H
