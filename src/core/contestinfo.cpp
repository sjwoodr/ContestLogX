/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "contestinfo.h"
#include <QJsonObject>
#include <QJsonArray>

ContestInfo::ContestInfo()
    : m_year(QDateTime::currentDateTime().date().year())
{
}

QJsonObject ContestInfo::toJson() const
{
    QJsonObject json;
    json["name"] = m_name.isEmpty() ? "General DXCC Logging" : m_name;
    json["type"] = m_type;
    json["mode"] = m_mode;
    json["year"] = m_year;
    
    if (m_startTime.isValid())
        json["start_time"] = m_startTime.toString(Qt::ISODate);
    if (m_endTime.isValid())
        json["end_time"] = m_endTime.toString(Qt::ISODate);
    
    if (!m_categories.isEmpty()) {
        QJsonObject categories;
        for (auto it = m_categories.begin(); it != m_categories.end(); ++it) {
            categories[it.key()] = it.value();
        }
        json["categories"] = categories;
    }
    
    if (!m_contestFile.isEmpty()) {
        json["contest_file"] = m_contestFile;
    }
    
    if (!m_contestVersion.isEmpty()) {
        json["version"] = m_contestVersion;
    }
    
    return json;
}

ContestInfo ContestInfo::fromJson(const QJsonObject& json)
{
    ContestInfo info;
    info.m_name = json["name"].toString();
    info.m_type = json["type"].toString();
    info.m_mode = json["mode"].toString();
    info.m_year = json["year"].toInt(QDateTime::currentDateTime().date().year());
    
    if (json.contains("start_time"))
        info.m_startTime = QDateTime::fromString(json["start_time"].toString(), Qt::ISODate);
    if (json.contains("end_time"))
        info.m_endTime = QDateTime::fromString(json["end_time"].toString(), Qt::ISODate);
    
    if (json.contains("categories")) {
        QJsonObject categories = json["categories"].toObject();
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            info.m_categories[it.key()] = it.value().toString();
        }
    }
    
    if (json.contains("contest_file")) {
        info.m_contestFile = json["contest_file"].toString();
    }
    
    if (json.contains("contest_version")) {
        info.m_contestVersion = json["contest_version"].toString();
    } else if (json.contains("version")) {
        info.m_contestVersion = json["version"].toString();
    }
    
    return info;
}
