/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "net/clxSnapshot.h"

namespace clx::net {

namespace {
constexpr int kRecentQsoCap = 200;  // served through /api/qsos?limit=N, pagination via offset
} // namespace

void ClxSnapshot::setRunning(bool running)
{
    QWriteLocker lk(&m_lock);
    m_state.running = running;
}

void ClxSnapshot::setContestName(const QString& name)
{
    QWriteLocker lk(&m_lock);
    m_state.contestName = name;
}

void ClxSnapshot::setContestFile(const QString& path)
{
    QWriteLocker lk(&m_lock);
    m_state.contestFile = path;
}

void ClxSnapshot::setSo2rEnabled(bool enabled)
{
    QWriteLocker lk(&m_lock);
    m_state.so2rEnabled = enabled;
}

void ClxSnapshot::setRig(bool isRightRadio, const RigSnapshot& rig)
{
    QWriteLocker lk(&m_lock);
    if (isRightRadio) m_state.rigR = rig;
    else              m_state.rigL = rig;
}

void ClxSnapshot::setScore(const ScoreSnapshot& score)
{
    QWriteLocker lk(&m_lock);
    m_state.score = score;
}

void ClxSnapshot::setRate(const RateSnapshot& rate)
{
    QWriteLocker lk(&m_lock);
    m_state.rate = rate;
}

void ClxSnapshot::setPropagation(const PropagationSnapshot& prop)
{
    QWriteLocker lk(&m_lock);
    m_state.propagation = prop;
}

void ClxSnapshot::pushQso(const QsoSnapshot& qso)
{
    QWriteLocker lk(&m_lock);
    m_state.recentQsos.append(qso);
    // Trim from the front when the bounded cache fills up. 200 covers
    // the dashboard's "recent QSOs" panel comfortably; full history is
    // served from the app's QsoListModel if we ever need it remotely.
    while (m_state.recentQsos.size() > kRecentQsoCap) {
        m_state.recentQsos.removeFirst();
    }
}

void ClxSnapshot::setAllQsos(const QVector<QsoSnapshot>& qsos)
{
    QWriteLocker lk(&m_lock);
    if (qsos.size() <= kRecentQsoCap) {
        m_state.recentQsos = qsos;
    } else {
        // Keep only the last N so the recent-cache stays bounded.
        m_state.recentQsos = qsos.mid(qsos.size() - kRecentQsoCap);
    }
}

void ClxSnapshot::setWorkedNamedMults(const QStringList& mults)
{
    QWriteLocker lk(&m_lock);
    m_state.workedNamedMults = mults;
}

void ClxSnapshot::setStartedAt(const QDateTime& t)
{
    QWriteLocker lk(&m_lock);
    m_state.startedAt = t;
}

ClxSnapshot::Copy ClxSnapshot::copy() const
{
    QReadLocker lk(&m_lock);
    return m_state;
}

} // namespace clx::net
