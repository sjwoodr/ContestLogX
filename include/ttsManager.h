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

#ifndef TTSMANAGER_H
#define TTSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>

class FlrigClient;

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
     * @brief Set the flrig client for data mode switching during audio playback
     * @param client FlrigClient instance (may be nullptr)
     */
    void setFlrigClient(FlrigClient *client) { m_flrigClient = client; }

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
    void switchToDataMode();
    void restoreOriginalMode();
    QString generateOutputPath() const;

    QProcess *m_ttsProcess;
    QProcess *m_audioProcess;
    QTimer *m_ttsTimer;
    QTimer *m_audioTimer;
    QString m_currentOutputFile;
    bool m_isActive;

    FlrigClient *m_flrigClient;
    QString m_savedMode;

    static const int TTS_TIMEOUT_MS = 10000;   // 10 seconds
    static const int AUDIO_TIMEOUT_MS = 60000;  // 60 seconds (for long messages)
    static const int PTT_SETTLE_MS = 150;       // delay after PTT for relay settle
};

#endif // TTSMANAGER_H
