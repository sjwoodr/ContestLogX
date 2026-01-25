#include "qrzcqapi.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QDebug>

QrzcqCallsignData::QrzcqCallsignData()
    : latitude(0.0)
    , longitude(0.0)
    , dxcc(0)
    , itu(0)
    , cq(0)
    , eqsl(false)
    , lotw(false)
    , bqsl(false)
    , mqsl(false)
    , utf8(false)
{
}

QrzcqApi::QrzcqApi(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_sessionReply(nullptr)
    , m_callsignReply(nullptr)
{
    m_userAgent = "ContestLogX";
}

QrzcqApi::~QrzcqApi()
{
    if (m_sessionReply) {
        m_sessionReply->abort();
    }
    if (m_callsignReply) {
        m_callsignReply->abort();
    }
}

void QrzcqApi::setCredentials(const QString &username, const QString &password)
{
    m_username = username;
    m_password = password;
}

void QrzcqApi::setUserAgent(const QString &userAgent)
{
    m_userAgent = userAgent;
}

void QrzcqApi::getSession()
{
    if (m_username.isEmpty() || m_password.isEmpty()) {
        emit sessionError("Username and password must be set before getting a session");
        return;
    }

    // Create request to get session token
    QUrl url("https://ssl.qrzcq.com/xml");
    QUrlQuery query;
    query.addQueryItem("username", m_username);
    query.addQueryItem("password", m_password);
    query.addQueryItem("agent", m_userAgent);
    url.setQuery(query);

    QNetworkRequest request(url);
    
    // Abort previous session request if any
    if (m_sessionReply) {
        m_sessionReply->abort();
    }

    m_sessionReply = m_networkManager->get(request);
    connect(m_sessionReply, &QNetworkReply::finished, this, &QrzcqApi::onSessionReply);
}

void QrzcqApi::lookupCallsign(const QString &callsign)
{
    if (m_sessionToken.isEmpty()) {
        emit lookupError("No valid session token. Call getSession() first.");
        return;
    }

    // Create request for callsign lookup
    QUrl url("https://ssl.qrzcq.com/xml");
    QUrlQuery query;
    query.addQueryItem("s", m_sessionToken);
    query.addQueryItem("callsign", callsign.toUpper());
    query.addQueryItem("agent", m_userAgent);
    url.setQuery(query);

    QNetworkRequest request(url);

    // Abort previous callsign request if any
    if (m_callsignReply) {
        m_callsignReply->abort();
    }

    m_callsignReply = m_networkManager->get(request);
    connect(m_callsignReply, &QNetworkReply::finished, this, &QrzcqApi::onCallsignReply);
}

bool QrzcqApi::hasValidSession() const
{
    return !m_sessionToken.isEmpty();
}

QString QrzcqApi::getSessionToken() const
{
    return m_sessionToken;
}

void QrzcqApi::onSessionReply()
{
    if (!m_sessionReply) {
        return;
    }

    if (m_sessionReply->error() != QNetworkReply::NoError) {
        emit sessionError(QString("Network error: %1").arg(m_sessionReply->errorString()));
        m_sessionReply->deleteLater();
        m_sessionReply = nullptr;
        return;
    }

    QString responseData = QString::fromUtf8(m_sessionReply->readAll());
    m_sessionReply->deleteLater();
    m_sessionReply = nullptr;

    // Check for error in response
    if (responseData.contains("<Error>", Qt::CaseInsensitive)) {
        QString error = extractXmlValue(responseData, "Error");
        emit sessionError(error);
        return;
    }

    // Parse session token from response
    QString token = extractXmlValue(responseData, "session");
    if (token.isEmpty()) {
        emit sessionError("No session token in response");
        return;
    }

    m_sessionToken = token;
    emit sessionObtained(token);
}

void QrzcqApi::onCallsignReply()
{
    if (!m_callsignReply) {
        return;
    }

    if (m_callsignReply->error() != QNetworkReply::NoError) {
        emit lookupError(QString("Network error: %1").arg(m_callsignReply->errorString()));
        m_callsignReply->deleteLater();
        m_callsignReply = nullptr;
        return;
    }

    QString responseData = QString::fromUtf8(m_callsignReply->readAll());
    m_callsignReply->deleteLater();
    m_callsignReply = nullptr;

    // Check for error in response
    if (responseData.contains("<Error>", Qt::CaseInsensitive)) {
        QString error = extractXmlValue(responseData, "Error");
        // Extract callsign from error message if "Not found: CALLSIGN" format
        if (error.contains("Not found:")) {
            QString callsign = error.mid(error.indexOf(":") + 1).trimmed();
            emit callsignNotFound(callsign);
        } else {
            emit lookupError(error);
        }
        return;
    }

    // Parse callsign data
    QrzcqCallsignData data = parseCallsignXml(responseData);
    if (!data.call.isEmpty()) {
        emit callsignFound(data);
    } else {
        emit lookupError("Failed to parse callsign data from response");
    }
}

QrzcqCallsignData QrzcqApi::parseCallsignXml(const QString &xmlData)
{
    QrzcqCallsignData data;

    data.call = extractXmlValue(xmlData, "call");
    data.name = extractXmlValue(xmlData, "name");
    data.qth = extractXmlValue(xmlData, "qth");
    data.address = extractXmlValue(xmlData, "address");
    data.city = extractXmlValue(xmlData, "city");
    data.zip = extractXmlValue(xmlData, "zip");
    data.license = extractXmlValue(xmlData, "license");
    data.continent = extractXmlValue(xmlData, "continent");
    data.country = extractXmlValue(xmlData, "country");
    data.state = extractXmlValue(xmlData, "state");
    data.county = extractXmlValue(xmlData, "county");
    data.locator = extractXmlValue(xmlData, "locator");
    data.website = extractXmlValue(xmlData, "website");
    data.iota = extractXmlValue(xmlData, "iota");
    data.prefix = extractXmlValue(xmlData, "prefix");
    data.qslpic = extractXmlValue(xmlData, "qslpic");

    // Parse numeric values
    QString latStr = extractXmlValue(xmlData, "latitude");
    if (!latStr.isEmpty()) {
        data.latitude = latStr.toDouble();
    }

    QString lonStr = extractXmlValue(xmlData, "longitude");
    if (!lonStr.isEmpty()) {
        data.longitude = lonStr.toDouble();
    }

    QString dxccStr = extractXmlValue(xmlData, "dxcc");
    if (!dxccStr.isEmpty()) {
        data.dxcc = dxccStr.toInt();
    }

    QString ituStr = extractXmlValue(xmlData, "itu");
    if (!ituStr.isEmpty()) {
        data.itu = ituStr.toInt();
    }

    QString cqStr = extractXmlValue(xmlData, "cq");
    if (!cqStr.isEmpty()) {
        data.cq = cqStr.toInt();
    }

    // Parse boolean values
    QString eqslStr = extractXmlValue(xmlData, "eqsl");
    data.eqsl = (eqslStr == "1");

    QString lotwStr = extractXmlValue(xmlData, "lotw");
    data.lotw = (lotwStr == "1");

    QString bqslStr = extractXmlValue(xmlData, "bqsl");
    data.bqsl = (bqslStr == "1");

    QString mqslStr = extractXmlValue(xmlData, "mqsl");
    data.mqsl = (mqslStr == "1");

    QString utf8Str = extractXmlValue(xmlData, "utf8");
    data.utf8 = (utf8Str == "1");

    return data;
}

QString QrzcqApi::parseSessionXml(const QString &xmlData)
{
    return extractXmlValue(xmlData, "session");
}

QString QrzcqApi::extractXmlValue(const QString &xmlData, const QString &tag)
{
    QString openTag = QString("<%1>").arg(tag);
    QString closeTag = QString("</%1>").arg(tag);

    int startIdx = xmlData.indexOf(openTag, 0, Qt::CaseInsensitive);
    if (startIdx == -1) {
        return QString();
    }

    startIdx += openTag.length();
    int endIdx = xmlData.indexOf(closeTag, startIdx, Qt::CaseInsensitive);
    if (endIdx == -1) {
        return QString();
    }

    return xmlData.mid(startIdx, endIdx - startIdx);
}
