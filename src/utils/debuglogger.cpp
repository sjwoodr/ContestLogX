/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "debuglogger.h"
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QFileInfo>
#include <QSettings>

static QMutex g_logMutex;
static const qint64 MAX_LOG_SIZE = 5 * 1024 * 1024; // 5 MB

DebugLogger& DebugLogger::instance()
{
    static DebugLogger logger;
    return logger;
}

void DebugLogger::init()
{
    // Truncate the log file on startup
    QFile logFile("clx_debug.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream log(&logFile);
        log << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") << "] "
            << "INFO: ContestLogX debug log started\n";
        logFile.close();
    }
}

void DebugLogger::loadSettings()
{
    // Load debug settings from Settings without logging (to avoid early log messages)
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "ContestLogX", "ContestLogX");
    m_flrigDebugEnabled = settings.value("Debug/FlrigDebug", false).toBool();
    m_mainWindowDebugEnabled = settings.value("Debug/MainWindowDebug", false).toBool();
    m_contestEngineDebugEnabled = settings.value("Debug/ContestEngineDebug", true).toBool();
    m_cwWindowDebugEnabled = settings.value("Debug/CWWindowDebug", false).toBool();
    m_dxccDatabaseDebugEnabled = settings.value("Debug/DxccDatabaseDebug", true).toBool();
}

void DebugLogger::setFlrigDebugEnabled(bool enabled)
{
    m_flrigDebugEnabled = enabled;
    log("INFO", enabled ? "flrig debug logging enabled" : "flrig debug logging disabled");
}

bool DebugLogger::isFlrigDebugEnabled() const
{
    return m_flrigDebugEnabled;
}

void DebugLogger::setMainWindowDebugEnabled(bool enabled)
{
    m_mainWindowDebugEnabled = enabled;
    log("INFO", enabled ? "MainWindow debug logging enabled" : "MainWindow debug logging disabled");
}

bool DebugLogger::isMainWindowDebugEnabled() const
{
    return m_mainWindowDebugEnabled;
}

void DebugLogger::setContestEngineDebugEnabled(bool enabled)
{
    m_contestEngineDebugEnabled = enabled;
    log("INFO", enabled ? "ContestEngine debug logging enabled" : "ContestEngine debug logging disabled");
}

bool DebugLogger::isContestEngineDebugEnabled() const
{
    return m_contestEngineDebugEnabled;
}

void DebugLogger::setCWWindowDebugEnabled(bool enabled)
{
    m_cwWindowDebugEnabled = enabled;
    log("INFO", enabled ? "CWWindow debug logging enabled" : "CWWindow debug logging disabled");
}

bool DebugLogger::isCWWindowDebugEnabled() const
{
    return m_cwWindowDebugEnabled;
}

void DebugLogger::setDxccDatabaseDebugEnabled(bool enabled)
{
    m_dxccDatabaseDebugEnabled = enabled;
    log("INFO", enabled ? "DxccDatabase debug logging enabled" : "DxccDatabase debug logging disabled");
}

bool DebugLogger::isDxccDatabaseDebugEnabled() const
{
    return m_dxccDatabaseDebugEnabled;
}

void DebugLogger::setStdoutEnabled(bool enabled)
{
    m_stdoutEnabled = enabled;
}

void DebugLogger::log(const QString& component, const QString& message)
{
    // Skip component-specific debug messages if disabled
    if (!m_flrigDebugEnabled && component == "Flrig") {
        return;
    }
    if (!m_mainWindowDebugEnabled && component == "MainWindow") {
        return;
    }
    if (!m_contestEngineDebugEnabled && component == "ContestEngine") {
        return;
    }
    if (!m_cwWindowDebugEnabled && component == "CWWindow") {
        return;
    }
    if (!m_dxccDatabaseDebugEnabled && component == "DxccDatabase") {
        return;
    }
    
    QMutexLocker locker(&g_logMutex);
    
    // Check log file size
    QFileInfo logInfo("clx_debug.log");
    if (logInfo.exists() && logInfo.size() >= MAX_LOG_SIZE) {
        // Truncate and restart the log
        QFile logFile("clx_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream log(&logFile);
            log << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") << "] "
                << "INFO: ContestLogX debug log truncated (reached 5MB limit)\n";
            logFile.close();
        }
    }
    
    QString logMsg = QString("[%1] %2: %3\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
        .arg(component)
        .arg(message);
    
    QFile logFile("clx_debug.log");
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream log(&logFile);
        log << logMsg;
        logFile.close();
    }
    
    // Also write to stdout if enabled
    if (m_stdoutEnabled) {
        fprintf(stdout, "%s", logMsg.toLocal8Bit().constData());
        fflush(stdout);
    }
}
