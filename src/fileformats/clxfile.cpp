/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "clxfile.h"
#include "../utils/bandplan.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

Wl2File::Wl2File()
    : m_version("1.0")
    , m_created(QDateTime::currentDateTime())
    , m_modified(QDateTime::currentDateTime())
{
}

bool Wl2File::load(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = "Cannot open file: " + file.errorString();
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        m_lastError = "JSON parse error: " + error.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        m_lastError = "JSON document is not an object";
        return false;
    }
    
    return loadJson(doc.object());
}

bool Wl2File::save(const QString& filename)
{
    m_modified = QDateTime::currentDateTime();
    
    QJsonObject json = toJson();
    QJsonDocument doc(json);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = "Cannot create file: " + file.errorString();
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

bool Wl2File::loadJson(const QJsonObject& json)
{
    QString format = json["format"].toString();
    
    if (format != "ContestLogX") {
        m_lastError = "Invalid format: " + format;
        return false;
    }
    
    // Timestamps
    m_created = QDateTime::fromString(json["created"].toString(), Qt::ISODate);
    m_modified = QDateTime::fromString(json["modified"].toString(), Qt::ISODate);
    
    // Contest
    if (json.contains("contest")) {
        QJsonObject contestJson = json["contest"].toObject();
        m_contest = ContestInfo::fromJson(contestJson);
        // Check version in contest object (new format) or at top level (old format for backwards compat)
        if (contestJson.contains("version")) {
            m_version = contestJson["version"].toString("1.0");
        } else if (json.contains("version")) {
            m_version = json["version"].toString("1.0");
        } else {
            m_version = "1.0";
        }
    }
    
    // Station
    if (json.contains("station")) {
        m_station = StationInfo::fromJson(json["station"].toObject());
    }
    
    // Exchange fields
    if (json.contains("exchange_fields")) {
        QJsonArray fields = json["exchange_fields"].toArray();
        for (const QJsonValue& v : fields) {
            m_exchangeFields.append(ExchangeField::fromJson(v.toObject()));
        }
    }
    
    // QSOs
    if (json.contains("qsos")) {
        QJsonArray qsos = json["qsos"].toArray();
        for (const QJsonValue& v : qsos) {
            QJsonObject qsoJson = v.toObject();
            QsoRecord qso;
            
            qso.setDateTime(QDateTime::fromString(qsoJson["timestamp"].toString(), Qt::ISODate));
            double freqKhz = qsoJson["frequency"].toDouble();
            qso.setFrequency(QString::number(freqKhz));
            
            // Calculate band from frequency if not in file
            QString band = qsoJson["band"].toString();
            if (band.isEmpty()) {
                band = BandPlan::freq2Band(freqKhz);
                if (!band.isEmpty()) {
                    qso.setBandName(band);
                }
            } else {
                qso.setBandName(band);
            }
            
            qso.setMode(qsoJson["mode"].toString());
            qso.setCall(qsoJson["callsign"].toString());
            qso.setDupe(qsoJson["duplicate"].toBool());
            qso.setSerial(qsoJson["serial"].toInt());
            qso.setPoints(qsoJson["points"].toInt());
            qso.setMultiplierCount(qsoJson["multiplier_count"].toInt());
            qso.setDxccCount(qsoJson["dxcc_count"].toInt());
            
            // RST fields
            if (qsoJson.contains("rst_sent")) {
                qso.setRstSent(qsoJson["rst_sent"].toString());
            }
            if (qsoJson.contains("rst_received")) {
                qso.setRstReceived(qsoJson["rst_received"].toString());
            }
            
            // Load all exchange fields
            if (qsoJson.contains("exchange_fields")) {
                QJsonObject exchFields = qsoJson["exchange_fields"].toObject();
                for (auto it = exchFields.begin(); it != exchFields.end(); ++it) {
                    qso.setExchangeField(it.key(), it.value().toString());
                }
            }
            
            m_qsos.append(qso);
        }
    }
    
    // CW Messages
    if (json.contains("cw_messages")) {
        QJsonObject cw = json["cw_messages"].toObject();
        for (auto it = cw.begin(); it != cw.end(); ++it) {
            m_cwMessages[it.key()] = it.value().toString();
        }
    }
    
    return true;
}

QJsonObject Wl2File::toJson() const
{
    QJsonObject json;
    
    // Metadata
    json["format"] = "ContestLogX";
    json["created"] = m_created.toString(Qt::ISODate);
    json["modified"] = m_modified.toString(Qt::ISODate);
    
    // Contest & Station
    QJsonObject contest = m_contest.toJson();
    contest["version"] = m_version;
    json["contest"] = contest;
    json["station"] = m_station.toJson();
    
    // Exchange fields
    QJsonArray fields;
    for (const ExchangeField& field : m_exchangeFields) {
        fields.append(field.toJson());
    }
    json["exchange_fields"] = fields;
    
    // QSOs
    QJsonArray qsos;
    int id = 1;
    for (const QsoRecord& qso : m_qsos) {
        QJsonObject qsoJson;
        qsoJson["id"] = id++;
        qsoJson["serial"] = (int)qso.getSerial();
        qsoJson["timestamp"] = qso.getDateTime().toString(Qt::ISODate);
        qsoJson["frequency"] = qso.getFrequency().toDouble();
        qsoJson["band"] = qso.getBand();
        qsoJson["mode"] = qso.getMode();
        qsoJson["callsign"] = qso.getCall();
        qsoJson["duplicate"] = qso.isDupe();
        
        // RST fields
        qsoJson["rst_sent"] = qso.getRstSent();
        qsoJson["rst_received"] = qso.getRstReceived();
        
        // All exchange fields as object
        QJsonObject exchFields;
        QMap<QString, QString> fields = qso.getExchangeFields();
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            exchFields[it.key()] = it.value();
        }
        if (!exchFields.isEmpty()) {
            qsoJson["exchange_fields"] = exchFields;
        }
        
        // Points, Multiplier and DXCC counts
        qsoJson["points"] = qso.getPoints();
        qsoJson["multiplier_count"] = qso.getMultiplierCount();
        qsoJson["dxcc_count"] = qso.getDxccCount();
        
        // Comment
        if (!qso.getComment().isEmpty()) {
            qsoJson["comment"] = qso.getComment();
        }
        
        qsos.append(qsoJson);
    }
    json["qsos"] = qsos;
    
    // CW Messages
    if (!m_cwMessages.isEmpty()) {
        QJsonObject cw;
        for (auto it = m_cwMessages.begin(); it != m_cwMessages.end(); ++it) {
            cw[it.key()] = it.value();
        }
        json["cw_messages"] = cw;
    }
    
    // Statistics
    QJsonObject stats;
    stats["total_qsos"] = m_qsos.count();
    stats["total_points"] = totalPoints();
    stats["score"] = score();
    json["statistics"] = stats;
    
    return json;
}

int Wl2File::totalQsos() const
{
    return m_qsos.count();
}

int Wl2File::totalPoints() const
{
    // TODO: Implement contest-specific scoring
    return m_qsos.count();
}

int Wl2File::totalMultipliers() const
{
    // TODO: Implement multiplier tracking
    return 0;
}

int Wl2File::score() const
{
    return totalPoints() * (totalMultipliers() > 0 ? totalMultipliers() : 1);
}
