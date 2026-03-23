/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef ONLINESCORECLIENT_H
#define ONLINESCORECLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QMap>
#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;

struct ScoreBreakdownEntry {
    QString band;       // "20", "40", "80", "160", "15", "10", "total"
    QString mode;       // "CW", "PH", "RY", "DG", "ALL"
    int qsoCount = 0;
    int points = 0;
    QMap<QString, int> mults;  // type -> count (e.g., "country" -> 42)
};

struct ScorePostData {
    QString contestId;
    QString callsign;
    QString ops;
    QString club;
    // Class attributes
    QString power       = "HIGH";
    QString assisted    = "NON-ASSISTED";
    QString transmitter = "ONE";
    QString opsCategory = "SINGLE-OP";
    QString bands       = "ALL";
    QString mode        = "MIXED";
    QString overlay     = "N/A";
    // QTH
    QString dxccCountry;
    int cqZone = 0;
    int ituZone = 0;
    QString arrlSection;
    QString stPrvOth;
    QString grid;
    // Score data
    QList<ScoreBreakdownEntry> breakdown;
    int totalScore = 0;
};

class OnlineScoreClient : public QObject
{
    Q_OBJECT

public:
    explicit OnlineScoreClient(QObject *parent = nullptr);

    void setCredentials(const QString& callsign, const QString& password);
    void postScore(const ScorePostData& data);
    bool isPostInFlight() const { return m_inFlight; }

signals:
    void postSuccess(const QString& timestamp);
    void postFailed(const QString& error);
    void authFailed();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QByteArray buildXml(const ScorePostData& data) const;

    QNetworkAccessManager *m_networkManager;
    QString m_authCallsign;
    QString m_authPassword;
    bool m_inFlight = false;
    int m_consecutiveAuthFailures = 0;

    static constexpr const char* POST_URL = "https://contestonlinescore.com/post/";
    static constexpr int TIMEOUT_MS = 15000;
};

#endif // ONLINESCORECLIENT_H
