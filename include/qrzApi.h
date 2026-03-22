/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef QRZAPI_H
#define QRZAPI_H

#include <QString>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Field names match the QRZ XML API spec (https://www.qrz.com/page/current_spec.html)
class QrzCallsignData
{
public:
    QString call;
    QString fname;      // first name
    QString name;       // last name
    QString addr1;      // street address
    QString addr2;      // city
    QString state;      // state (USA)
    QString zip;
    QString country;    // QSL mailing address country
    QString land;       // DXCC entity name
    QString grid;       // Maidenhead grid locator
    double lat;
    double lon;
    int dxcc;           // DXCC entity ID
    int cqzone;
    int ituzone;
    QString iota;
    bool eqsl;
    bool lotw;
    bool mqsl;

    QrzCallsignData();
};

class QrzApi : public QObject
{
    Q_OBJECT

public:
    explicit QrzApi(QObject *parent = nullptr);
    ~QrzApi();

    void setCredentials(const QString& username, const QString& password);
    void setUserAgent(const QString& userAgent);
    void getSession();
    void lookupCallsign(const QString& callsign);
    bool hasValidSession() const;
    bool hasCredentials() const;
    QString getSessionToken() const;

signals:
    void sessionObtained(const QString& token);
    void sessionError(const QString& error);
    void callsignFound(const QrzCallsignData& data);
    void callsignNotFound(const QString& callsign);
    void lookupError(const QString& error);

private slots:
    void onSessionReply();
    void onCallsignReply();

private:
    QrzCallsignData parseCallsignXml(const QString& xmlData);
    QString extractXmlValue(const QString& xmlData, const QString& tag);

    QNetworkAccessManager *m_networkManager;
    QString m_username;
    QString m_password;
    QString m_userAgent;
    QString m_sessionToken;
    QNetworkReply *m_sessionReply;
    QNetworkReply *m_callsignReply;
};

#endif // QRZAPI_H
