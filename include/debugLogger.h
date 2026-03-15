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

#ifndef DEBUGLOGGER_H
#define DEBUGLOGGER_H

#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

class DebugLogger
{
public:
    static DebugLogger& instance();

    void log(const QString& component, const QString& message);
    void init(const QString& logPath = QString());
    void loadSettings();
    void setFlushEnabled(bool enabled);
    void setFlrigDebugEnabled(bool enabled);
    bool isFlrigDebugEnabled() const;
    void setMainWindowDebugEnabled(bool enabled);
    bool isMainWindowDebugEnabled() const;
    void setContestEngineDebugEnabled(bool enabled);
    bool isContestEngineDebugEnabled() const;
    void setContestSelectDialogDebugEnabled(bool enabled);
    bool isContestSelectDialogDebugEnabled() const;
    void setCWWindowDebugEnabled(bool enabled);
    bool isCWWindowDebugEnabled() const;
    void setDxccDatabaseDebugEnabled(bool enabled);
    bool isDxccDatabaseDebugEnabled() const;
    void setDxClusterDebugEnabled(bool enabled);
    bool isDxClusterDebugEnabled() const;
    void setScpDebugEnabled(bool enabled);
    bool isScpDebugEnabled() const;
    void setMultiplierWidgetDebugEnabled(bool enabled);
    bool isMultiplierWidgetDebugEnabled() const;
    void setCallsignLookupDebugEnabled(bool enabled);
    bool isCallsignLookupDebugEnabled() const;
    void setStdoutEnabled(bool enabled);

private:
    DebugLogger() = default;
    bool m_flrigDebugEnabled = false;
    bool m_mainWindowDebugEnabled = false;
    bool m_contestEngineDebugEnabled = false;
    bool m_contestSelectDialogDebugEnabled = false;
    bool m_cwWindowDebugEnabled = false;
    bool m_dxccDatabaseDebugEnabled = false;
    bool m_dxClusterDebugEnabled = false;
    bool m_scpDebugEnabled = false;
    bool m_multiplierWidgetDebugEnabled = false;
    bool m_callsignLookupDebugEnabled = false;
    bool m_stdoutEnabled = false;
    bool m_flushEnabled = false;  // true = flush after every write (--flush flag)

    QFile m_logFile;
    qint64 m_logBytesWritten = 0;

    void writeToFile(const QString& msg);
    void rotatIfNeeded();
};

#endif // DEBUGLOGGER_H
