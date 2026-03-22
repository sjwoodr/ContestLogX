/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef STATIONINFO_H
#define STATIONINFO_H

#include <QString>
#include <QJsonObject>
#include <QMap>

class StationInfo
{
public:
    StationInfo();
    
    QString callsign() const { return m_callsign; }
    void setCallsign(const QString& call) { m_callsign = call; }
    
    QString operatorName() const { return m_operator; }
    void setOperatorName(const QString& name) { m_operator = name; }
    
    QString grid() const { return m_grid; }
    void setGrid(const QString& grid) { m_grid = grid; }
    
    QString state() const { return m_state; }
    void setState(const QString& state) { m_state = state; }
    
    QString county() const { return m_county; }
    void setCounty(const QString& county) { m_county = county; }
    
    int cqZone() const { return m_cqZone; }
    void setCqZone(int zone) { m_cqZone = zone; }
    
    int ituZone() const { return m_ituZone; }
    void setItuZone(int zone) { m_ituZone = zone; }
    
    QString rig() const { return m_rig; }
    void setRig(const QString& rig) { m_rig = rig; }
    
    QString antenna() const { return m_antenna; }
    void setAntenna(const QString& antenna) { m_antenna = antenna; }
    
    int power() const { return m_power; }
    void setPower(int power) { m_power = power; }
    
    QJsonObject toJson() const;
    static StationInfo fromJson(const QJsonObject& json);
    
private:
    QString m_callsign;
    QString m_operator;
    QString m_grid;
    QString m_state;
    QString m_county;
    int m_cqZone;
    int m_ituZone;
    QString m_rig;
    QString m_antenna;
    int m_power;
};

#endif // STATIONINFO_H
