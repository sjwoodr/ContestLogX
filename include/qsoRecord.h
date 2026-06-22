/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef QSORECORD_H
#define QSORECORD_H

#include <QString>
#include <QDateTime>
#include <QMap>

/**
 * @brief QSO Record data structure
 * 
 * This class provides a Qt-friendly interface for QSO data.
 */
class QsoRecord
{
public:
    QsoRecord();
    ~QsoRecord();

    // Accessors
    QString getCall() const;
    QDateTime getDateTime() const;
    QString getFrequency() const;
    QString getRxFrequency() const;
    QString getMode() const;
    QString getBand() const;
    QString getExchange() const;
    unsigned long getSerial() const;
    bool isDupe() const;
    
    // Exchange field accessors
    QString getRstSent() const;
    QString getRstReceived() const;
    QString getExchangeSent() const;
    QString getExchangeReceived() const;
    QMap<QString, QString> getExchangeFields() const { return m_exchangeFields; }
    QString getExchangeField(const QString& key) const;
    
    // Multiplier and scoring
    int getMultiplierCount() const { return m_multiplierCount; }
    int getDxccCount() const { return m_dxccCount; }
    int getItuRegionCount() const { return m_ituRegionCount; }
    int getGridSquareMultiplierCount() const { return m_gridSquareMultCount; }
    int getPoints() const { return m_points; }
    QString getComment() const { return m_comment; }
    bool isOutOfBand() const { return m_outOfBand; }
    QString getGridSquareMultiplier() const { return m_gridSquareMult; }

    // Mutators
    void setCall(const QString& call);
    void setDateTime(const QDateTime& dt);
    void setFrequency(const QString& freq);
    void setRxFrequency(const QString& rxFreq);
    void setMode(const QString& mode);
    void setBand(unsigned char band);
    void setBandName(const QString& band);
    void setExchange(const QString& exchange);
    void setSerial(unsigned long serial);
    void setDupe(bool dupe);
    
    // Exchange field mutators
    void setRstSent(const QString& rst);
    void setRstReceived(const QString& rst);
    void setExchangeSent(const QString& exch);
    void setExchangeReceived(const QString& exch);
    void setExchangeField(const QString& key, const QString& value);
    void setExchangeFields(const QMap<QString, QString>& fields);
    
    // Multiplier and scoring
    void setMultiplierCount(int count) { m_multiplierCount = count; }
    void setDxccCount(int count) { m_dxccCount = count; }
    void setItuRegionCount(int count) { m_ituRegionCount = count; }
    void setGridSquareMultiplierCount(int count) { m_gridSquareMultCount = count; }
    void setPoints(int points) { m_points = points; }
    void setComment(const QString& comment) { m_comment = comment; }
    void setOutOfBand(bool outOfBand) { m_outOfBand = outOfBand; }
    void setGridSquareMultiplier(const QString& grid) { m_gridSquareMult = grid; }

    // Station info - lookup-derived facts about the worked station,
    // keyed by standard ADIF field names (NAME, QTH, GRIDSQUARE, COUNTRY,
    // DXCC, CONT, CQZ, ITUZ).  Not shown as log columns but saved to CLX
    // and exported to ADIF.
    QString getStationInfo(const QString& key) const { return m_stationInfo.value(key); }
    void setStationInfo(const QString& key, const QString& value) { if (!value.isEmpty()) m_stationInfo[key] = value; }
    const QMap<QString, QString>& getStationInfoMap() const { return m_stationInfo; }
    void setStationInfoMap(const QMap<QString, QString>& info) { m_stationInfo = info; }

    // Validation
    bool isValid() const;
    QString validationError() const;

private:
    QString m_call;
    QDateTime m_dateTime;
    QString m_frequency;
    QString m_rxFrequency;
    QString m_mode;
    QString m_band;
    QString m_exchange;  // Cached exchange string
    unsigned long m_serial = 0;
    bool m_dupe = false;
    bool m_outOfBand = false;
    QMap<QString, QString> m_exchangeFields;  // Individual exchange fields
    QMap<QString, QString> m_stationInfo;    // Lookup-derived station metadata (ADIF-keyed)
    int m_multiplierCount = 0;
    int m_dxccCount = 0;
    int m_ituRegionCount = 0;
    int m_gridSquareMultCount = 0;
    int m_points = 0;
    QString m_comment;
    QString m_gridSquareMult;
};

#endif // QSORECORD_H
