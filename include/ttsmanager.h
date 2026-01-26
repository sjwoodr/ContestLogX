/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef TTSMANAGER_H
#define TTSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>

/**
 * @brief Manages text-to-speech operations for SSB voice keying
 *
 * Handles TTS generation and audio playback in background processes
 * with timeout handling and error reporting.
 */
class TtsManager : public QObject
{
    Q_OBJECT

public:
    explicit TtsManager(QObject *parent = nullptr);
    ~TtsManager();

    /**
     * @brief Generate speech and play it
     * @param text Text to convert to speech
     */
    void speak(const QString& text);

    /**
     * @brief Cancel any ongoing TTS operation
     */
    void cancel();

    /**
     * @brief Check if TTS is currently active
     */
    bool isActive() const { return m_isActive; }

signals:
    /**
     * @brief Emitted when TTS operation completes successfully
     */
    void finished();

    /**
     * @brief Emitted when TTS operation fails
     * @param error Error message
     */
    void error(const QString& error);

    /**
     * @brief Emitted when TTS generation completes and audio playback starts
     */
    void playbackStarted();

private slots:
    void onTtsFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTtsError(QProcess::ProcessError error);
    void onTtsTimeout();

    void onAudioFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onAudioError(QProcess::ProcessError error);
    void onAudioTimeout();

private:
    void startTtsProcess(const QString& text);
    void startAudioProcess(const QString& audioFile);
    void cleanup();
    QString generateOutputPath() const;

    QProcess *m_ttsProcess;
    QProcess *m_audioProcess;
    QTimer *m_ttsTimer;
    QTimer *m_audioTimer;
    QString m_currentOutputFile;
    bool m_isActive;

    static const int TTS_TIMEOUT_MS = 10000;  // 10 seconds
    static const int AUDIO_TIMEOUT_MS = 60000;  // 60 seconds (for long messages)
};

#endif // TTSMANAGER_H
