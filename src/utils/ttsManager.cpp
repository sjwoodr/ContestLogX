/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "ttsManager.h"
#include "settings.h"
#include "debugLogger.h"
#include "piperManager.h"
#include "rigInterface.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDateTime>

TtsManager::TtsManager(QObject *parent)
    : QObject(parent)
    , m_ttsProcess(nullptr)
    , m_audioProcess(nullptr)
    , m_ttsTimer(new QTimer(this))
    , m_audioTimer(new QTimer(this))
    , m_isActive(false)
    , m_cancelled(false)
    , m_rigClient(nullptr)
{
    m_ttsTimer->setSingleShot(true);
    connect(m_ttsTimer, &QTimer::timeout, this, &TtsManager::onTtsTimeout);

    m_audioTimer->setSingleShot(true);
    connect(m_audioTimer, &QTimer::timeout, this, &TtsManager::onAudioTimeout);
}

TtsManager::~TtsManager()
{
    cancel();
}

void TtsManager::speak(const QString& text)
{
    if (m_isActive) {
        DebugLogger::instance().log("TtsManager", "TTS already active, ignoring new request");
        return;
    }

    Settings& settings = Settings::instance();
    if (!settings.getSsbKeyingEnabled()) {
        DebugLogger::instance().log("TtsManager", "SSB keying disabled in settings");
        return;
    }

    QString ttsCmd = settings.getTtsCommand();
    if (ttsCmd.isEmpty()) {
        emit error("TTS command not configured");
        return;
    }

    DebugLogger::instance().log("TtsManager", QString("Starting TTS for text: %1").arg(text));

    m_isActive = true;
    m_cancelled = false;
    startTtsProcess(text);
}

void TtsManager::cancel()
{
    DebugLogger::instance().log("TtsManager", "Cancelling TTS operation");

    m_cancelled = true;

    m_ttsTimer->stop();
    m_audioTimer->stop();

    if (m_ttsProcess) {
        m_ttsProcess->kill();
        m_ttsProcess->waitForFinished(1000);
        m_ttsProcess->deleteLater();
        m_ttsProcess = nullptr;
    }

    if (m_audioProcess) {
        m_audioProcess->kill();
        m_audioProcess->waitForFinished(1000);
        m_audioProcess->deleteLater();
        m_audioProcess = nullptr;
    }

    // Restore mode after processes are stopped (no concurrent flrig activity)
    restoreOriginalMode(false);  // synchronous: processes already dead, cancel is immediate

    cleanup();
    m_isActive = false;
}

QString TtsManager::generateOutputPath() const
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    return tempDir + "/clx_tts_" + timestamp + ".wav";
}

void TtsManager::startTtsProcess(const QString& text)
{
    Settings& settings = Settings::instance();

    QString ttsCmd = settings.getTtsCommand();
    QString ttsArgs = settings.getTtsArgs();

    m_currentOutputFile = generateOutputPath();

    // Build command arguments with template substitution
    QString argsStr = ttsArgs;
    argsStr.replace("{OUTPUT}", m_currentOutputFile);
    argsStr.replace("{TEXT}", text);

    QStringList argsList;

#ifdef Q_OS_MACOS
    // macOS 'say' command special handling
    if (ttsCmd == "say") {
        argsList << "-o" << m_currentOutputFile << "--data-format=LEF32@22050" << text;
    } else if (!argsStr.isEmpty()) {
        argsList = argsStr.split(' ', Qt::SkipEmptyParts);
    }
#else
    // Linux and others - add --data-dir for piper to find downloaded models
    if (ttsCmd.contains("piper")) {
        PiperManager piperMgr;
        argsList << "--data-dir" << piperMgr.getVoiceModelDir();
    }
    if (!argsStr.isEmpty()) {
        argsList.append(argsStr.split(' ', Qt::SkipEmptyParts));
    }
#endif

    DebugLogger::instance().log("TtsManager",
        QString("TTS command: %1 %2").arg(ttsCmd).arg(argsList.join(" ")));

    m_ttsProcess = new QProcess(this);
    connect(m_ttsProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TtsManager::onTtsFinished);
    connect(m_ttsProcess, &QProcess::errorOccurred, this, &TtsManager::onTtsError);

    m_ttsProcess->start(ttsCmd, argsList);

#ifndef Q_OS_MACOS
    // On Linux, piper expects text on stdin
    if (ttsCmd.contains("piper")) {
        m_ttsProcess->write(text.toUtf8());
        m_ttsProcess->closeWriteChannel();
    }
#endif

    m_ttsTimer->start(TTS_TIMEOUT_MS);
}

void TtsManager::startAudioProcess(const QString& audioFile)
{
    Settings& settings = Settings::instance();

    QString audioCmd = settings.getAudioPlayCommand();
    QString audioArgs = settings.getAudioPlayArgs();

    if (audioCmd.isEmpty()) {
        emit error("Audio playback command not configured");
        cleanup();
        m_isActive = false;
        return;
    }

    // Build command arguments with template substitution
    QString argsStr = audioArgs;
    argsStr.replace("{FILE}", audioFile);

    QStringList argsList;
    if (!argsStr.isEmpty()) {
        argsList = argsStr.split(' ', Qt::SkipEmptyParts);
    }

    DebugLogger::instance().log("TtsManager",
        QString("Audio command: %1 %2").arg(audioCmd).arg(argsList.join(" ")));

    // Switch to data mode (USB-D/LSB-D) and engage PTT
    switchToDataMode();

    // Delay audio start to let the radio's PTT relay settle
    int delayMs = m_savedMode.isEmpty() ? 0 : PTT_SETTLE_MS;

    auto launchAudio = [this, audioCmd, argsList]() {
        if (!m_isActive) return;  // cancelled during delay

        m_audioProcess = new QProcess(this);
        connect(m_audioProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &TtsManager::onAudioFinished);
        connect(m_audioProcess, &QProcess::errorOccurred, this, &TtsManager::onAudioError);

        m_audioProcess->start(audioCmd, argsList);
        m_audioTimer->start(AUDIO_TIMEOUT_MS);

        emit playbackStarted();
    };

    if (delayMs > 0) {
        DebugLogger::instance().log("TtsManager",
            QString("Waiting %1ms for PTT settle").arg(delayMs));
        QTimer::singleShot(delayMs, this, launchAudio);
    } else {
        launchAudio();
    }
}

void TtsManager::onTtsFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_ttsTimer->stop();

    DebugLogger::instance().log("TtsManager",
        QString("TTS finished with exit code: %1, status: %2")
        .arg(exitCode).arg(exitStatus == QProcess::NormalExit ? "normal" : "crashed"));

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString errorMsg = QString::fromUtf8(m_ttsProcess->readAllStandardError());
        DebugLogger::instance().log("TtsManager", QString("TTS error: %1").arg(errorMsg));
        emit error(QString("TTS failed: %1").arg(errorMsg));
        cleanup();
        m_isActive = false;
        m_ttsProcess->deleteLater();
        m_ttsProcess = nullptr;
        return;
    }

    m_ttsProcess->deleteLater();
    m_ttsProcess = nullptr;

    // Check if output file exists
    if (!QFile::exists(m_currentOutputFile)) {
        emit error("TTS output file not created");
        cleanup();
        m_isActive = false;
        return;
    }

    // Start audio playback
    startAudioProcess(m_currentOutputFile);
}

void TtsManager::onTtsError(QProcess::ProcessError processError)
{
    m_ttsTimer->stop();

    if (m_cancelled) {
        cleanup();
        m_isActive = false;
        return;
    }

    QString errorMsg;
    switch (processError) {
        case QProcess::FailedToStart:
            errorMsg = "TTS process failed to start";
            break;
        case QProcess::Crashed:
            errorMsg = "TTS process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "TTS process timed out";
            break;
        default:
            errorMsg = "TTS process error";
            break;
    }

    DebugLogger::instance().log("TtsManager", errorMsg);
    emit error(errorMsg);
    cleanup();
    m_isActive = false;
}

void TtsManager::onTtsTimeout()
{
    DebugLogger::instance().log("TtsManager", "TTS timeout");
    if (m_ttsProcess) {
        m_ttsProcess->kill();
    }
    emit error("TTS operation timed out");
    cleanup();
    m_isActive = false;
}

void TtsManager::onAudioFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_audioTimer->stop();

    DebugLogger::instance().log("TtsManager",
        QString("Audio finished with exit code: %1, status: %2")
        .arg(exitCode).arg(exitStatus == QProcess::NormalExit ? "normal" : "crashed"));

    if (m_cancelled) {
        // cancel() will call restoreOriginalMode() after process cleanup
        cleanup();
        m_isActive = false;
        return;
    }

    // Restore original mode: release PTT now, then delay before mode change so
    // the radio's PTT relay has time to fully settle before accepting a mode command.
    restoreOriginalMode(true);

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString errorMsg = QString::fromUtf8(m_audioProcess->readAllStandardError());
        DebugLogger::instance().log("TtsManager", QString("Audio error: %1").arg(errorMsg));
        emit error(QString("Audio playback failed: %1").arg(errorMsg));
    } else {
        emit finished();
    }

    m_audioProcess->deleteLater();
    m_audioProcess = nullptr;
    cleanup();
    m_isActive = false;
}

void TtsManager::onAudioError(QProcess::ProcessError processError)
{
    m_audioTimer->stop();

    if (m_cancelled) {
        // cancel() will call restoreOriginalMode() after process cleanup
        cleanup();
        m_isActive = false;
        return;
    }

    restoreOriginalMode(true);

    QString errorMsg;
    switch (processError) {
        case QProcess::FailedToStart:
            errorMsg = "Audio process failed to start";
            break;
        case QProcess::Crashed:
            errorMsg = "Audio process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Audio process timed out";
            break;
        default:
            errorMsg = "Audio process error";
            break;
    }

    DebugLogger::instance().log("TtsManager", errorMsg);
    emit error(errorMsg);
    cleanup();
    m_isActive = false;
}

void TtsManager::onAudioTimeout()
{
    DebugLogger::instance().log("TtsManager", "Audio timeout");
    restoreOriginalMode(true);
    if (m_audioProcess) {
        m_audioProcess->kill();
    }
    emit error("Audio playback timed out");
    cleanup();
    m_isActive = false;
}

void TtsManager::switchToDataMode()
{
    m_savedMode.clear();

    if (!m_rigClient || !m_rigClient->isConnected())
        return;

    QString currentMode = m_rigClient->getMode();
    if (currentMode.isEmpty())
        return;

    QString dataMode;
    if (currentMode == "USB")
        dataMode = "USB-D";
    else if (currentMode == "LSB")
        dataMode = "LSB-D";

    if (!dataMode.isEmpty()) {
        DebugLogger::instance().log("TtsManager",
            QString("Switching from %1 to %2 for audio playback").arg(currentMode, dataMode));
        m_savedMode = currentMode;
        m_rigClient->setMode(dataMode);
        m_rigClient->setPTT(true);
        DebugLogger::instance().log("TtsManager", "PTT engaged");
    }
}

void TtsManager::restoreOriginalMode(bool delayed)
{
    if (m_savedMode.isEmpty() || !m_rigClient || !m_rigClient->isConnected())
        return;

    m_rigClient->setPTT(false);
    DebugLogger::instance().log("TtsManager", "PTT released");

    if (delayed) {
        // Give the radio's PTT relay time to fully release before changing mode.
        // Some radios (e.g. ICOM) lock mode changes while PTT is active/settling.
        QString savedMode = m_savedMode;
        m_savedMode.clear();  // clear now so a concurrent cancel() is a no-op
        QTimer::singleShot(PTT_SETTLE_MS, this, [this, savedMode]() {
            if (!m_rigClient || !m_rigClient->isConnected())
                return;
            DebugLogger::instance().log("TtsManager",
                QString("Restoring mode to %1").arg(savedMode));
            m_rigClient->setMode(savedMode);
        });
    } else {
        DebugLogger::instance().log("TtsManager",
            QString("Restoring mode to %1").arg(m_savedMode));
        m_rigClient->setMode(m_savedMode);
        m_savedMode.clear();
    }
}

void TtsManager::cleanup()
{
    // Remove temporary TTS output file
    if (!m_currentOutputFile.isEmpty() && QFile::exists(m_currentOutputFile)) {
        QFile::remove(m_currentOutputFile);
        DebugLogger::instance().log("TtsManager",
            QString("Cleaned up temp file: %1").arg(m_currentOutputFile));
        m_currentOutputFile.clear();
    }
}
