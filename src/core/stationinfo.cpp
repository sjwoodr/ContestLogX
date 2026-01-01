/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "stationinfo.h"

StationInfo::StationInfo()
    : m_cqZone(0)
    , m_ituZone(0)
    , m_power(100)
{
}

QJsonObject StationInfo::toJson() const
{
    QJsonObject json;
    json["callsign"] = m_callsign;
    json["operator"] = m_operator;
    
    QJsonObject location;
    if (!m_grid.isEmpty()) location["grid"] = m_grid;
    if (!m_state.isEmpty()) location["state"] = m_state;
    if (!m_county.isEmpty()) location["county"] = m_county;
    if (m_cqZone > 0) location["cq_zone"] = m_cqZone;
    if (m_ituZone > 0) location["itu_zone"] = m_ituZone;
    if (!location.isEmpty()) json["location"] = location;
    
    QJsonObject equipment;
    if (!m_rig.isEmpty()) equipment["rig"] = m_rig;
    if (!m_antenna.isEmpty()) equipment["antenna"] = m_antenna;
    if (m_power > 0) equipment["power"] = m_power;
    if (!equipment.isEmpty()) json["equipment"] = equipment;
    
    return json;
}

StationInfo StationInfo::fromJson(const QJsonObject& json)
{
    StationInfo info;
    info.m_callsign = json["callsign"].toString();
    info.m_operator = json["operator"].toString();
    
    if (json.contains("location")) {
        QJsonObject location = json["location"].toObject();
        info.m_grid = location["grid"].toString();
        info.m_state = location["state"].toString();
        info.m_county = location["county"].toString();
        info.m_cqZone = location["cq_zone"].toInt();
        info.m_ituZone = location["itu_zone"].toInt();
    }
    
    if (json.contains("equipment")) {
        QJsonObject equipment = json["equipment"].toObject();
        info.m_rig = equipment["rig"].toString();
        info.m_antenna = equipment["antenna"].toString();
        info.m_power = equipment["power"].toInt(100);
    }
    
    return info;
}
