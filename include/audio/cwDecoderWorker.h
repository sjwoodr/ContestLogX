/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * CwDecoderWorker — QObject running on a dedicated QThread; owns a CwDecoder
 * and an AudioCapture pointer; drains audio samples from the ring buffer,
 * runs decode, emits decoded character and WPM signals (SPEC-005).
 */

#ifndef AUDIO_CWDECODERWORKER_H
#define AUDIO_CWDECODERWORKER_H

#include <QObject>
#include <QChar>
#include <QList>
#include <QMutex>
#include <QVector>

#include "audio/audioTypes.h"
#include "audio/cwDecoder.h"
#include "audio/audioCapture.h"

namespace clx::audio {

class CwDecoderWorker : public QObject
{
    Q_OBJECT
public:
    explicit CwDecoderWorker(QObject* parent = nullptr);
    ~CwDecoderWorker() override;

public slots:
    // Takes ownership of the capture object (via setParent(this) on worker thread).
    void startCapture(clx::audio::AudioCapture* capture,
                      int passbandLowHz, int passbandHighHz, int binCount,
                      int wpmMin, int wpmMax, float squelchThreshold);
    void stopCapture();
    void reconfigure(int passbandLowHz, int passbandHighHz, int binCount);
    void setWpmRange(int wpmMin, int wpmMax);
    void setSquelch(float threshold);
    void setPttMute(bool active);
    void muteForInternalSend(int durationMs);
    void clearBuffers();
    void onAudioBlockReady();   // invoked via queued connection from capture

signals:
    void charDecoded(int binIndex, QChar ch, qint64 timestampMs);
    void wpmUpdated(int binIndex, int wpm);
    void binLayoutChanged(const QList<double>& centerFrequencies);
    void muteStateChanged(bool muted);
    void captureStarted();
    void captureStopped();
    void errorOccurred(const QString& message);
    void pttFallbackLogged();   // emitted once when PTT signal missing + mute needed

private:
    void drainAndProcess();
    qint64 monotonicNowMs() const;

    CwDecoder m_decoder;
    AudioCapture* m_capture = nullptr;
    MuteState m_muteState;
    QVector<int> m_lastReportedWpm;   // per-bin; used to rate-limit wpmUpdated emissions
    QMutex m_mutex;                   // guards mute-state mutations
    int m_sampleRateHz = 0;           // actual capture rate, set after start()
    int m_blockSamples = 0;           // samples per DSP block at m_sampleRateHz
};

} // namespace clx::audio

#endif // AUDIO_CWDECODERWORKER_H
