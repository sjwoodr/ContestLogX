/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * AudioCapture — QAudioSource wrapper that pushes audio into an SPSC ring
 * buffer consumed by the decoder worker (SPEC-005).
 */

#ifndef AUDIO_AUDIOCAPTURE_H
#define AUDIO_AUDIOCAPTURE_H

#include <QObject>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <memory>

#include "audio/audioTypes.h"
#include "audio/spscRingBuffer.h"

namespace clx::audio {

class AudioCapture : public QObject
{
    Q_OBJECT
public:
    explicit AudioCapture(const QAudioDevice& device, QObject* parent = nullptr);
    ~AudioCapture() override;

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    // Consumer side — called by the decoder worker.
    // Returns number of samples read into `out` (<= count).
    size_t popSamples(int16_t* out, size_t count) {
        return m_ring.pop(out, count);
    }

    size_t availableSamples() const { return m_ring.available(); }

    const QAudioDevice& device() const { return m_device; }

    // The actual sample rate we're capturing at. Set by start() after
    // format negotiation; 0 until start() returns true. Consumers (the
    // decoder worker) use this to size blocks and compute Goertzel
    // coefficients for the bins at the native rate — we do NOT
    // downsample internally any more (avoiding the alias-fold of
    // above-Nyquist content back into our detection band).
    int actualSampleRate() const { return m_format.sampleRate(); }

signals:
    void audioBlockReady();                     // fired whenever new audio arrives
    void deviceError(const QString& message);
    void captureStarted();
    void captureStopped();

private slots:
    void onReadyRead();

private:
    QAudioDevice m_device;
    QAudioFormat m_format;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice* m_io = nullptr;
    SpscRingBuffer m_ring;
    bool m_running = false;
};

} // namespace clx::audio

#endif // AUDIO_AUDIOCAPTURE_H
