/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef EXCHANGEFIELD_H
#define EXCHANGEFIELD_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>

class ExchangeField
{
public:
    enum FieldType {
        TypeString,
        TypeNumber,
        TypeCallsign,
        TypeRST,
        TypeDate,
        TypeTime
    };
    
    enum Attribute {
        AttrNone = 0,
        AttrPrompt = 0x01,          // Show in entry form
        AttrCallsign = 0x02,        // This is the callsign field
        AttrDupeCheck = 0x04,       // Use in dupe checking
        AttrMultiplier = 0x08,      // This is a multiplier
        AttrRequired = 0x10,        // Required field
        AttrUpperCase = 0x20,       // Force uppercase
        AttrAutoIncrement = 0x40,   // Auto-increment (serial)
        AttrHidden = 0x80,          // Don't display
        AttrReadOnly = 0x100        // Can't edit
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)
    
    ExchangeField();
    
    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    
    QString label() const { return m_label; }
    void setLabel(const QString& label) { m_label = label; }
    
    int width() const { return m_width; }
    void setWidth(int width) { m_width = width; }
    
    FieldType type() const { return m_type; }
    void setType(FieldType type) { m_type = type; }
    
    Attributes attributes() const { return m_attributes; }
    void setAttributes(Attributes attrs) { m_attributes = attrs; }
    void addAttribute(Attribute attr) { m_attributes |= attr; }
    bool hasAttribute(Attribute attr) const { return m_attributes & attr; }
    
    QString defaultValue() const { return m_defaultValue; }
    void setDefaultValue(const QString& value) { m_defaultValue = value; }
    
    QJsonObject toJson() const;
    static ExchangeField fromJson(const QJsonObject& json);
    
private:
    QString m_name;
    QString m_label;
    int m_width;
    FieldType m_type;
    Attributes m_attributes;
    QString m_defaultValue;
    QMap<QString, QString> m_validation;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ExchangeField::Attributes)

#endif // EXCHANGEFIELD_H
