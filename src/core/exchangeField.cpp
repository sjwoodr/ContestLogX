/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "exchangeField.h"
#include <QJsonArray>

ExchangeField::ExchangeField()
    : m_width(10)
    , m_type(TypeString)
    , m_attributes(AttrNone)
{
}

QJsonObject ExchangeField::toJson() const
{
    QJsonObject json;
    json["name"] = m_name;
    json["label"] = m_label;
    json["width"] = m_width;
    
    // Type
    QString typeStr;
    switch (m_type) {
        case TypeString: typeStr = "string"; break;
        case TypeNumber: typeStr = "number"; break;
        case TypeCallsign: typeStr = "callsign"; break;
        case TypeRST: typeStr = "rst"; break;
        case TypeDate: typeStr = "date"; break;
        case TypeTime: typeStr = "time"; break;
    }
    json["type"] = typeStr;
    
    // Attributes
    QJsonArray attrs;
    if (m_attributes & AttrPrompt) attrs.append("prompt");
    if (m_attributes & AttrCallsign) attrs.append("callsign");
    if (m_attributes & AttrDupeCheck) attrs.append("dupe_check");
    if (m_attributes & AttrMultiplier) attrs.append("multiplier");
    if (m_attributes & AttrRequired) attrs.append("required");
    if (m_attributes & AttrUpperCase) attrs.append("upper_case");
    if (m_attributes & AttrAutoIncrement) attrs.append("auto_increment");
    if (m_attributes & AttrHidden) attrs.append("hidden");
    if (m_attributes & AttrReadOnly) attrs.append("read_only");
    if (!attrs.isEmpty()) json["attributes"] = attrs;
    
    if (!m_defaultValue.isEmpty())
        json["default"] = m_defaultValue;
    
    return json;
}

ExchangeField ExchangeField::fromJson(const QJsonObject& json)
{
    ExchangeField field;
    field.m_name = json["name"].toString();
    field.m_label = json["label"].toString();
    field.m_width = json["width"].toInt(10);
    
    // Type
    QString typeStr = json["type"].toString();
    if (typeStr == "string") field.m_type = TypeString;
    else if (typeStr == "number") field.m_type = TypeNumber;
    else if (typeStr == "callsign") field.m_type = TypeCallsign;
    else if (typeStr == "rst") field.m_type = TypeRST;
    else if (typeStr == "date") field.m_type = TypeDate;
    else if (typeStr == "time") field.m_type = TypeTime;
    
    // Attributes
    if (json.contains("attributes")) {
        QJsonArray attrs = json["attributes"].toArray();
        for (const QJsonValue& v : attrs) {
            QString attr = v.toString();
            if (attr == "prompt") field.m_attributes |= AttrPrompt;
            else if (attr == "callsign") field.m_attributes |= AttrCallsign;
            else if (attr == "dupe_check") field.m_attributes |= AttrDupeCheck;
            else if (attr == "multiplier") field.m_attributes |= AttrMultiplier;
            else if (attr == "required") field.m_attributes |= AttrRequired;
            else if (attr == "upper_case") field.m_attributes |= AttrUpperCase;
            else if (attr == "auto_increment") field.m_attributes |= AttrAutoIncrement;
            else if (attr == "hidden") field.m_attributes |= AttrHidden;
            else if (attr == "read_only") field.m_attributes |= AttrReadOnly;
        }
    }
    
    field.m_defaultValue = json["default"].toString();
    
    return field;
}
