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

#include "qrzApi.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// https://www.qrz.com/page/current_spec.html

QrzCallsignData::QrzCallsignData()
    : lat(0.0)
    , lon(0.0)
    , dxcc(0)
    , cqzone(0)
    , ituzone(0)
    , eqsl(false)
    , lotw(false)
    , mqsl(false)
{
}

QrzApi::QrzApi(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_userAgent("ContestLogX")
    , m_sessionReply(nullptr)
    , m_callsignReply(nullptr)
{
}

QrzApi::~QrzApi()
{
    if (m_sessionReply)
        m_sessionReply->abort();
    if (m_callsignReply)
        m_callsignReply->abort();
}

void QrzApi::setCredentials(const QString& username, const QString& password)
{
    m_username = username;
    m_password = password;
}

void QrzApi::setUserAgent(const QString& userAgent)
{
    m_userAgent = userAgent;
}

void QrzApi::getSession()
{
    if (m_username.isEmpty() || m_password.isEmpty()) {
        emit sessionError("Username and password must be set before getting a session");
        return;
    }

    // QRZ uses semicolons as parameter separators
    QUrl url(QString("https://xmldata.qrz.com/xml/current/?username=%1;password=%2;agent=%3")
                 .arg(m_username, m_password, m_userAgent));

    if (m_sessionReply)
        m_sessionReply->abort();

    m_sessionReply = m_networkManager->get(QNetworkRequest(url));
    connect(m_sessionReply, &QNetworkReply::finished, this, &QrzApi::onSessionReply);
}

void QrzApi::lookupCallsign(const QString& callsign)
{
    if (m_sessionToken.isEmpty()) {
        emit lookupError("No active QRZ session — configure credentials in Preferences");
        return;
    }

    QUrl url(QString("https://xmldata.qrz.com/xml/current/?s=%1;callsign=%2;agent=%3")
                 .arg(m_sessionToken, callsign.toUpper(), m_userAgent));

    if (m_callsignReply)
        m_callsignReply->abort();

    m_callsignReply = m_networkManager->get(QNetworkRequest(url));
    connect(m_callsignReply, &QNetworkReply::finished, this, &QrzApi::onCallsignReply);
}

bool QrzApi::hasValidSession() const
{
    return !m_sessionToken.isEmpty();
}

bool QrzApi::hasCredentials() const
{
    return !m_username.isEmpty() && !m_password.isEmpty();
}

QString QrzApi::getSessionToken() const
{
    return m_sessionToken;
}

void QrzApi::onSessionReply()
{
    if (!m_sessionReply)
        return;

    if (m_sessionReply->error() != QNetworkReply::NoError) {
        emit sessionError(QString("Network error: %1").arg(m_sessionReply->errorString()));
        m_sessionReply->deleteLater();
        m_sessionReply = nullptr;
        return;
    }

    QString xml = QString::fromUtf8(m_sessionReply->readAll());
    m_sessionReply->deleteLater();
    m_sessionReply = nullptr;

    QString error = extractXmlValue(xml, "Error");
    if (!error.isEmpty()) {
        emit sessionError(error);
        return;
    }

    QString key = extractXmlValue(xml, "Key");
    if (key.isEmpty()) {
        emit sessionError("No session key in response");
        return;
    }

    m_sessionToken = key;
    emit sessionObtained(key);
}

void QrzApi::onCallsignReply()
{
    if (!m_callsignReply)
        return;

    if (m_callsignReply->error() != QNetworkReply::NoError) {
        emit lookupError(QString("Network error: %1").arg(m_callsignReply->errorString()));
        m_callsignReply->deleteLater();
        m_callsignReply = nullptr;
        return;
    }

    QString xml = QString::fromUtf8(m_callsignReply->readAll());
    m_callsignReply->deleteLater();
    m_callsignReply = nullptr;

    // Check for session expiry (error present, no Key)
    QString error = extractXmlValue(xml, "Error");
    if (!error.isEmpty()) {
        if (error.contains("Session Timeout", Qt::CaseInsensitive) ||
            error.contains("Invalid session", Qt::CaseInsensitive)) {
            m_sessionToken.clear();
            emit sessionError(error);
        } else if (error.contains("Not found", Qt::CaseInsensitive)) {
            QString call = error.mid(error.indexOf(':') + 1).trimmed();
            emit callsignNotFound(call.isEmpty() ? "Unknown" : call);
        } else {
            emit lookupError(error);
        }
        return;
    }

    QrzCallsignData data = parseCallsignXml(xml);
    if (!data.call.isEmpty()) {
        emit callsignFound(data);
    } else {
        emit lookupError("Failed to parse callsign data from QRZ response");
    }
}

QrzCallsignData QrzApi::parseCallsignXml(const QString& xml)
{
    QrzCallsignData data;

    data.call    = extractXmlValue(xml, "call");
    data.fname   = extractXmlValue(xml, "fname");
    data.name    = extractXmlValue(xml, "name");
    data.addr1   = extractXmlValue(xml, "addr1");
    data.addr2   = extractXmlValue(xml, "addr2");
    data.state   = extractXmlValue(xml, "state");
    data.zip     = extractXmlValue(xml, "zip");
    data.country = extractXmlValue(xml, "country");
    data.land    = extractXmlValue(xml, "land");
    data.grid    = extractXmlValue(xml, "grid");
    data.iota    = extractXmlValue(xml, "iota");

    QString latStr = extractXmlValue(xml, "lat");
    if (!latStr.isEmpty())
        data.lat = latStr.toDouble();

    QString lonStr = extractXmlValue(xml, "lon");
    if (!lonStr.isEmpty())
        data.lon = lonStr.toDouble();

    QString dxccStr = extractXmlValue(xml, "dxcc");
    if (!dxccStr.isEmpty())
        data.dxcc = dxccStr.toInt();

    QString cqStr = extractXmlValue(xml, "cqzone");
    if (!cqStr.isEmpty())
        data.cqzone = cqStr.toInt();

    QString ituStr = extractXmlValue(xml, "ituzone");
    if (!ituStr.isEmpty())
        data.ituzone = ituStr.toInt();

    data.eqsl = (extractXmlValue(xml, "eqsl") == "1");
    data.lotw = (extractXmlValue(xml, "lotw") == "1");
    data.mqsl = (extractXmlValue(xml, "mqsl") == "1");

    return data;
}

QString QrzApi::extractXmlValue(const QString& xml, const QString& tag)
{
    QString open  = QString("<%1>").arg(tag);
    QString close = QString("</%1>").arg(tag);

    int start = xml.indexOf(open, 0, Qt::CaseInsensitive);
    if (start == -1)
        return {};
    start += open.length();

    int end = xml.indexOf(close, start, Qt::CaseInsensitive);
    if (end == -1)
        return {};

    return xml.mid(start, end - start);
}
