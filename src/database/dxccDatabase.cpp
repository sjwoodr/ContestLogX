/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "dxccDatabase.h"
#include "debugLogger.h"
#include "settings.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

DxccDatabase::DxccDatabase(QObject *parent)
    : QObject(parent)
    , m_loaded(false)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
{
}

DxccDatabase::~DxccDatabase()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

QString DxccDatabase::getDataPath() const
{
    // Use user data path for downloaded cty.dat file
    return Settings::getUserDataPath();
}

bool DxccDatabase::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        DebugLogger::instance().log("DxccDatabase", QString("Failed to open file: %1").arg(filename));
        return false;
    }

    m_entities.clear();
    m_prefixMap.clear();

    QTextStream in(&file);
    int nextDxccNumber = 1;

    while (!in.atEnd()) {
        QString line = in.readLine();
        
        if (line.isEmpty()) {
            continue;
        }

        // Check if this is a country definition line (has multiple colons)
        // cty.dat format: Country:CQ:ITU:Cont:Lat:Long:TZ:PFX:
        if (line.count(':') >= 8) {
            // Parse using : as delimiter
            QStringList fields = line.split(':', Qt::SkipEmptyParts);
            if (fields.size() < 8) {
                continue;
            }

            DxccEntity entity;
            
            // Field 0: Country name
            entity.country = fields[0].trimmed();
            
            // Field 1: CQ Zone
            entity.cqZone = fields[1].trimmed().toInt();
            
            // Field 2: ITU Zone
            entity.ituZone = fields[2].trimmed().toInt();
            
            // Field 3: Continent
            entity.continent = fields[3].trimmed();
            
            // Field 4: Latitude
            entity.latitude = fields[4].trimmed().toDouble();
            
            // Field 5: Longitude
            entity.longitude = fields[5].trimmed().toDouble();
            
            // Field 6: GMT Offset
            entity.gmtOffset = fields[6].trimmed().toDouble();
            
            // Field 7: Primary prefix
            QString primaryPrefix = fields[7].trimmed();
            
            // Handle * prefix (WAEDC only - counts in CQ/DARC contests but NOT ARRL-sponsored contests)
            if (primaryPrefix.startsWith('*')) {
                primaryPrefix = primaryPrefix.mid(1);
                entity.waeOnly = true;
            }

            entity.dxcc = nextDxccNumber++;
            entity.primaryPrefix = primaryPrefix;

            // Read alias prefixes on following lines until semicolon
            QString aliasText;
            while (!in.atEnd()) {
                QString aliasLine = in.readLine();
                aliasText += aliasLine.trimmed();
                if (aliasLine.contains(';')) {
                    break;
                }
                if (!aliasLine.trimmed().isEmpty() && !aliasText.endsWith(',')) {
                    aliasText += ",";
                }
            }

            // Parse aliases (remove semicolon and split by comma)
            aliasText.remove(';');
            QStringList aliasList = aliasText.split(',', Qt::SkipEmptyParts);
            
            for (const QString &alias : aliasList) {
                QString cleanAlias = alias.trimmed();
                if (cleanAlias.isEmpty()) continue;
                
                // Handle special prefix markers
                bool isExactMatch = cleanAlias.startsWith('=');
                if (isExactMatch) {
                    cleanAlias = cleanAlias.mid(1);
                }
                
                // Remove override markers to get base prefix
                // (#) - CQ Zone override
                // [#] - ITU Zone override  
                // <#/#> - Lat/Long override
                // {aa} - Continent override
                // ~#~ - GMT offset override
                QString basePrefix = cleanAlias;
                basePrefix = basePrefix.remove(QRegularExpression("\\([^)]*\\)"));
                basePrefix = basePrefix.remove(QRegularExpression("\\[[^\\]]*\\]"));
                basePrefix = basePrefix.remove(QRegularExpression("<[^>]*>"));
                basePrefix = basePrefix.remove(QRegularExpression("\\{[^}]*\\}"));
                basePrefix = basePrefix.remove(QRegularExpression("~[^~]*~"));
                basePrefix = basePrefix.trimmed();
                
                if (!basePrefix.isEmpty()) {
                    DxccPrefix prefix;
                    prefix.prefix = basePrefix;
                    prefix.dxcc = entity.dxcc;
                    prefix.country = entity.country;
                    prefix.continent = entity.continent;
                    prefix.cqZone = entity.cqZone;
                    prefix.ituZone = entity.ituZone;
                    prefix.latitude = entity.latitude;
                    prefix.longitude = entity.longitude;
                    prefix.gmtOffset = entity.gmtOffset;
                    prefix.exactMatch = isExactMatch;
                    
                    entity.prefixes.append(prefix);
                    m_prefixMap[basePrefix.toUpper()] = prefix;
                }
            }

            // Store entity
            if (!entity.country.isEmpty()) {
                m_entities[entity.dxcc] = entity;
            }
        }
    }

    file.close();
    m_loaded = !m_entities.isEmpty();
    
    DebugLogger::instance().log("DxccDatabase", 
                     QString("Loaded %1 entities with %2 prefixes")
                     .arg(m_entities.size())
                     .arg(m_prefixMap.size()));
    
    return m_loaded;
}

bool DxccDatabase::downloadLatest()
{
    if (m_currentReply) {
        DebugLogger::instance().log("DxccDatabase", "Download already in progress");
        return false;
    }

    // Download from AD1C's CTY.DAT
    QUrl url("https://www.country-files.com/cty/cty.dat");
    
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "ContestLogX/1.0");
    
    m_currentReply = m_networkManager->get(request);
    
    connect(m_currentReply, &QNetworkReply::finished,
            this, &DxccDatabase::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &DxccDatabase::onDownloadProgress);
    
    DebugLogger::instance().log("DxccDatabase", "Starting download from " + url.toString());
    return true;
}

void DxccDatabase::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void DxccDatabase::onDownloadFinished()
{
    if (!m_currentReply) {
        return;
    }

    bool success = false;
    QString error;

    if (m_currentReply->error() == QNetworkReply::NoError) {
        // Save to file
        QString dataPath = getDataPath();
        QString filename = QDir(dataPath).filePath("cty.dat");
        
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentReply->readAll());
            file.close();
            
            // Load the downloaded file
            if (loadFromFile(filename)) {
                success = true;
                DebugLogger::instance().log("DxccDatabase", "Successfully downloaded and loaded cty.dat");
            } else {
                error = "Failed to parse downloaded file";
            }
        } else {
            error = "Failed to save downloaded file: " + file.errorString();
        }
    } else {
        error = m_currentReply->errorString();
        DebugLogger::instance().log("DxccDatabase", "Download failed: " + error);
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    emit downloadFinished(success, error);
}

QString DxccDatabase::stripPortableSuffixes(const QString &callsign) const
{
    QString call = callsign.toUpper();
    
    // Handle portable operation: if there's a slash, check both sides
    // For example: KL7XX/W4 means use W4 (not KL7XX)
    // But YB1AR/2 means use YB (base call has known prefix, suffix is region)
    
    if (call.contains("/")) {
        int slashPos = call.indexOf("/");
        QString beforeSlash = call.left(slashPos);
        QString afterSlash = call.mid(slashPos + 1);
        QString upperAfterSlash = afterSlash.toUpper();

        // Step 1: license-class / non-location-changing portable suffixes —
        // strip and use the base call's DXCC (operator stays in home country).
        // /A and /E are formal license-class indicators per the CQ WPX rule.
        // /MM (maritime mobile) and /AM (aeronautical mobile) do not change
        // the licensing country either. /AE, /AG, /T, /N are US class markers.
        // /QRP is a power-class marker.
        static const QStringList licenseSuffixes = {
            "M", "MM", "AM", "P", "A", "E", "J",
            "AE", "AG", "T", "N", "QRP"
        };
        if (licenseSuffixes.contains(upperAfterSlash)) {
            DebugLogger::instance().log("DxccDatabase",
                QString("Stripped license/portable suffix: %1 -> %2").arg(call, beforeSlash));
            return beforeSlash;
        }

        // Step 2: digit-only suffix is a call-area-change indicator (e.g.,
        // K1AAA/4, YB1AR/2). The operator stays in the same DXCC entity, so
        // use the base call for the DXCC lookup.
        static const QRegularExpression digitsOnly("^[0-9]+$");
        if (digitsOnly.match(upperAfterSlash).hasMatch()) {
            DebugLogger::instance().log("DxccDatabase",
                QString("Digit suffix /%1 — using base call %2 for DXCC").arg(afterSlash, beforeSlash));
            return beforeSlash;
        }

        // Step 3: if the part after the slash IS a known DXCC prefix, the
        // operator has moved to that location and we MUST use it for DXCC
        // lookup (e.g., W1AAA/KH6 → Hawaii, not USA; W1AW/HB → Switzerland,
        // not USA). This must be checked BEFORE the base-call test below,
        // otherwise common base prefixes (W, K, etc.) would short-circuit
        // and we'd report the home country instead of the operating location.
        if (m_prefixMap.contains(upperAfterSlash)) {
            DebugLogger::instance().log("DxccDatabase",
                QString("Portable to known location: %1 -> %2").arg(call, afterSlash));
            return upperAfterSlash;
        }

        // Step 4: partial-prefix match for slightly unusual designators
        // (e.g., /VP2 → VP2E or VP2M, the shortest match wins).
        QString bestMatch;
        for (const QString& prefix : m_prefixMap.keys()) {
            if (prefix.startsWith(upperAfterSlash) &&
                (bestMatch.isEmpty() || prefix.length() < bestMatch.length())) {
                bestMatch = prefix;
            }
        }
        if (!bestMatch.isEmpty()) {
            DebugLogger::instance().log("DxccDatabase",
                QString("Partial-prefix portable match: %1 -> %2 (matched %3)").arg(call, afterSlash, bestMatch));
            return bestMatch;
        }

        // Step 5: afterSlash is unrecognised. If the base call begins with a
        // known DXCC prefix, fall back to that (covers oddball designators
        // like a region number embedded in a slash that didn't match step 2).
        for (const QString& prefix : m_prefixMap.keys()) {
            if (beforeSlash.startsWith(prefix)) {
                DebugLogger::instance().log("DxccDatabase",
                    QString("Unknown designator %1; base %2 matches prefix %3").arg(afterSlash, beforeSlash, prefix));
                return beforeSlash;
            }
        }

        // Step 6: nothing matched. Fall back to the base call as a last resort.
        DebugLogger::instance().log("DxccDatabase",
            QString("Unrecognised slash form %1, using base %2").arg(call, beforeSlash));
        return beforeSlash;
    }

    return call;
}

QString DxccDatabase::findBestMatch(const QString &callsign) const
{
    // First strip portable suffixes
    QString call = stripPortableSuffixes(callsign);
    
    // First, check for exact matches (prefixes that start with =)
    if (m_prefixMap.contains(call)) {
        const DxccPrefix &prefix = m_prefixMap[call];
        if (prefix.exactMatch) {
            return call;
        }
    }
    
    // Try progressively shorter prefixes (longest match wins)
    for (int len = call.length(); len > 0; len--) {
        QString prefix = call.left(len);
        if (m_prefixMap.contains(prefix)) {
            const DxccPrefix &p = m_prefixMap[prefix];
            // Skip exact match prefixes during prefix matching
            if (!p.exactMatch) {
                return prefix;
            }
        }
    }
    
    return QString();
}

DxccEntity DxccDatabase::lookupCallsign(const QString &callsign) const
{
    QString bestMatch = findBestMatch(callsign);
    
    if (!bestMatch.isEmpty() && m_prefixMap.contains(bestMatch)) {
        const DxccPrefix &prefix = m_prefixMap[bestMatch];
        if (m_entities.contains(prefix.dxcc)) {
            return m_entities[prefix.dxcc];
        }
    }
    
    return DxccEntity();
}

QString DxccDatabase::getCountry(const QString &callsign) const
{
    DxccEntity entity = lookupCallsign(callsign);
    return entity.country;
}

QString DxccDatabase::getContinent(const QString &callsign) const
{
    DxccEntity entity = lookupCallsign(callsign);
    return entity.continent;
}

int DxccDatabase::getDxcc(const QString &callsign) const
{
    DxccEntity entity = lookupCallsign(callsign);
    return entity.dxcc;
}

int DxccDatabase::getItuZone(const QString &callsign) const
{
    DxccEntity entity = lookupCallsign(callsign);
    return entity.ituZone;
}

int DxccDatabase::mapItuZoneToRegion(int ituZone) const
{
    // ITU Region mapping based on ITU zones
    // Region 1 → ITU zones 14–30, 32–45, 48, 49
    // Region 2 → ITU zones 7–13, 15–17, 31, 46, 47
    // Region 3 → ITU zones 1–6, 50–75, plus zone 34 (some overlap exceptions)
    
    if ((ituZone >= 14 && ituZone <= 30) || 
        (ituZone >= 32 && ituZone <= 45) || 
        ituZone == 48 || ituZone == 49) {
        return 1;
    } else if ((ituZone >= 7 && ituZone <= 13) || 
               (ituZone >= 15 && ituZone <= 17) || 
               ituZone == 31 || ituZone == 46 || ituZone == 47) {
        return 2;
    } else if ((ituZone >= 1 && ituZone <= 6) || 
               (ituZone >= 50 && ituZone <= 75) || ituZone == 34) {
        return 3;
    }
    return 0; // Invalid zone
}

int DxccDatabase::getItuRegion(const QString &callsign) const
{
    QString callWithoutSuffix = stripPortableSuffixes(callsign);
    
    // Special case overrides: some DXCC entities are in different regions than their ITU zones suggest
    // U.S. territories in the Pacific that are assigned to Region 2
    if (callWithoutSuffix.startsWith("KL") ||  // Alaska
        callWithoutSuffix.startsWith("KH6") ||  // Hawaii
        callWithoutSuffix.startsWith("KH2") ||  // Guam
        callWithoutSuffix.startsWith("KH0") ||  // Northern Mariana Islands
        callWithoutSuffix.startsWith("KH8") ||  // American Samoa
        callWithoutSuffix.startsWith("KH9")) {  // Wake Island
        return 2;  // All U.S. Pacific territories are in Region 2
    }
    
    // Caribbean French territories assigned to Region 2 (despite France being Region 1)
    if (callWithoutSuffix.startsWith("FG") ||  // Guadeloupe
        callWithoutSuffix.startsWith("FM") ||  // Martinique
        callWithoutSuffix.startsWith("FY")) {  // French Guiana
        return 2;
    }
    
    int ituZone = getItuZone(callsign);
    return mapItuZoneToRegion(ituZone);
}
