/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef DEBUGLOGGER_H
#define DEBUGLOGGER_H

#include <QString>
#include <QDateTime>

class DebugLogger
{
public:
    static DebugLogger& instance();
    
    void log(const QString& component, const QString& message);
    void init();
    void loadSettings();
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
    void setStdoutEnabled(bool enabled);
    
private:
    DebugLogger() = default;
    bool m_flrigDebugEnabled = false;
    bool m_mainWindowDebugEnabled = false;
    bool m_contestEngineDebugEnabled = false;
    bool m_contestSelectDialogDebugEnabled = false;
    bool m_cwWindowDebugEnabled = false;
    bool m_dxccDatabaseDebugEnabled = false;
    bool m_stdoutEnabled = false;
};

#endif // DEBUGLOGGER_H
