/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "settings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

Settings& Settings::instance()
{
    static Settings inst;
    return inst;
}

Settings::Settings() : m_modified(false)
{
    load();
}

QString Settings::settingsFilePath() const
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir dir(configPath);
    if (!dir.exists("ContestLogX")) {
        dir.mkpath("ContestLogX");
    }
    dir.cd("ContestLogX");
    return dir.filePath("ContestLogX.json");
}

void Settings::load()
{
    QString path = settingsFilePath();
    QFile file(path);
    
    if (!file.exists()) {
        // Create default settings
        m_settings = QJsonObject();
        m_settings["station"] = QJsonObject();
        m_settings["rig"] = QJsonObject{
            {"flrigHost", "localhost"},
            {"flrigPort", 12345},
            {"pollInterval", 500},
            {"autoConnect", false}
        };
        m_settings["window"] = QJsonObject{
            {"geometry", QJsonObject{
                {"x", 370},
                {"y", 194},
                {"width", 1566},
                {"height", 905}
            }},
            {"maximized", false}
        };
        save();
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open settings file for reading:" << path;
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "Settings file is not a valid JSON object";
        return;
    }
    
    m_settings = doc.object();
}

void Settings::save()
{
    QString path = settingsFilePath();
    QFile file(path);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not open settings file for writing:" << path;
        return;
    }
    
    QJsonDocument doc(m_settings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    m_modified = false;
}

QString Settings::getCallsign() const
{
    return m_settings["station"].toObject()["callsign"].toString("");
}

void Settings::setCallsign(const QString& call)
{
    QJsonObject station = m_settings["station"].toObject();
    station["callsign"] = call;
    m_settings["station"] = station;
    m_modified = true;
}

QString Settings::getOperatorName() const
{
    return m_settings["station"].toObject()["operatorName"].toString("");
}

void Settings::setOperatorName(const QString& name)
{
    QJsonObject station = m_settings["station"].toObject();
    station["operatorName"] = name;
    m_settings["station"] = station;
    m_modified = true;
}

QString Settings::getGridSquare() const
{
    return m_settings["station"].toObject()["gridSquare"].toString("");
}

void Settings::setGridSquare(const QString& grid)
{
    QJsonObject station = m_settings["station"].toObject();
    station["gridSquare"] = grid;
    m_settings["station"] = station;
    m_modified = true;
}

QString Settings::getState() const
{
    return m_settings["station"].toObject()["state"].toString("");
}

void Settings::setState(const QString& state)
{
    QJsonObject station = m_settings["station"].toObject();
    station["state"] = state;
    m_settings["station"] = station;
    m_modified = true;
}

QString Settings::getFlrigHost() const
{
    return m_settings["rig"].toObject()["flrigHost"].toString("localhost");
}

void Settings::setFlrigHost(const QString& host)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["flrigHost"] = host;
    m_settings["rig"] = rig;
    save();
}

int Settings::getFlrigPort() const
{
    return m_settings["rig"].toObject()["flrigPort"].toInt(12345);
}

void Settings::setFlrigPort(int port)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["flrigPort"] = port;
    m_settings["rig"] = rig;
    save();
}

int Settings::getFlrigPollInterval() const
{
    return m_settings["rig"].toObject()["pollInterval"].toInt(500);
}

void Settings::setFlrigPollInterval(int ms)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["pollInterval"] = ms;
    m_settings["rig"] = rig;
    save();
}

bool Settings::getFlrigAutoConnect() const
{
    return m_settings["rig"].toObject()["autoConnect"].toBool(false);
}

void Settings::setFlrigAutoConnect(bool autoConnect)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["autoConnect"] = autoConnect;
    m_settings["rig"] = rig;
    save();
    qDebug() << "Saved AutoConnect setting:" << autoConnect;
}

int Settings::getCwWpm() const
{
    return m_settings["cw"].toObject()["wpm"].toInt(28);
}

void Settings::setCwWpm(int wpm)
{
    QJsonObject cw = m_settings["cw"].toObject();
    cw["wpm"] = wpm;
    m_settings["cw"] = cw;
    save();
}

QRect Settings::getWindowGeometry() const
{
    QJsonObject window = m_settings["window"].toObject();
    QJsonObject geom = window["geometry"].toObject();
    
    return QRect(
        geom["x"].toInt(370),
        geom["y"].toInt(194),
        geom["width"].toInt(1566),
        geom["height"].toInt(905)
    );
}

void Settings::setWindowGeometry(const QRect& geometry)
{
    QJsonObject window = m_settings["window"].toObject();
    window["geometry"] = QJsonObject{
        {"x", geometry.x()},
        {"y", geometry.y()},
        {"width", geometry.width()},
        {"height", geometry.height()}
    };
    m_settings["window"] = window;
    save();
}

bool Settings::getWindowMaximized() const
{
    return m_settings["window"].toObject()["maximized"].toBool(false);
}

void Settings::setWindowMaximized(bool maximized)
{
    QJsonObject window = m_settings["window"].toObject();
    window["maximized"] = maximized;
    m_settings["window"] = window;
    save();
}

QMap<int, int> Settings::getColumnWidths() const
{
    QMap<int, int> widths;
    QJsonObject columns = m_settings["columns"].toObject();
    
    for (auto it = columns.constBegin(); it != columns.constEnd(); ++it) {
        bool ok;
        int column = it.key().toInt(&ok);
        if (ok) {
            widths[column] = it.value().toInt();
        }
    }
    
    return widths;
}

void Settings::setColumnWidth(int column, int width)
{
    QJsonObject columns = m_settings["columns"].toObject();
    columns[QString::number(column)] = width;
    m_settings["columns"] = columns;
    save();
}

QList<CwMemory> Settings::getCwMemories() const
{
    QList<CwMemory> memories;
    QJsonArray memArray = m_settings["cw"].toObject()["memories"].toArray();
    
    for (const QJsonValue& val : memArray) {
        QJsonObject obj = val.toObject();
        CwMemory mem;
        mem.abbreviation = obj["abbreviation"].toString();
        mem.text = obj["text"].toString();
        memories.append(mem);
    }
    
    // Ensure we always have 8 memories (even if empty)
    while (memories.size() < 8) {
        memories.append(CwMemory{"", ""});
    }
    
    return memories;
}

void Settings::setCwMemories(const QList<CwMemory>& memories)
{
    QJsonArray memArray;
    for (const CwMemory& mem : memories) {
        QJsonObject obj;
        obj["abbreviation"] = mem.abbreviation;
        obj["text"] = mem.text;
        memArray.append(obj);
    }
    
    QJsonObject cw = m_settings["cw"].toObject();
    cw["memories"] = memArray;
    m_settings["cw"] = cw;
    save();
}

QList<int> Settings::getMainSplitterSizes() const
{
    QList<int> sizes;
    QJsonArray array = m_settings["splitters"].toObject()["main"].toArray();
    for (const QJsonValue& val : array) {
        sizes.append(val.toInt());
    }
    return sizes;
}

void Settings::setMainSplitterSizes(const QList<int>& sizes)
{
    QJsonArray array;
    for (int size : sizes) {
        array.append(size);
    }
    
    QJsonObject splitters = m_settings["splitters"].toObject();
    splitters["main"] = array;
    m_settings["splitters"] = splitters;
    save();
}

QList<int> Settings::getRightPanelSplitterSizes() const
{
    QList<int> sizes;
    QJsonArray array = m_settings["splitters"].toObject()["rightPanel"].toArray();
    for (const QJsonValue& val : array) {
        sizes.append(val.toInt());
    }
    return sizes;
}

void Settings::setRightPanelSplitterSizes(const QList<int>& sizes)
{
    QJsonArray array;
    for (int size : sizes) {
        array.append(size);
    }
    
    QJsonObject splitters = m_settings["splitters"].toObject();
    splitters["rightPanel"] = array;
    m_settings["splitters"] = splitters;
    save();
}

QString Settings::getDxClusterServer() const
{
    return m_settings["dxCluster"].toObject()["server"].toString("");
}

void Settings::setDxClusterServer(const QString& server)
{
    QJsonObject dxCluster = m_settings["dxCluster"].toObject();
    dxCluster["server"] = server;
    m_settings["dxCluster"] = dxCluster;
    save();
}

QString Settings::getDxClusterCallsign() const
{
    return m_settings["dxCluster"].toObject()["callsign"].toString("");
}

void Settings::setDxClusterCallsign(const QString& callsign)
{
    QJsonObject dxCluster = m_settings["dxCluster"].toObject();
    dxCluster["callsign"] = callsign;
    m_settings["dxCluster"] = dxCluster;
    save();
}

bool Settings::getDxClusterVisible() const
{
    return m_settings["ui"].toObject()["dxClusterVisible"].toBool(true);
}

void Settings::setDxClusterVisible(bool visible)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["dxClusterVisible"] = visible;
    m_settings["ui"] = ui;
    save();
}

bool Settings::getCwConsoleVisible() const
{
    return m_settings["ui"].toObject()["cwConsoleVisible"].toBool(true);
}

void Settings::setCwConsoleVisible(bool visible)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["cwConsoleVisible"] = visible;
    m_settings["ui"] = ui;
    save();
}

QByteArray Settings::getMainSplitterState() const
{
    QString base64 = m_settings["ui"].toObject()["mainSplitterState"].toString("");
    return QByteArray::fromBase64(base64.toUtf8());
}

void Settings::setMainSplitterState(const QByteArray& state)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["mainSplitterState"] = QString::fromUtf8(state.toBase64());
    m_settings["ui"] = ui;
    save();
}

QByteArray Settings::getRightPanelSplitterState() const
{
    QString base64 = m_settings["ui"].toObject()["rightPanelSplitterState"].toString("");
    return QByteArray::fromBase64(base64.toUtf8());
}

void Settings::setRightPanelSplitterState(const QByteArray& state)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["rightPanelSplitterState"] = QString::fromUtf8(state.toBase64());
    m_settings["ui"] = ui;
    save();
}

bool Settings::getFlrigDebugEnabled() const
{
    return m_settings["debug"].toObject()["flrigDebugEnabled"].toBool(false);
}

void Settings::setFlrigDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["flrigDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getMainWindowDebugEnabled() const
{
    return m_settings["debug"].toObject()["mainWindowDebugEnabled"].toBool(true);
}

void Settings::setMainWindowDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["mainWindowDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getContestEngineDebugEnabled() const
{
    return m_settings["debug"].toObject()["contestEngineDebugEnabled"].toBool(true);
}

void Settings::setContestEngineDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["contestEngineDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getCWWindowDebugEnabled() const
{
    return m_settings["debug"].toObject()["cwWindowDebugEnabled"].toBool(false);
}

void Settings::setCWWindowDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["cwWindowDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getDxccDatabaseDebugEnabled() const
{
    return m_settings["debug"].toObject()["dxccDatabaseDebugEnabled"].toBool(false);
}

void Settings::setDxccDatabaseDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["dxccDatabaseDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}
