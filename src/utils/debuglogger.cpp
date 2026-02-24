/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "debuglogger.h"
#include <QMutex>
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
    QMutexLocker locker(&g_logMutex);
    if (m_logFile.isOpen())
        m_logFile.close();

    m_logFile.setFileName("clx_debug.log");
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    m_logBytesWritten = 0;

    if (m_logFile.isOpen()) {
        QString msg = QString("[%1] INFO: ContestLogX debug log started\n")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));
        QTextStream ts(&m_logFile);
        ts << msg;
        if (m_flushEnabled)
            m_logFile.flush();
        m_logBytesWritten += msg.toUtf8().size();
    }
}

void DebugLogger::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "ContestLogX", "ContestLogX");
    m_flrigDebugEnabled = settings.value("Debug/FlrigDebug", false).toBool();
    m_mainWindowDebugEnabled = settings.value("Debug/MainWindowDebug", false).toBool();
    m_contestEngineDebugEnabled = settings.value("Debug/ContestEngineDebug", false).toBool();
    m_contestSelectDialogDebugEnabled = settings.value("Debug/ContestSelectDialogDebug", false).toBool();
    m_cwWindowDebugEnabled = settings.value("Debug/CWWindowDebug", false).toBool();
    m_dxccDatabaseDebugEnabled = settings.value("Debug/DxccDatabaseDebug", false).toBool();
    m_scpDebugEnabled = settings.value("Debug/ScpDebug", false).toBool();
    m_multiplierWidgetDebugEnabled = settings.value("Debug/MultiplierWidgetDebug", false).toBool();
}

void DebugLogger::setFlushEnabled(bool enabled)
{
    m_flushEnabled = enabled;
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

void DebugLogger::setContestSelectDialogDebugEnabled(bool enabled)
{
    m_contestSelectDialogDebugEnabled = enabled;
    log("INFO", enabled ? "ContestSelectDialog debug logging enabled" : "ContestSelectDialog debug logging disabled");
}

bool DebugLogger::isContestSelectDialogDebugEnabled() const
{
    return m_contestSelectDialogDebugEnabled;
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

void DebugLogger::setDxClusterDebugEnabled(bool enabled)
{
    m_dxClusterDebugEnabled = enabled;
    log("INFO", enabled ? "DX Cluster debug logging enabled" : "DX Cluster debug logging disabled");
}

bool DebugLogger::isDxClusterDebugEnabled() const
{
    return m_dxClusterDebugEnabled;
}

void DebugLogger::setScpDebugEnabled(bool enabled)
{
    m_scpDebugEnabled = enabled;
    log("INFO", enabled ? "Super Check Partial debug logging enabled" : "Super Check Partial debug logging disabled");
}

bool DebugLogger::isScpDebugEnabled() const
{
    return m_scpDebugEnabled;
}

void DebugLogger::setMultiplierWidgetDebugEnabled(bool enabled)
{
    m_multiplierWidgetDebugEnabled = enabled;
    log("INFO", enabled ? "MultiplierWidget debug logging enabled" : "MultiplierWidget debug logging disabled");
}

bool DebugLogger::isMultiplierWidgetDebugEnabled() const
{
    return m_multiplierWidgetDebugEnabled;
}

void DebugLogger::setStdoutEnabled(bool enabled)
{
    m_stdoutEnabled = enabled;
}

void DebugLogger::rotatIfNeeded()
{
    // Called inside locked section. Rotate log if over size limit.
    if (m_logBytesWritten < MAX_LOG_SIZE)
        return;

    m_logFile.close();
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    m_logBytesWritten = 0;

    if (m_logFile.isOpen()) {
        QString msg = QString("[%1] INFO: ContestLogX debug log truncated (reached 5MB limit)\n")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));
        QTextStream ts(&m_logFile);
        ts << msg;
        m_logBytesWritten += msg.toUtf8().size();
    }
}

void DebugLogger::writeToFile(const QString& msg)
{
    // Called inside locked section. File must be open.
    if (!m_logFile.isOpen())
        return;
    QTextStream ts(&m_logFile);
    ts << msg;
    if (m_flushEnabled)
        m_logFile.flush();
    m_logBytesWritten += msg.toUtf8().size();
}

void DebugLogger::log(const QString& component, const QString& message)
{
    // Fast-path guards — cheap boolean checks before any lock or allocation.
    if (!m_flrigDebugEnabled && component == "Flrig") return;
    if (!m_mainWindowDebugEnabled && component == "MainWindow") return;
    if (!m_contestEngineDebugEnabled && component == "ContestEngine") return;
    if (!m_contestSelectDialogDebugEnabled && component == "ContestSelectDialog") return;
    if (!m_cwWindowDebugEnabled && component == "CWWindow") return;
    if (!m_dxccDatabaseDebugEnabled && component == "DxccDatabase") return;
    if (!m_dxClusterDebugEnabled && component == "DxCluster") return;
    if (!m_scpDebugEnabled && (component == "ScpDialog" || component == "ScpLineEdit")) return;
    if (!m_multiplierWidgetDebugEnabled && component == "MultiplierWidget") return;

    QMutexLocker locker(&g_logMutex);

    rotatIfNeeded();

    QString logMsg = QString("[%1] %2: %3\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
        .arg(component)
        .arg(message);

    writeToFile(logMsg);

    if (m_stdoutEnabled) {
        fprintf(stdout, "%s", logMsg.toLocal8Bit().constData());
        fflush(stdout);
    }
}
