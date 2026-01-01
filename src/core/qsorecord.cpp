/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "qsorecord.h"
#include <QDebug>

QsoRecord::QsoRecord()
    : m_serial(0), m_dupe(false), m_multiplierCount(0), m_dxccCount(0), m_points(0)
{
    m_mode = "USB";  // Default to USB
}

QsoRecord::~QsoRecord()
{
}

QString QsoRecord::getCall() const
{
    return m_call;
}

QDateTime QsoRecord::getDateTime() const
{
    return m_dateTime;
}

QString QsoRecord::getFrequency() const
{
    return m_frequency;
}

QString QsoRecord::getRxFrequency() const
{
    return m_rxFrequency;
}

QString QsoRecord::getMode() const
{
    return m_mode;
}

QString QsoRecord::getBand() const
{
    return m_band;
}

QString QsoRecord::getExchange() const
{
    return m_exchange;
}

unsigned long QsoRecord::getSerial() const
{
    return m_serial;
}

bool QsoRecord::isDupe() const
{
    return m_dupe;
}

QString QsoRecord::getRstSent() const
{
    return m_exchangeFields.value("RSTs", "");
}

QString QsoRecord::getRstReceived() const
{
    return m_exchangeFields.value("RSTr", "");
}

QString QsoRecord::getExchangeSent() const
{
    return m_exchangeFields.value("EXCHs", "");
}

QString QsoRecord::getExchangeReceived() const
{
    return m_exchangeFields.value("EXCHr", "");
}

QString QsoRecord::getExchangeField(const QString& key) const
{
    return m_exchangeFields.value(key, "");
}

void QsoRecord::setCall(const QString& call)
{
    m_call = call.toUpper();
}

void QsoRecord::setDateTime(const QDateTime& dt)
{
    m_dateTime = dt;
}

void QsoRecord::setFrequency(const QString& freq)
{
    m_frequency = freq;
}

void QsoRecord::setRxFrequency(const QString& rxFreq)
{
    m_rxFrequency = rxFreq;
}

void QsoRecord::setMode(const QString& mode)
{
    m_mode = mode;
}

void QsoRecord::setBand(unsigned char band)
{
    // Map band index to string
    const char* bands[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
    if (band < 11) {
        m_band = bands[band];
    }
}

void QsoRecord::setBandName(const QString& band)
{
    m_band = band;
}

void QsoRecord::setExchange(const QString& exchange)
{
    m_exchange = exchange;
}

void QsoRecord::setSerial(unsigned long serial)
{
    m_serial = serial;
}

void QsoRecord::setDupe(bool dupe)
{
    m_dupe = dupe;
}

void QsoRecord::setRstSent(const QString& rst)
{
    m_exchangeFields["RSTs"] = rst;
}

void QsoRecord::setRstReceived(const QString& rst)
{
    m_exchangeFields["RSTr"] = rst;
}

void QsoRecord::setExchangeSent(const QString& exch)
{
    m_exchangeFields["EXCHs"] = exch;
}

void QsoRecord::setExchangeReceived(const QString& exch)
{
    m_exchangeFields["EXCHr"] = exch;
}

void QsoRecord::setExchangeField(const QString& key, const QString& value)
{
    m_exchangeFields[key] = value;
}

void QsoRecord::setExchangeFields(const QMap<QString, QString>& fields)
{
    m_exchangeFields = fields;
}

bool QsoRecord::isValid() const
{
    return !m_call.isEmpty() && m_dateTime.isValid();
}

QString QsoRecord::validationError() const
{
    if (m_call.isEmpty()) {
        return "Callsign is required";
    }
    if (!m_dateTime.isValid()) {
        return "Valid date/time is required";
    }
    return QString();
}
