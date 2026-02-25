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

#include "clxFile.h"
#include "cwMemory.h"
#include "ssbMemory.h"
#include "../utils/bandPlan.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

ClxFile::ClxFile()
    : m_version("1.0")
    , m_created(QDateTime::currentDateTime())
    , m_modified(QDateTime::currentDateTime())
{
}

bool ClxFile::load(const QString& filename)
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

bool ClxFile::save(const QString& filename)
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

bool ClxFile::loadJson(const QJsonObject& json)
{
    QString format = json["format"].toString();
    
    if (format != "ContestLogX") {
        m_lastError = "Invalid format: " + format;
        return false;
    }
    
    // Timestamps
    m_created = QDateTime::fromString(json["created"].toString(), Qt::ISODate);
    m_modified = QDateTime::fromString(json["modified"].toString(), Qt::ISODate);
    
    // Load format version (for CLX file structure compatibility)
    if (json.contains("format_version")) {
        m_version = json["format_version"].toString("1.0");
    } else if (json.contains("version")) {
        // Backwards compatibility: old files had version at top level
        m_version = json["version"].toString("1.0");
    } else {
        m_version = "1.0";
    }
    
    // Contest
    if (json.contains("contest")) {
        QJsonObject contestJson = json["contest"].toObject();
        m_contest = ContestInfo::fromJson(contestJson);
        // Contest version is preserved in contestJson and loaded by ContestInfo::fromJson
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
            
            qso.setDateTime(QDateTime::fromString(qsoJson["timestamp"].toString(), Qt::ISODate).toUTC());
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
            qso.setGridSquareMultiplierCount(qsoJson["grid_square_count"].toInt());
            
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

            // Load station info (lookup-derived metadata)
            if (qsoJson.contains("station_info")) {
                QJsonObject stInfo = qsoJson["station_info"].toObject();
                for (auto it = stInfo.begin(); it != stInfo.end(); ++it)
                    qso.setStationInfo(it.key(), it.value().toString());
            }

            m_qsos.append(qso);
        }
    }
    
    // Contest-specific memories
    if (json.contains("memory_mode") && json["memory_mode"].toString() == "contest") {
        m_useContestMemories = true;
    }

    if (json.contains("cw_memories")) {
        QJsonArray cwArr = json["cw_memories"].toArray();
        for (const QJsonValue& v : cwArr) {
            QJsonObject obj = v.toObject();
            CwMemory mem;
            mem.abbreviation = obj["abbreviation"].toString();
            mem.text = obj["text"].toString();
            m_cwMemories.append(mem);
        }
    }

    if (json.contains("ssb_memories")) {
        QJsonArray ssbArr = json["ssb_memories"].toArray();
        for (const QJsonValue& v : ssbArr) {
            QJsonObject obj = v.toObject();
            SsbMemory mem;
            mem.abbreviation = obj["abbreviation"].toString();
            mem.text = obj["text"].toString();
            m_ssbMemories.append(mem);
        }
    }

    return true;
}

QJsonObject ClxFile::toJson() const
{
    QJsonObject json;
    
    // Metadata
    json["format"] = "ContestLogX";
    json["format_version"] = m_version;
    json["created"] = m_created.toString(Qt::ISODate);
    json["modified"] = m_modified.toString(Qt::ISODate);
    
    // Contest & Station
    QJsonObject contest = m_contest.toJson();
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
        qsoJson["timestamp"] = qso.getDateTime().toUTC().toString(Qt::ISODate);
        double freqKhz = qso.getFrequency().toDouble();
        qsoJson["frequency"] = freqKhz;
        QString band = qso.getBand();
        if (band.isEmpty() && freqKhz > 0)
            band = BandPlan::freq2Band(freqKhz);
        qsoJson["band"] = band;
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
        qsoJson["grid_square_count"] = qso.getGridSquareMultiplierCount();
        
        // Comment
        if (!qso.getComment().isEmpty()) {
            qsoJson["comment"] = qso.getComment();
        }

        // Station info (lookup-derived metadata, ADIF-keyed)
        const QMap<QString, QString>& stInfo = qso.getStationInfoMap();
        if (!stInfo.isEmpty()) {
            QJsonObject stInfoJson;
            for (auto it = stInfo.constBegin(); it != stInfo.constEnd(); ++it)
                stInfoJson[it.key()] = it.value();
            qsoJson["station_info"] = stInfoJson;
        }
        
        qsos.append(qsoJson);
    }
    json["qsos"] = qsos;
    
    // Contest-specific memories
    if (m_useContestMemories) {
        json["memory_mode"] = "contest";

        QJsonArray cwArr;
        for (const CwMemory& mem : m_cwMemories) {
            QJsonObject obj;
            obj["abbreviation"] = mem.abbreviation;
            obj["text"] = mem.text;
            cwArr.append(obj);
        }
        json["cw_memories"] = cwArr;

        QJsonArray ssbArr;
        for (const SsbMemory& mem : m_ssbMemories) {
            QJsonObject obj;
            obj["abbreviation"] = mem.abbreviation;
            obj["text"] = mem.text;
            ssbArr.append(obj);
        }
        json["ssb_memories"] = ssbArr;
    }
    
    // Statistics
    QJsonObject stats;
    stats["total_qsos"] = m_qsos.count();
    stats["total_points"] = totalPoints();
    stats["score"] = score();
    json["statistics"] = stats;
    
    return json;
}

int ClxFile::totalQsos() const
{
    return m_qsos.count();
}

int ClxFile::totalPoints() const
{
    // TODO: Implement contest-specific scoring
    return m_qsos.count();
}

int ClxFile::totalMultipliers() const
{
    // TODO: Implement multiplier tracking
    return 0;
}

int ClxFile::score() const
{
    return totalPoints() * (totalMultipliers() > 0 ? totalMultipliers() : 1);
}
