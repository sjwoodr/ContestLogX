#ifndef QRZCQAPI_H
#define QRZCQAPI_H

#include <QString>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QrzcqCallsignData
{
public:
    QString call;
    QString name;
    QString qth;
    QString address;
    QString city;
    QString zip;
    QString license;
    QString continent;
    QString country;
    QString state;
    QString county;
    QString locator;
    double latitude;
    double longitude;
    QString website;
    int dxcc;
    int itu;
    int cq;
    QString iota;
    QString prefix;
    bool eqsl;
    bool lotw;
    bool bqsl;
    bool mqsl;
    bool utf8;
    QString qslpic;

    QrzcqCallsignData();
};

class QrzcqApi : public QObject
{
    Q_OBJECT

public:
    explicit QrzcqApi(QObject *parent = nullptr);
    ~QrzcqApi();

    // Set credentials for API access
    void setCredentials(const QString &username, const QString &password);
    
    // Set user agent string
    void setUserAgent(const QString &userAgent);
    
    // Obtain a session token (valid for 3 days)
    void getSession();
    
    // Look up a callsign asynchronously
    void lookupCallsign(const QString &callsign);
    
    // Check if we have a valid cached session token
    bool hasValidSession() const;
    
    // Get the current session token
    QString getSessionToken() const;
    
    // Check if credentials are configured
    bool hasCredentials() const;

signals:
    // Emitted when session is obtained
    void sessionObtained(const QString &token);
    void sessionError(const QString &error);
    
    // Emitted when callsign lookup completes
    void callsignFound(const QrzcqCallsignData &data);
    void callsignNotFound(const QString &callsign);
    void lookupError(const QString &error);

private slots:
    void onSessionReply();
    void onCallsignReply();
    void onWebScrapingReply();

private:
    // Parse XML response
    QrzcqCallsignData parseCallsignXml(const QString &xmlData);
    QString parseSessionXml(const QString &xmlData);
    QString extractXmlValue(const QString &xmlData, const QString &tag);
    
    // Web scraping fallback
    QrzcqCallsignData scrapeCallsignFromHtml(const QString &htmlData);

    QNetworkAccessManager *m_networkManager;
    QString m_username;
    QString m_password;
    QString m_userAgent;
    QString m_sessionToken;
    
    // Track pending requests
    QNetworkReply *m_sessionReply;
    QNetworkReply *m_callsignReply;
    QNetworkReply *m_scrapingReply;
};

#endif // QRZCQAPI_H
