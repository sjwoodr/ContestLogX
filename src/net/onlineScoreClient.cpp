/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "onlineScoreClient.h"
#include "debugLogger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QCoreApplication>

OnlineScoreClient::OnlineScoreClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &OnlineScoreClient::onReplyFinished);
}

void OnlineScoreClient::setCredentials(const QString& callsign, const QString& password)
{
    m_authCallsign = callsign;
    m_authPassword = password;
}

void OnlineScoreClient::postScore(const ScorePostData& data)
{
    if (m_inFlight) {
        DebugLogger::instance().log("OnlineScore", "Post skipped — previous request still in flight");
        return;
    }

    QByteArray xmlBody = buildXml(data);

    DebugLogger::instance().log("OnlineScore",
        QString("Posting score %1 for %2 to %3 (%4 bytes)")
            .arg(data.totalScore).arg(data.callsign).arg(POST_URL).arg(xmlBody.size()));
    DebugLogger::instance().log("OnlineScore", QString("XML:\n%1").arg(QString::fromUtf8(xmlBody)));

    QUrl url(POST_URL);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");
    request.setTransferTimeout(TIMEOUT_MS);

    // Basic Auth
    QString credentials = m_authCallsign + ":" + m_authPassword;
    QByteArray authHeader = "Basic " + credentials.toUtf8().toBase64();
    request.setRawHeader("Authorization", authHeader);

    m_inFlight = true;
    m_networkManager->post(request, xmlBody);
}

QByteArray OnlineScoreClient::buildXml(const ScorePostData& data) const
{
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    w.setAutoFormatting(true);
    w.writeStartDocument();

    w.writeStartElement("dynamicresults");

    w.writeTextElement("contest", data.contestId);
    w.writeTextElement("call", data.callsign);
    w.writeTextElement("ops", data.ops);
    w.writeTextElement("soft", "ContestLogX");
    w.writeTextElement("version", QCoreApplication::applicationVersion());

    // <class> element with attributes
    w.writeStartElement("class");
    w.writeAttribute("power", data.power);
    w.writeAttribute("assisted", data.assisted);
    w.writeAttribute("transmitter", data.transmitter);
    w.writeAttribute("ops", data.opsCategory);
    w.writeAttribute("bands", data.bands);
    w.writeAttribute("mode", data.mode);
    w.writeAttribute("overlay", data.overlay);
    w.writeEndElement(); // class

    w.writeTextElement("club", data.club);

    // <qth>
    w.writeStartElement("qth");
    w.writeTextElement("dxcccountry", data.dxccCountry);
    w.writeTextElement("cqzone", QString::number(data.cqZone));
    w.writeTextElement("iaruzone", QString::number(data.ituZone));
    w.writeTextElement("arrlsection", data.arrlSection);
    w.writeTextElement("stprvoth", data.stPrvOth);
    if (data.grid.length() >= 6)
        w.writeTextElement("grid6", data.grid.left(6));
    else if (data.grid.length() >= 4)
        w.writeTextElement("grid4", data.grid.left(4));
    w.writeEndElement(); // qth

    // <breakdown>
    w.writeStartElement("breakdown");
    for (const ScoreBreakdownEntry& entry : data.breakdown) {
        // QSO count
        w.writeStartElement("qso");
        w.writeAttribute("band", entry.band);
        w.writeAttribute("mode", entry.mode);
        w.writeCharacters(QString::number(entry.qsoCount));
        w.writeEndElement();

        // Multipliers (one element per type)
        for (auto it = entry.mults.constBegin(); it != entry.mults.constEnd(); ++it) {
            w.writeStartElement("mult");
            w.writeAttribute("band", entry.band);
            w.writeAttribute("mode", entry.mode);
            w.writeAttribute("type", it.key());
            w.writeCharacters(QString::number(it.value()));
            w.writeEndElement();
        }

        // Points
        w.writeStartElement("point");
        w.writeAttribute("band", entry.band);
        w.writeAttribute("mode", entry.mode);
        w.writeCharacters(QString::number(entry.points));
        w.writeEndElement();
    }
    w.writeEndElement(); // breakdown

    w.writeTextElement("score", QString::number(data.totalScore));
    w.writeTextElement("timestamp",
        QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"));

    w.writeEndElement(); // dynamicresults
    w.writeEndDocument();

    return xml;
}

void OnlineScoreClient::onReplyFinished(QNetworkReply *reply)
{
    m_inFlight = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Auth failure: 401 or 403
        if (httpStatus == 401 || httpStatus == 403) {
            m_consecutiveAuthFailures++;
            DebugLogger::instance().log("OnlineScore",
                QString("Auth failure #%1: HTTP %2").arg(m_consecutiveAuthFailures).arg(httpStatus));
            if (m_consecutiveAuthFailures >= 3) {
                emit authFailed();
                return;
            }
            emit postFailed(QString("Authentication failed (HTTP %1)").arg(httpStatus));
            return;
        }

        // Network or other error
        DebugLogger::instance().log("OnlineScore",
            QString("Post failed: %1 (HTTP %2)").arg(reply->errorString()).arg(httpStatus));
        emit postFailed(reply->errorString());
        return;
    }

    // Parse JSON response
    QByteArray responseData = reply->readAll();
    DebugLogger::instance().log("OnlineScore",
        QString("Response: %1").arg(QString::fromUtf8(responseData)));

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
        DebugLogger::instance().log("OnlineScore", "Unparseable response — treating as transient error");
        emit postFailed("Unparseable server response");
        return;
    }

    QJsonObject obj = doc.object();
    int status = obj["status"].toInt();
    QString message = obj["status_message"].toString();

    if (status == 200) {
        m_consecutiveAuthFailures = 0;
        QString timestamp = QDateTime::currentDateTimeUtc().toString("HH:mm");
        DebugLogger::instance().log("OnlineScore",
            QString("Score posted successfully at %1 UTC").arg(timestamp));
        emit postSuccess(timestamp);
    } else {
        DebugLogger::instance().log("OnlineScore",
            QString("Server rejected post: %1 - %2").arg(status).arg(message));
        emit postFailed(message);
    }
}
