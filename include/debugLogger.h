/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
