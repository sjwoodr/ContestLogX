/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "settings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QApplication>
#include <QScreen>

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
        // Load default layout from data/default_layout.json
        QString defaultLayoutPath = getDataPath() + "/default_layout.json";
        QFile defaultFile(defaultLayoutPath);
        
        if (defaultFile.open(QIODevice::ReadOnly)) {
            QByteArray data = defaultFile.readAll();
            defaultFile.close();
            
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                m_settings = doc.object();
                // Scale layout if needed based on screen size
                scaleDefaultLayout();
                save();
                return;
            }
        }
        
        // Fallback to hardcoded defaults if default_layout.json doesn't exist
        m_settings = QJsonObject();
        m_settings["station"] = QJsonObject();
        m_settings["rig"] = QJsonObject{
            {"backend", "flrig"},
            {"flrigHost", "localhost"},
            {"flrigPort", 12345},
            {"pollInterval", 500},
            {"autoConnect", false},
            {"hamlibHost", "localhost"},
            {"hamlibPort", 4532},
            {"hamlibAutoConnect", false}
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
        // Add default shortcuts
        QJsonObject shortcuts;
        shortcuts["clearQsoEntry"] = "Ctrl+W";
        m_settings["shortcuts"] = shortcuts;
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
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Could not open settings file for writing:" << path;
        qWarning() << "Error:" << file.errorString();
        return;
    }
    
    QJsonDocument doc(m_settings);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
    qint64 bytesWritten = file.write(jsonData);
    
    if (bytesWritten == -1) {
        qWarning() << "Failed to write settings to file:" << file.errorString();
        file.close();
        return;
    }
    
    if (bytesWritten != jsonData.size()) {
        qWarning() << "Warning: Only wrote" << bytesWritten << "of" << jsonData.size() << "bytes";
    }
    
    // Ensure data is written to disk
    if (!file.flush()) {
        qWarning() << "Failed to flush settings file:" << file.errorString();
    }
    
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

QString Settings::getDxccCountry() const
{
    return m_settings["station"].toObject()["dxccCountry"].toString("");
}

void Settings::setDxccCountry(const QString& prefix)
{
    QJsonObject station = m_settings["station"].toObject();
    station["dxccCountry"] = prefix;
    m_settings["station"] = station;
    m_modified = true;
}

int Settings::getCqZone() const
{
    return m_settings["station"].toObject()["cqZone"].toInt(0);
}

void Settings::setCqZone(int zone)
{
    QJsonObject station = m_settings["station"].toObject();
    station["cqZone"] = zone;
    m_settings["station"] = station;
    m_modified = true;
}

int Settings::getItuZone() const
{
    return m_settings["station"].toObject()["ituZone"].toInt(0);
}

void Settings::setItuZone(int zone)
{
    QJsonObject station = m_settings["station"].toObject();
    station["ituZone"] = zone;
    m_settings["station"] = station;
    m_modified = true;
}

QString Settings::getArrlSection() const
{
    return m_settings["station"].toObject()["arrlSection"].toString("");
}

void Settings::setArrlSection(const QString& section)
{
    QJsonObject station = m_settings["station"].toObject();
    station["arrlSection"] = section;
    m_settings["station"] = station;
    m_modified = true;
}

QString Settings::getRigBackend() const
{
    return m_settings["rig"].toObject()["backend"].toString("flrig");
}

void Settings::setRigBackend(const QString& backend)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["backend"] = backend;
    m_settings["rig"] = rig;
    save();
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

QString Settings::getHamlibHost() const
{
    return m_settings["rig"].toObject()["hamlibHost"].toString("localhost");
}

void Settings::setHamlibHost(const QString& host)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["hamlibHost"] = host;
    m_settings["rig"] = rig;
    save();
}

int Settings::getHamlibPort() const
{
    return m_settings["rig"].toObject()["hamlibPort"].toInt(4532);
}

void Settings::setHamlibPort(int port)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["hamlibPort"] = port;
    m_settings["rig"] = rig;
    save();
}

bool Settings::getHamlibAutoConnect() const
{
    return m_settings["rig"].toObject()["hamlibAutoConnect"].toBool(false);
}

void Settings::setHamlibAutoConnect(bool autoConnect)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["hamlibAutoConnect"] = autoConnect;
    m_settings["rig"] = rig;
    save();
}

bool Settings::getMockedAutoConnect() const
{
    return m_settings["rig"].toObject()["mockedAutoConnect"].toBool(false);
}

void Settings::setMockedAutoConnect(bool autoConnect)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["mockedAutoConnect"] = autoConnect;
    m_settings["rig"] = rig;
    save();
}

// WSJT-X integration

bool Settings::getWsjtxEnabled() const
{
    return m_settings["wsjtx"].toObject()["enabled"].toBool(false);
}

void Settings::setWsjtxEnabled(bool enabled)
{
    QJsonObject wsjtx = m_settings["wsjtx"].toObject();
    wsjtx["enabled"] = enabled;
    m_settings["wsjtx"] = wsjtx;
    save();
}

int Settings::getWsjtxPort() const
{
    return m_settings["wsjtx"].toObject()["port"].toInt(2237);
}

void Settings::setWsjtxPort(int port)
{
    QJsonObject wsjtx = m_settings["wsjtx"].toObject();
    wsjtx["port"] = port;
    m_settings["wsjtx"] = wsjtx;
    save();
}

// SO2R settings

bool Settings::getSo2rEnabled() const
{
    return m_settings["so2r"].toObject()["enabled"].toBool(false);
}

void Settings::setSo2rEnabled(bool enabled)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    so2r["enabled"] = enabled;
    m_settings["so2r"] = so2r;
    save();
}

QString Settings::getRadioRRigBackend() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["backend"].toString("flrig");
}

void Settings::setRadioRRigBackend(const QString& backend)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["backend"] = backend;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

QString Settings::getRadioRFlrigHost() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["flrigHost"].toString("localhost");
}

void Settings::setRadioRFlrigHost(const QString& host)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["flrigHost"] = host;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

int Settings::getRadioRFlrigPort() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["flrigPort"].toInt(12345);
}

void Settings::setRadioRFlrigPort(int port)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["flrigPort"] = port;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

bool Settings::getRadioRFlrigAutoConnect() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["flrigAutoConnect"].toBool(false);
}

void Settings::setRadioRFlrigAutoConnect(bool autoConnect)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["flrigAutoConnect"] = autoConnect;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

QString Settings::getRadioRHamlibHost() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["hamlibHost"].toString("localhost");
}

void Settings::setRadioRHamlibHost(const QString& host)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["hamlibHost"] = host;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

int Settings::getRadioRHamlibPort() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["hamlibPort"].toInt(4532);
}

void Settings::setRadioRHamlibPort(int port)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["hamlibPort"] = port;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

bool Settings::getRadioRHamlibAutoConnect() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["hamlibAutoConnect"].toBool(false);
}

void Settings::setRadioRHamlibAutoConnect(bool autoConnect)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["hamlibAutoConnect"] = autoConnect;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

bool Settings::getRadioRMockedAutoConnect() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["mockedAutoConnect"].toBool(false);
}

void Settings::setRadioRMockedAutoConnect(bool autoConnect)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["mockedAutoConnect"] = autoConnect;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

// ---------- CW Decoder — per-radio audio input + PTT mute (SPEC-005) ----------

QString Settings::getRadioLAudioInputDevice() const
{
    return m_settings["rig"].toObject()["audioInputDevice"].toString();
}

void Settings::setRadioLAudioInputDevice(const QString& deviceDescription)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["audioInputDevice"] = deviceDescription;
    m_settings["rig"] = rig;
    save();
}

bool Settings::getRadioLMuteDecoderOnPtt() const
{
    return m_settings["rig"].toObject()["muteDecoderOnPtt"].toBool(true);
}

void Settings::setRadioLMuteDecoderOnPtt(bool enabled)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["muteDecoderOnPtt"] = enabled;
    m_settings["rig"] = rig;
    save();
}

int Settings::getRadioLDecoderPttGraceMs() const
{
    return m_settings["rig"].toObject()["decoderPttGraceMs"].toInt(250);
}

void Settings::setRadioLDecoderPttGraceMs(int ms)
{
    QJsonObject rig = m_settings["rig"].toObject();
    rig["decoderPttGraceMs"] = ms;
    m_settings["rig"] = rig;
    save();
}

QString Settings::getRadioRAudioInputDevice() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["audioInputDevice"].toString();
}

void Settings::setRadioRAudioInputDevice(const QString& deviceDescription)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["audioInputDevice"] = deviceDescription;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

bool Settings::getRadioRMuteDecoderOnPtt() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["muteDecoderOnPtt"].toBool(true);
}

void Settings::setRadioRMuteDecoderOnPtt(bool enabled)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["muteDecoderOnPtt"] = enabled;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

int Settings::getRadioRDecoderPttGraceMs() const
{
    return m_settings["so2r"].toObject()["radioR"].toObject()["decoderPttGraceMs"].toInt(250);
}

void Settings::setRadioRDecoderPttGraceMs(int ms)
{
    QJsonObject so2r = m_settings["so2r"].toObject();
    QJsonObject radioR = so2r["radioR"].toObject();
    radioR["decoderPttGraceMs"] = ms;
    so2r["radioR"] = radioR;
    m_settings["so2r"] = so2r;
    save();
}

QJsonObject Settings::getCwDecoderSettings(bool right) const
{
    const QJsonObject audio = m_settings["cwDecoder"].toObject();
    return audio[right ? "right" : "left"].toObject();
}

void Settings::setCwDecoderSettings(bool right, const QJsonObject& obj)
{
    QJsonObject audio = m_settings["cwDecoder"].toObject();
    audio[right ? "right" : "left"] = obj;
    m_settings["cwDecoder"] = audio;
    save();
}

int Settings::getCwDecoderPassbandLowHz(bool right) const
{
    return getCwDecoderSettings(right).value("passbandLowHz").toInt(600);
}
void Settings::setCwDecoderPassbandLowHz(bool right, int hz)
{
    QJsonObject o = getCwDecoderSettings(right); o["passbandLowHz"] = hz; setCwDecoderSettings(right, o);
}
int Settings::getCwDecoderPassbandHighHz(bool right) const
{
    return getCwDecoderSettings(right).value("passbandHighHz").toInt(900);
}
void Settings::setCwDecoderPassbandHighHz(bool right, int hz)
{
    QJsonObject o = getCwDecoderSettings(right); o["passbandHighHz"] = hz; setCwDecoderSettings(right, o);
}
int Settings::getCwDecoderBinCount(bool right) const
{
    return getCwDecoderSettings(right).value("binCount").toInt(6);
}
void Settings::setCwDecoderBinCount(bool right, int n)
{
    QJsonObject o = getCwDecoderSettings(right); o["binCount"] = n; setCwDecoderSettings(right, o);
}
int Settings::getCwDecoderCenterHz(bool right) const
{
    const QJsonObject obj = getCwDecoderSettings(right);
    // Preferred: explicit centerHz. Migration: if only the old
    // passbandLowHz/passbandHighHz are present, compute midpoint from them.
    // Fallback: 700 Hz (common CW sidetone default).
    if (obj.contains("centerHz")) return obj.value("centerHz").toInt(700);
    if (obj.contains("passbandLowHz") && obj.contains("passbandHighHz")) {
        const int lo = obj.value("passbandLowHz").toInt(600);
        const int hi = obj.value("passbandHighHz").toInt(900);
        return (lo + hi) / 2;
    }
    return 700;
}
void Settings::setCwDecoderCenterHz(bool right, int hz)
{
    QJsonObject o = getCwDecoderSettings(right); o["centerHz"] = hz; setCwDecoderSettings(right, o);
}
double Settings::getCwDecoderSquelch(bool right) const
{
    return getCwDecoderSettings(right).value("squelchThreshold").toDouble(0.05);
}
void Settings::setCwDecoderSquelch(bool right, double threshold)
{
    QJsonObject o = getCwDecoderSettings(right); o["squelchThreshold"] = threshold; setCwDecoderSettings(right, o);
}
int Settings::getCwDecoderWpmMin(bool right) const
{
    return getCwDecoderSettings(right).value("wpmMin").toInt(5);
}
void Settings::setCwDecoderWpmMin(bool right, int wpm)
{
    QJsonObject o = getCwDecoderSettings(right); o["wpmMin"] = wpm; setCwDecoderSettings(right, o);
}
int Settings::getCwDecoderWpmMax(bool right) const
{
    return getCwDecoderSettings(right).value("wpmMax").toInt(60);
}
void Settings::setCwDecoderWpmMax(bool right, int wpm)
{
    QJsonObject o = getCwDecoderSettings(right); o["wpmMax"] = wpm; setCwDecoderSettings(right, o);
}
double Settings::getCwDecoderWordGap(bool right) const
{
    // 4.0 = current decoder default, contest-friendly compromise between
    // textbook 7× spacing and tight QRQ contest CW that runs ~3×.
    return getCwDecoderSettings(right).value("wordGapMultiplier").toDouble(4.0);
}
void Settings::setCwDecoderWordGap(bool right, double multiplier)
{
    QJsonObject o = getCwDecoderSettings(right); o["wordGapMultiplier"] = multiplier; setCwDecoderSettings(right, o);
}
bool Settings::getCwDecoderSquelchAuto(bool right) const
{
    return getCwDecoderSettings(right).value("squelchAuto").toBool(false);
}
void Settings::setCwDecoderSquelchAuto(bool right, bool enabled)
{
    QJsonObject o = getCwDecoderSettings(right); o["squelchAuto"] = enabled; setCwDecoderSettings(right, o);
}

// ---------- end CW Decoder settings ----------

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

QByteArray Settings::getWindowGeometryState() const
{
    QString base64 = m_settings["window"].toObject()["geometryState"].toString();
    return QByteArray::fromBase64(base64.toUtf8());
}

void Settings::setWindowGeometryState(const QByteArray& state)
{
    QJsonObject window = m_settings["window"].toObject();
    window["geometryState"] = QString::fromUtf8(state.toBase64());
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
        mem.role = memoryRoleFromString(obj["role"].toString());
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
        QString roleStr = memoryRoleToString(mem.role);
        if (!roleStr.isEmpty()) obj["role"] = roleStr;
        memArray.append(obj);
    }

    QJsonObject cw = m_settings["cw"].toObject();
    cw["memories"] = memArray;
    m_settings["cw"] = cw;
    save();
}

int Settings::getCwSnPadding() const
{
    return m_settings["cw"].toObject()["snPadding"].toInt(1);
}

void Settings::setCwSnPadding(int digits)
{
    QJsonObject cw = m_settings["cw"].toObject();
    cw["snPadding"] = digits;
    m_settings["cw"] = cw;
    save();
}

bool Settings::getCwSnCutNumbers() const
{
    return m_settings["cw"].toObject()["snCutNumbers"].toBool(false);
}

void Settings::setCwSnCutNumbers(bool enabled)
{
    QJsonObject cw = m_settings["cw"].toObject();
    cw["snCutNumbers"] = enabled;
    m_settings["cw"] = cw;
    save();
}

QList<SsbMemory> Settings::getSsbMemories() const
{
    QList<SsbMemory> memories;
    QJsonArray memArray = m_settings["ssb"].toObject()["memories"].toArray();

    for (const QJsonValue& val : memArray) {
        QJsonObject obj = val.toObject();
        SsbMemory mem;
        mem.abbreviation = obj["abbreviation"].toString();
        mem.text = obj["text"].toString();
        mem.role = memoryRoleFromString(obj["role"].toString());
        memories.append(mem);
    }

    // Ensure we always have 8 memories (even if empty)
    while (memories.size() < 8) {
        memories.append(SsbMemory{"", ""});
    }

    return memories;
}

void Settings::setSsbMemories(const QList<SsbMemory>& memories)
{
    QJsonArray memArray;
    for (const SsbMemory& mem : memories) {
        QJsonObject obj;
        obj["abbreviation"] = mem.abbreviation;
        obj["text"] = mem.text;
        QString roleStr = memoryRoleToString(mem.role);
        if (!roleStr.isEmpty()) obj["role"] = roleStr;
        memArray.append(obj);
    }

    QJsonObject ssb = m_settings["ssb"].toObject();
    ssb["memories"] = memArray;
    m_settings["ssb"] = ssb;
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

QStringList Settings::getDxClusterServers() const
{
    QJsonObject dxCluster = m_settings["dxCluster"].toObject();
    if (dxCluster.contains("servers")) {
        QStringList servers;
        for (const QJsonValue& v : dxCluster["servers"].toArray())
            servers.append(v.toString());
        return servers;
    }

    // Key not yet present — seed from default_dxclusters.json
    QString defaultPath = getDataPath() + "/default_dxclusters.json";
    QFile f(defaultPath);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isObject()) {
            QStringList servers;
            for (const QJsonValue& v : doc.object()["servers"].toArray())
                servers.append(v.toString());
            return servers;
        }
    }
    return {};
}

void Settings::setDxClusterServers(const QStringList& servers)
{
    QJsonArray arr;
    for (const QString& s : servers)
        arr.append(s);
    QJsonObject dxCluster = m_settings["dxCluster"].toObject();
    dxCluster["servers"] = arr;
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

QByteArray Settings::getDockWidgetState() const
{
    QString base64 = m_settings["ui"].toObject()["dockWidgetState"].toString("");
    return QByteArray::fromBase64(base64.toLatin1());
}

void Settings::setDockWidgetState(const QByteArray& state)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["dockWidgetState"] = QString::fromLatin1(state.toBase64());
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

bool Settings::getContestSelectDialogDebugEnabled() const
{
    return m_settings["debug"].toObject()["contestSelectDialogDebugEnabled"].toBool(false);
}

void Settings::setContestSelectDialogDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["contestSelectDialogDebugEnabled"] = enabled;
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

bool Settings::getScpDebugEnabled() const
{
    return m_settings["debug"].toObject()["scpDebugEnabled"].toBool(false);
}

void Settings::setScpDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["scpDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getCallsignLookupDebugEnabled() const
{
    return m_settings["debug"].toObject()["callsignLookupDebugEnabled"].toBool(false);
}

void Settings::setCallsignLookupDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["callsignLookupDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getMultiplierWidgetDebugEnabled() const
{
    return m_settings["debug"].toObject()["multiplierWidgetDebugEnabled"].toBool(false);
}

void Settings::setMultiplierWidgetDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["multiplierWidgetDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getWsjtxDebugEnabled() const
{
    return m_settings["debug"].toObject()["wsjtxDebugEnabled"].toBool(false);
}

void Settings::setWsjtxDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["wsjtxDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getCwDecoderDebugEnabled() const
{
    return m_settings["debug"].toObject()["cwDecoderDebugEnabled"].toBool(false);
}

void Settings::setCwDecoderDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["cwDecoderDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

bool Settings::getDxClusterDebugEnabled() const
{
    return m_settings["debug"].toObject()["dxClusterDebugEnabled"].toBool(false);
}

void Settings::setDxClusterDebugEnabled(bool enabled)
{
    QJsonObject debug = m_settings["debug"].toObject();
    debug["dxClusterDebugEnabled"] = enabled;
    m_settings["debug"] = debug;
    save();
}

QMap<QString, QString> Settings::getShortcuts() const
{
    QMap<QString, QString> shortcuts;
    QJsonObject shortcutsObj = m_settings["shortcuts"].toObject();
    for (auto it = shortcutsObj.begin(); it != shortcutsObj.end(); ++it) {
        shortcuts[it.key()] = it.value().toString();
    }
    return shortcuts;
}

void Settings::setShortcuts(const QMap<QString, QString>& shortcuts)
{
    QJsonObject shortcutsObj;
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        shortcutsObj[it.key()] = it.value();
    }
    m_settings["shortcuts"] = shortcutsObj;
    save();
}

QString Settings::getShortcut(const QString& actionName) const
{
    QString value = m_settings["shortcuts"].toObject()[actionName].toString("");
    
    // Return default if not found
    if (value.isEmpty()) {
        // Check built-in defaults
        static const QMap<QString, QString> DEFAULTS = {
            {"clearQsoEntry", "Ctrl+W"}
        };
        return DEFAULTS.value(actionName, "");
    }
    
    return value;
}

void Settings::setShortcut(const QString& actionName, const QString& keySequence)
{
    QJsonObject shortcuts = m_settings["shortcuts"].toObject();
    shortcuts[actionName] = keySequence;
    m_settings["shortcuts"] = shortcuts;
    save();
}

QString Settings::getCabrilloEmail() const
{
    return m_settings["cabrillo"].toObject()["email"].toString("");
}

void Settings::setCabrilloEmail(const QString& email)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["email"] = email;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloAddressCity() const
{
    return m_settings["cabrillo"].toObject()["addressCity"].toString("");
}

void Settings::setCabrilloAddressCity(const QString& city)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["addressCity"] = city;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloAddress() const
{
    return m_settings["cabrillo"].toObject()["address"].toString("");
}

void Settings::setCabrilloAddress(const QString& address)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["address"] = address;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloPostalCode() const
{
    return m_settings["cabrillo"].toObject()["postalCode"].toString("");
}

void Settings::setCabrilloPostalCode(const QString& code)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["postalCode"] = code;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloCountry() const
{
    return m_settings["cabrillo"].toObject()["country"].toString("");
}

void Settings::setCabrilloCountry(const QString& country)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["country"] = country;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloClub() const
{
    return m_settings["cabrillo"].toObject()["club"].toString("");
}

void Settings::setCabrilloClub(const QString& club)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["club"] = club;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrillSoapbox() const
{
    return m_settings["cabrillo"].toObject()["soapbox"].toString("");
}

void Settings::setCabrillSoapbox(const QString& soapbox)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["soapbox"] = soapbox;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloCategory() const
{
    return m_settings["cabrillo"].toObject()["category"].toString("SINGLE-OP");
}

void Settings::setCabrilloCategory(const QString& category)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["category"] = category;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloPower() const
{
    return m_settings["cabrillo"].toObject()["power"].toString("HIGH");
}

void Settings::setCabrilloPower(const QString& power)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["power"] = power;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloMode() const
{
    return m_settings["cabrillo"].toObject()["mode"].toString("CW");
}

void Settings::setCabrilloMode(const QString& mode)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["mode"] = mode;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloOperatorType() const
{
    return m_settings["cabrillo"].toObject()["operatorType"].toString("SINGLE");
}

void Settings::setCabrilloOperatorType(const QString& opType)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["operatorType"] = opType;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloBand() const
{
    return m_settings["cabrillo"].toObject()["band"].toString("ALL-BAND");
}

void Settings::setCabrilloBand(const QString& band)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["band"] = band;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloTransmitter() const
{
    return m_settings["cabrillo"].toObject()["transmitter"].toString("ONE");
}

void Settings::setCabrilloTransmitter(const QString& transmitter)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["transmitter"] = transmitter;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloAssisted() const
{
    return m_settings["cabrillo"].toObject()["assisted"].toString("NO");
}

void Settings::setCabrilloAssisted(const QString& assisted)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["assisted"] = assisted;
    m_settings["cabrillo"] = cabrillo;
    save();
}

QString Settings::getCabrilloOverlay() const
{
    return m_settings["cabrillo"].toObject()["overlay"].toString("");
}

void Settings::setCabrilloOverlay(const QString& overlay)
{
    QJsonObject cabrillo = m_settings["cabrillo"].toObject();
    cabrillo["overlay"] = overlay;
    m_settings["cabrillo"] = cabrillo;
    save();
}

bool Settings::getCallHistoryEnabled() const
{
    return m_settings["callHistory"].toObject()["enabled"].toBool(false);
}

void Settings::setCallHistoryEnabled(bool enabled)
{
    QJsonObject callHistory = m_settings["callHistory"].toObject();
    callHistory["enabled"] = enabled;
    m_settings["callHistory"] = callHistory;
    save();
}

bool Settings::getCallHistoryAutoSaveEnabled() const
{
    return m_settings["callHistory"].toObject()["autoSave"].toBool(false);
}

void Settings::setCallHistoryAutoSaveEnabled(bool enabled)
{
    QJsonObject callHistory = m_settings["callHistory"].toObject();
    callHistory["autoSave"] = enabled;
    m_settings["callHistory"] = callHistory;
    save();
}

bool Settings::getScpEnabled() const
{
    return m_settings["scp"].toObject()["enabled"].toBool(false);
}

void Settings::setScpEnabled(bool enabled)
{
    QJsonObject scp = m_settings["scp"].toObject();
    scp["enabled"] = enabled;
    m_settings["scp"] = scp;
    m_modified = true;
}

QString Settings::getCallsignLookupService() const
{
    return m_settings["callsignLookup"].toObject()["service"].toString("none");
}

void Settings::setCallsignLookupService(const QString& service)
{
    QJsonObject obj = m_settings["callsignLookup"].toObject();
    obj["service"] = service;
    m_settings["callsignLookup"] = obj;
    m_modified = true;
}

bool Settings::getQrzcqAutoLookupEnabled() const
{
    return m_settings["qrzcq"].toObject()["autoLookupEnabled"].toBool(false);
}

void Settings::setQrzcqAutoLookupEnabled(bool enabled)
{
    QJsonObject qrzcq = m_settings["qrzcq"].toObject();
    qrzcq["autoLookupEnabled"] = enabled;
    m_settings["qrzcq"] = qrzcq;
    m_modified = true;
}

QString Settings::getQrzcqUsername() const
{
    QString encrypted = m_settings["qrzcq"].toObject()["username"].toString();
    if (encrypted.isEmpty()) return "";
    
    // Simple XOR decryption with base64
    QByteArray decoded = QByteArray::fromBase64(encrypted.toLatin1());
    const char key[] = "ContestLogX";
    QByteArray result;
    for (int i = 0; i < decoded.length(); ++i) {
        result.append(decoded[i] ^ key[i % strlen(key)]);
    }
    return QString::fromLatin1(result);
}

QString Settings::getQrzcqPassword() const
{
    QString encrypted = m_settings["qrzcq"].toObject()["password"].toString();
    if (encrypted.isEmpty()) return "";
    
    // Simple XOR decryption with base64
    QByteArray decoded = QByteArray::fromBase64(encrypted.toLatin1());
    const char key[] = "ContestLogX";
    QByteArray result;
    for (int i = 0; i < decoded.length(); ++i) {
        result.append(decoded[i] ^ key[i % strlen(key)]);
    }
    return QString::fromLatin1(result);
}

void Settings::setQrzcqCredentials(const QString& username, const QString& password)
{
    QJsonObject qrzcq = m_settings["qrzcq"].toObject();
    
    // Simple XOR encryption with base64
    const char key[] = "ContestLogX";
    
    if (!username.isEmpty()) {
        QByteArray userBytes = username.toLatin1();
        QByteArray encrypted;
        for (int i = 0; i < userBytes.length(); ++i) {
            encrypted.append(userBytes[i] ^ key[i % strlen(key)]);
        }
        qrzcq["username"] = QString::fromLatin1(encrypted.toBase64());
    } else {
        qrzcq["username"] = "";
    }
    
    if (!password.isEmpty()) {
        QByteArray passBytes = password.toLatin1();
        QByteArray encrypted;
        for (int i = 0; i < passBytes.length(); ++i) {
            encrypted.append(passBytes[i] ^ key[i % strlen(key)]);
        }
        qrzcq["password"] = QString::fromLatin1(encrypted.toBase64());
    } else {
        qrzcq["password"] = "";
    }
    
    m_settings["qrzcq"] = qrzcq;
    m_modified = true;
}

QString Settings::getQrzUsername() const
{
    QString encrypted = m_settings["qrz"].toObject()["username"].toString();
    if (encrypted.isEmpty()) return "";
    QByteArray decoded = QByteArray::fromBase64(encrypted.toLatin1());
    const char key[] = "ContestLogX";
    QByteArray result;
    for (int i = 0; i < decoded.length(); ++i)
        result.append(decoded[i] ^ key[i % strlen(key)]);
    return QString::fromLatin1(result);
}

QString Settings::getQrzPassword() const
{
    QString encrypted = m_settings["qrz"].toObject()["password"].toString();
    if (encrypted.isEmpty()) return "";
    QByteArray decoded = QByteArray::fromBase64(encrypted.toLatin1());
    const char key[] = "ContestLogX";
    QByteArray result;
    for (int i = 0; i < decoded.length(); ++i)
        result.append(decoded[i] ^ key[i % strlen(key)]);
    return QString::fromLatin1(result);
}

void Settings::setQrzCredentials(const QString& username, const QString& password)
{
    QJsonObject qrz = m_settings["qrz"].toObject();
    const char key[] = "ContestLogX";

    if (!username.isEmpty()) {
        QByteArray userBytes = username.toLatin1();
        QByteArray encrypted;
        for (int i = 0; i < userBytes.length(); ++i)
            encrypted.append(userBytes[i] ^ key[i % strlen(key)]);
        qrz["username"] = QString::fromLatin1(encrypted.toBase64());
    } else {
        qrz["username"] = "";
    }

    if (!password.isEmpty()) {
        QByteArray passBytes = password.toLatin1();
        QByteArray encrypted;
        for (int i = 0; i < passBytes.length(); ++i)
            encrypted.append(passBytes[i] ^ key[i % strlen(key)]);
        qrz["password"] = QString::fromLatin1(encrypted.toBase64());
    } else {
        qrz["password"] = "";
    }

    m_settings["qrz"] = qrz;
    m_modified = true;
}

// Online scoring settings
bool Settings::getOnlineScoringEnabled() const
{
    return m_settings["onlineScoring"].toObject()["enabled"].toBool(false);
}

void Settings::setOnlineScoringEnabled(bool enabled)
{
    QJsonObject os = m_settings["onlineScoring"].toObject();
    os["enabled"] = enabled;
    m_settings["onlineScoring"] = os;
    m_modified = true;
}

QString Settings::getOnlineScoringCallsign() const
{
    QString encrypted = m_settings["onlineScoring"].toObject()["callsign"].toString();
    if (encrypted.isEmpty()) return "";
    QByteArray decoded = QByteArray::fromBase64(encrypted.toLatin1());
    const char key[] = "ContestLogX";
    QByteArray result;
    for (int i = 0; i < decoded.length(); ++i)
        result.append(decoded[i] ^ key[i % strlen(key)]);
    return QString::fromLatin1(result);
}

QString Settings::getOnlineScoringPassword() const
{
    QString encrypted = m_settings["onlineScoring"].toObject()["password"].toString();
    if (encrypted.isEmpty()) return "";
    QByteArray decoded = QByteArray::fromBase64(encrypted.toLatin1());
    const char key[] = "ContestLogX";
    QByteArray result;
    for (int i = 0; i < decoded.length(); ++i)
        result.append(decoded[i] ^ key[i % strlen(key)]);
    return QString::fromLatin1(result);
}

void Settings::setOnlineScoringCredentials(const QString& callsign, const QString& password)
{
    QJsonObject os = m_settings["onlineScoring"].toObject();
    const char key[] = "ContestLogX";

    auto encode = [&](const QString& value) -> QString {
        if (value.isEmpty()) return "";
        QByteArray bytes = value.toLatin1();
        QByteArray encrypted;
        for (int i = 0; i < bytes.length(); ++i)
            encrypted.append(bytes[i] ^ key[i % strlen(key)]);
        return QString::fromLatin1(encrypted.toBase64());
    };

    os["callsign"] = encode(callsign);
    os["password"] = encode(password);
    m_settings["onlineScoring"] = os;
    m_modified = true;
}

int Settings::getOnlineScoringInterval() const
{
    return m_settings["onlineScoring"].toObject()["intervalMinutes"].toInt(5);
}

void Settings::setOnlineScoringInterval(int minutes)
{
    QJsonObject os = m_settings["onlineScoring"].toObject();
    os["intervalMinutes"] = minutes;
    m_settings["onlineScoring"] = os;
    m_modified = true;
}

bool Settings::getOnlineScoringPerQso() const
{
    return m_settings["onlineScoring"].toObject()["perQso"].toBool(false);
}

void Settings::setOnlineScoringPerQso(bool perQso)
{
    QJsonObject os = m_settings["onlineScoring"].toObject();
    os["perQso"] = perQso;
    m_settings["onlineScoring"] = os;
    m_modified = true;
}

QString Settings::getContestUserPrompt(const QString& contestFile, const QString& promptId) const
{
    QJsonObject contests = m_settings["contests"].toObject();
    QJsonObject contest = contests[contestFile].toObject();
    QJsonObject userPrompts = contest["userPrompts"].toObject();
    return userPrompts[promptId].toString();
}

void Settings::setContestUserPrompt(const QString& contestFile, const QString& promptId, const QString& value)
{
    QJsonObject contests = m_settings["contests"].toObject();
    QJsonObject contest = contests[contestFile].toObject();
    QJsonObject userPrompts = contest["userPrompts"].toObject();

    userPrompts[promptId] = value;
    contest["userPrompts"] = userPrompts;
    contests[contestFile] = contest;
    m_settings["contests"] = contests;
    m_modified = true;
}

void Settings::clearContestUserPrompts(const QString& contestFile)
{
    QJsonObject contests = m_settings["contests"].toObject();
    QJsonObject contest = contests[contestFile].toObject();
    contest["userPrompts"] = QJsonObject();  // Empty object
    contests[contestFile] = contest;
    m_settings["contests"] = contests;
    m_modified = true;
}

QByteArray Settings::getCabrilloDialogGeometry() const
{
    QString base64 = m_settings["dialogs"].toObject()["cabrilloGeometry"].toString();
    return QByteArray::fromBase64(base64.toLatin1());
}

void Settings::setCabrilloDialogGeometry(const QByteArray& geometry)
{
    QJsonObject dialogs = m_settings["dialogs"].toObject();
    dialogs["cabrilloGeometry"] = QString::fromLatin1(geometry.toBase64());
    m_settings["dialogs"] = dialogs;
    m_modified = true;
}

bool Settings::getSsbKeyingEnabled() const
{
    return m_settings["ssbKeying"].toObject()["enabled"].toBool(false);
}

void Settings::setSsbKeyingEnabled(bool enabled)
{
    QJsonObject ssbKeying = m_settings["ssbKeying"].toObject();
    ssbKeying["enabled"] = enabled;
    m_settings["ssbKeying"] = ssbKeying;
    m_modified = true;
}

QString Settings::getTtsCommand() const
{
    QString cmd = m_settings["ssbKeying"].toObject()["ttsCommand"].toString();
    if (cmd.isEmpty()) {
#ifdef Q_OS_MACOS
        return "say";
#else
        return "piper";
#endif
    }
    return cmd;
}

void Settings::setTtsCommand(const QString& command)
{
    QJsonObject ssbKeying = m_settings["ssbKeying"].toObject();
    ssbKeying["ttsCommand"] = command;
    m_settings["ssbKeying"] = ssbKeying;
    m_modified = true;
}

QString Settings::getTtsArgs() const
{
    QString args = m_settings["ssbKeying"].toObject()["ttsArgs"].toString();
    if (args.isEmpty()) {
#ifdef Q_OS_MACOS
        return "";  // say doesn't need args, takes text directly
#else
        return "--model en_US-hfc_male-medium --output_file {OUTPUT}";
#endif
    }
    return args;
}

void Settings::setTtsArgs(const QString& args)
{
    QJsonObject ssbKeying = m_settings["ssbKeying"].toObject();
    ssbKeying["ttsArgs"] = args;
    m_settings["ssbKeying"] = ssbKeying;
    m_modified = true;
}

QString Settings::getAudioPlayCommand() const
{
    QString cmd = m_settings["ssbKeying"].toObject()["audioPlayCommand"].toString();
    if (cmd.isEmpty()) {
#ifdef Q_OS_MACOS
        return "afplay";
#else
        return "paplay";
#endif
    }
    return cmd;
}

void Settings::setAudioPlayCommand(const QString& command)
{
    QJsonObject ssbKeying = m_settings["ssbKeying"].toObject();
    ssbKeying["audioPlayCommand"] = command;
    m_settings["ssbKeying"] = ssbKeying;
    m_modified = true;
}

QString Settings::getAudioPlayArgs() const
{
    QString args = m_settings["ssbKeying"].toObject()["audioPlayArgs"].toString();
    if (args.isEmpty()) {
        return "{FILE}";  // Default for both platforms
    }
    return args;
}

void Settings::setAudioPlayArgs(const QString& args)
{
    QJsonObject ssbKeying = m_settings["ssbKeying"].toObject();
    ssbKeying["audioPlayArgs"] = args;
    m_settings["ssbKeying"] = ssbKeying;
    m_modified = true;
}

QString Settings::getTheme() const
{
    return m_settings["ui"].toObject()["theme"].toString("dark");
}

void Settings::setTheme(const QString& theme)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["theme"] = theme;
    m_settings["ui"] = ui;
    save();
}

bool Settings::getForceX11() const
{
    return m_settings["ui"].toObject()["forceX11"].toBool(true);
}

void Settings::setForceX11(bool enabled)
{
    QJsonObject ui = m_settings["ui"].toObject();
    ui["forceX11"] = enabled;
    m_settings["ui"] = ui;
    save();
}

int Settings::getTermsAcceptedVersion() const
{
    QJsonValue v = m_settings["termsAccepted"];
    if (v.isBool())
        return v.toBool() ? 1 : 0;
    return v.toInt(0);
}

void Settings::setTermsAcceptedVersion(int version)
{
    m_settings["termsAccepted"] = version;
    save();
}

QFont Settings::getPanelFont(const QString& panelKey) const
{
    QJsonObject fontObj = m_settings["fonts"].toObject()[panelKey].toObject();
    if (fontObj.isEmpty())
        return QFont();
    QFont font;
    QString family = fontObj["family"].toString();
    if (!family.isEmpty())
        font.setFamily(family);
    int pointSize = fontObj["pointSize"].toInt(0);
    if (pointSize > 0)
        font.setPointSize(pointSize);
    return font;
}

void Settings::setPanelFont(const QString& panelKey, const QFont& font)
{
    QJsonObject fonts = m_settings["fonts"].toObject();
    QJsonObject fontObj;
    fontObj["family"] = font.family();
    fontObj["pointSize"] = font.pointSize();
    fonts[panelKey] = fontObj;
    m_settings["fonts"] = fonts;
    save();
}

QString Settings::getDataPath()
{
    // AppImage: resolve via APPDIR environment variable
    QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty()) {
        QString appImageData = appDir + "/usr/share/contestlogx/data";
        if (QDir(appImageData).exists()) {
            return QDir(appImageData).absolutePath();
        }
    }

    // macOS app bundle: ContestLogX.app/Contents/Resources/share/contestlogx/data
    QString binDir = QCoreApplication::applicationDirPath();
    QDir bundleData(binDir + "/../Resources/share/contestlogx/data");
    if (bundleData.exists()) {
        return bundleData.absolutePath();
    }

    // Get the invocation path (supports symlinks)
    // argv[0] contains the path used to invoke the program, before symlink resolution
    QStringList args = QCoreApplication::arguments();
    if (args.isEmpty()) {
        // Fallback to standard method if no arguments
        QString binDir = QCoreApplication::applicationDirPath();
        QString dataDir = QDir(binDir).filePath("../data");
        return QDir(dataDir).absolutePath();
    }

    QString invocationPath = args[0];
    QFileInfo invocationInfo(invocationPath);

    // Get the directory where the program was invoked from (symlink location if applicable)
    QString invokeDir;
    if (invocationInfo.isAbsolute()) {
        invokeDir = invocationInfo.absolutePath();
    } else {
        // Relative path - resolve relative to current directory at startup
        QDir currentDir = QDir::current();
        QString absoluteInvokePath = currentDir.absoluteFilePath(invocationPath);
        invokeDir = QFileInfo(absoluteInvokePath).absolutePath();
    }

    // Find data directory relative to invocation directory
    QDir dir(invokeDir);
    QString dataPath = dir.filePath("data");

    // If data doesn't exist at invocation location, try going up one level (for build/bin directories)
    if (!QDir(dataPath).exists()) {
        dir.cdUp();
        dataPath = dir.filePath("data");
    }

    return QDir(dataPath).absolutePath();
}

QString Settings::getContestsPath()
{
    // AppImage: resolve via APPDIR environment variable
    QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty()) {
        QString appImageContests = appDir + "/usr/share/contestlogx/contests";
        if (QDir(appImageContests).exists()) {
            return QDir(appImageContests).absolutePath();
        }
    }

    // macOS app bundle: ContestLogX.app/Contents/Resources/share/contestlogx/contests
    QString binDir = QCoreApplication::applicationDirPath();
    QDir bundleContests(binDir + "/../Resources/share/contestlogx/contests");
    if (bundleContests.exists()) {
        return bundleContests.absolutePath();
    }

    // Development: contests/ in current working directory
    QDir cwdContests("contests");
    if (cwdContests.exists()) {
        return cwdContests.absolutePath();
    }

    // Installed: relative to executable (../share/contestlogx/contests or ../contests)
    QDir appContests(binDir + "/../contests");
    if (appContests.exists()) {
        return appContests.absolutePath();
    }

    // Fallback
    return QDir("contests").absolutePath();
}

QString Settings::getUserDataPath()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

// ---------- Remote Control — embedded HTTP server (TODO item 3) ----------

bool Settings::getRemoteControlEnabled() const
{
    return m_settings["remoteControl"].toObject()["enabled"].toBool(false);
}

void Settings::setRemoteControlEnabled(bool enabled)
{
    QJsonObject rc = m_settings["remoteControl"].toObject();
    rc["enabled"] = enabled;
    m_settings["remoteControl"] = rc;
    save();
}

int Settings::getRemoteControlPort() const
{
    return m_settings["remoteControl"].toObject()["port"].toInt(8080);
}

void Settings::setRemoteControlPort(int port)
{
    QJsonObject rc = m_settings["remoteControl"].toObject();
    rc["port"] = port;
    m_settings["remoteControl"] = rc;
    save();
}

QString Settings::getRemoteControlBindMode() const
{
    return m_settings["remoteControl"].toObject()["bindMode"].toString(QStringLiteral("lan"));
}

void Settings::setRemoteControlBindMode(const QString& mode)
{
    QJsonObject rc = m_settings["remoteControl"].toObject();
    rc["bindMode"] = mode;
    m_settings["remoteControl"] = rc;
    save();
}

QString Settings::getRemoteControlToken() const
{
    return m_settings["remoteControl"].toObject()["token"].toString();
}

void Settings::setRemoteControlToken(const QString& token)
{
    QJsonObject rc = m_settings["remoteControl"].toObject();
    rc["token"] = token;
    m_settings["remoteControl"] = rc;
    save();
}

void Settings::scaleDefaultLayout()
{
    // Get primary screen geometry
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) {
        return;
    }
    
    QRect screenGeometry = screen->availableGeometry();
    
    // Reference dimensions from default layout
    const int referenceWidth = 2239;
    const int referenceHeight = 1033;
    
    // Calculate scale factor based on screen size
    double scaleWidth = static_cast<double>(screenGeometry.width()) / referenceWidth;
    double scaleHeight = static_cast<double>(screenGeometry.height()) / referenceHeight;
    double scale = qMin(scaleWidth, scaleHeight);
    
    // Only scale if screen is significantly smaller
    if (scale < 0.9) {
        QJsonObject window = m_settings["window"].toObject();
        QJsonObject geometry = window["geometry"].toObject();
        
        int newWidth = qMax(800, static_cast<int>(referenceWidth * scale));
        int newHeight = qMax(600, static_cast<int>(referenceHeight * scale));
        
        geometry["width"] = newWidth;
        geometry["height"] = newHeight;
        geometry["x"] = qMax(0, (screenGeometry.width() - newWidth) / 2);
        geometry["y"] = qMax(0, (screenGeometry.height() - newHeight) / 2);
        
        window["geometry"] = geometry;
        m_settings["window"] = window;
    }
}
