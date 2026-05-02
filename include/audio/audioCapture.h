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
#include <QTimer>
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

    // Virtual so the PracticeAudioSource subclass can substitute
    // synthesized tones for a real QAudioSource without the decoder
    // worker having to branch on type.
    virtual bool start();
    virtual void stop();
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

protected:
    // Exposed to PracticeAudioSource so it can push synthesized samples
    // into the same ring buffer the decoder worker drains from, and set
    // the effective sample rate reported to the worker.
    SpscRingBuffer m_ring;
    QAudioFormat m_format;
    bool m_running = false;

private:
    QAudioDevice m_device;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice* m_io = nullptr;

    // Silence-detection diagnostic (real-device capture only — PracticeAudioSource
    // overrides start() and never enables this). If we open a stream against a
    // device that the OS reports as in-use but no non-zero sample ever arrives,
    // emit a clear deviceError so the operator gets a real message instead of
    // staring at an empty decoder. Catches the Windows-with-WASAPI failure mode
    // where the driver/format negotiation succeeds but only zero-filled frames
    // are delivered (e.g., wrong channel, an OS-level mute we can't see, or
    // some app having grabbed the device in exclusive mode).
    static constexpr int kSilenceDetectionMs = 3000;
    QTimer* m_silenceTimer = nullptr;
    bool m_seenNonZeroSample = false;
    bool m_silenceWarned = false;
    int m_chunkLogCount = 0;            // first few chunks get logged for triage
};

} // namespace clx::audio

#endif // AUDIO_AUDIOCAPTURE_H
