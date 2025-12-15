#include "DxccDatabase.h"
#include "debuglogger.h"
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
    QString appDir = QCoreApplication::applicationDirPath();
    QString dataDir = QDir(appDir).filePath("../data");
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
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

        // Check if this is a country definition line (has multiple colons in specific pattern)
        int colonCount = line.count(':');
        if (colonCount >= 8) {
            // Parse the fixed-width country definition line
            // Format: Country Name:CQ:ITU:Cont:Lat:Long:TZ:PFX:
            DxccEntity entity;
            
            // Country name (columns 1-26)
            entity.country = line.mid(0, qMin(26, line.length())).trimmed();
            
            // CQ Zone (columns 27-31)
            if (line.length() > 26) {
                entity.cqZone = line.mid(26, 5).trimmed().toInt();
            }
            
            // ITU Zone (columns 32-36)
            if (line.length() > 31) {
                entity.ituZone = line.mid(31, 5).trimmed().toInt();
            }
            
            // Continent (columns 37-41)
            if (line.length() > 36) {
                entity.continent = line.mid(36, 5).trimmed();
            }
            
            // Latitude (columns 42-50)
            if (line.length() > 41) {
                entity.latitude = line.mid(41, 9).trimmed().toDouble();
            }
            
            // Longitude (columns 51-60)
            if (line.length() > 50) {
                entity.longitude = line.mid(50, 10).trimmed().toDouble();
            }
            
            // GMT Offset (columns 61-69)
            if (line.length() > 60) {
                entity.gmtOffset = line.mid(60, 9).trimmed().toDouble();
            }
            
            // Primary prefix (columns 70+)
            QString primaryPrefix;
            if (line.length() > 69) {
                primaryPrefix = line.mid(69).trimmed();
                // Remove trailing colon
                if (primaryPrefix.endsWith(':')) {
                    primaryPrefix.chop(1);
                }
                primaryPrefix = primaryPrefix.trimmed();
                
                // Handle * prefix (WAEDC only)
                if (primaryPrefix.startsWith('*')) {
                    primaryPrefix = primaryPrefix.mid(1);
                }
            }
            
            entity.dxcc = nextDxccNumber++;

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

QString DxccDatabase::findBestMatch(const QString &callsign) const
{
    QString call = callsign.toUpper();
    
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
