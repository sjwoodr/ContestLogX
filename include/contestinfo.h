/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#ifndef CONTESTINFO_H
#define CONTESTINFO_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>

class ContestInfo
{
public:
    ContestInfo();
    
    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    
    QString type() const { return m_type; }
    void setType(const QString& type) { m_type = type; }
    
    QString mode() const { return m_mode; }
    void setMode(const QString& mode) { m_mode = mode; }
    
    int year() const { return m_year; }
    void setYear(int year) { m_year = year; }
    
    QDateTime startTime() const { return m_startTime; }
    void setStartTime(const QDateTime& time) { m_startTime = time; }
    
    QDateTime endTime() const { return m_endTime; }
    void setEndTime(const QDateTime& time) { m_endTime = time; }
    
    QString category(const QString& key) const { return m_categories.value(key); }
    void setCategory(const QString& key, const QString& value) { m_categories[key] = value; }
    QMap<QString, QString> categories() const { return m_categories; }
    
    QString contestFile() const { return m_contestFile; }
    void setContestFile(const QString& file) { m_contestFile = file; }
    
    QJsonObject toJson() const;
    static ContestInfo fromJson(const QJsonObject& json);
    
private:
    QString m_name;
    QString m_type;
    QString m_mode;
    int m_year;
    QDateTime m_startTime;
    QDateTime m_endTime;
    QMap<QString, QString> m_categories;
    QString m_contestFile;
};

#endif // CONTESTINFO_H
