/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * ClxSnapshot - thread-safe point-in-time view of the current CLX session
 * state. Populated from MainWindow on score / QSO / rig-state changes;
 * consumed by the HTTP server's JSON endpoints (and eventually by the M/M
 * multicast sender). A single writer (MainWindow) + multiple readers
 * (HTTP request handlers) pattern; protected by QReadWriteLock.
 */

#ifndef NET_CLXSNAPSHOT_H
#define NET_CLXSNAPSHOT_H

#include <QDateTime>
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QVector>

namespace clx::net {

struct RigSnapshot {
    QString backend;       // "flrig", "hamlib", "mocked"
    bool    connected = false;
    qint64  freqHz    = 0;
    QString mode;          // "CW", "USB", ...
    QString band;          // "20m", "40m", ...
    bool    pttActive = false;
    QString runSpMode;     // "Run", "S&P", "Off"
};

struct QsoSnapshot {
    QString dateUtc;       // YYYY-MM-DD
    QString timeUtc;       // HHMMSS
    QString call;
    qint64  freqHz = 0;
    QString mode;
    QString rstSent;
    QString rstRcvd;
    QString exchSent;
    QString exchRcvd;
    int     points = 0;
};

struct ScoreSnapshot {
    int totalQsos = 0;
    int totalPoints = 0;
    int namedMults = 0;
    int dxccMults = 0;
    int ituMults = 0;
    int gridMults = 0;
    int prefixMults = 0;
    qint64 finalScore = 0;
    // QSO breakdown: outer key band ("20m"), inner key mode ("CW"/"PH"/etc).
    QHash<QString, QHash<QString, int>> qsosByBandMode;
};

struct RateSnapshot {
    int currentHourlyRate = 0;   // extrapolated from last 10 minutes
    int lastHourRate = 0;        // actual last 3600s
    int sessionAverageRate = 0;
};

struct PropagationSnapshot {
    int sfi = 0;
    int aIndex = 0;
    int kIndex = 0;
    QDateTime fetchedAt;
};

// Everything the HTTP endpoints need to build their JSON responses, in
// one place. Designed for cheap full-copy under the read lock so request
// handlers don't hold the lock while serializing.
class ClxSnapshot {
public:
    // Writer side - MainWindow calls these on change events.
    void setRunning(bool running);
    void setContestName(const QString& name);
    void setContestFile(const QString& path);
    void setSo2rEnabled(bool enabled);
    void setRig(bool isRightRadio, const RigSnapshot& rig);
    void setScore(const ScoreSnapshot& score);
    void setRate(const RateSnapshot& rate);
    void setPropagation(const PropagationSnapshot& prop);
    // Recent-QSO buffer - MainWindow appends on log, newest last. The
    // snapshot keeps the last N (configurable) so /api/qsos?limit=N can
    // pull cheaply. Full history goes through the app's QsoListModel.
    void pushQso(const QsoSnapshot& qso);
    void setAllQsos(const QVector<QsoSnapshot>& qsos);   // used on file-load
    void setWorkedNamedMults(const QStringList& mults);
    void setStartedAt(const QDateTime& t);

    // Reader side - HTTP handlers take a copy under the read lock and
    // operate on the copy without holding the lock through serialization.
    struct Copy {
        bool running = false;
        QString contestName;
        QString contestFile;
        bool so2rEnabled = false;
        RigSnapshot rigL;
        RigSnapshot rigR;
        ScoreSnapshot score;
        RateSnapshot rate;
        PropagationSnapshot propagation;
        QVector<QsoSnapshot> recentQsos;   // newest last
        QStringList workedNamedMults;
        QDateTime startedAt;
    };
    Copy copy() const;

private:
    mutable QReadWriteLock m_lock;
    Copy m_state;
};

} // namespace clx::net

#endif // NET_CLXSNAPSHOT_H
