/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
