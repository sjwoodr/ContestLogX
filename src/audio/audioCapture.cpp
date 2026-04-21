/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/audioCapture.h"
#include "debugLogger.h"

namespace clx::audio {

AudioCapture::AudioCapture(const QAudioDevice& device, QObject* parent)
    : QObject(parent)
    , m_device(device)
    , m_ring(kRingBufferSamples)
{
    m_format.setSampleRate(kSampleRateHz);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioCapture::~AudioCapture()
{
    stop();
}

bool AudioCapture::start()
{
    if (m_running) return true;
    if (m_device.isNull()) {
        emit deviceError(QStringLiteral("Audio device is null"));
        return false;
    }

    if (!m_device.isFormatSupported(m_format)) {
        QAudioFormat preferred = m_device.preferredFormat();
        if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
            DebugLogger::instance().log("CwDecoder",
                QString("Preferred audio format requested (%1 Hz mono S16) not supported by "
                        "'%2'; using device preferred format (%3 Hz, %4 ch)")
                    .arg(m_format.sampleRate())
                    .arg(m_device.description())
                    .arg(preferred.sampleRate())
                    .arg(preferred.channelCount()));
        }
        m_format = preferred;
        if (!m_device.isFormatSupported(m_format)) {
            emit deviceError(QStringLiteral("No supported audio format for device"));
            return false;
        }
    }

    m_source.reset(new QAudioSource(m_device, m_format, this));
    m_io = m_source->start();
    if (!m_io) {
        emit deviceError(QStringLiteral("QAudioSource::start() returned null I/O"));
        m_source.reset();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioCapture::onReadyRead);
    m_running = true;
    emit captureStarted();
    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder",
            QString("Audio capture started on '%1'").arg(m_device.description()));
    }
    return true;
}

void AudioCapture::stop()
{
    if (!m_running) return;
    m_running = false;
    if (m_source) {
        m_source->stop();
        m_source.reset();
    }
    m_io = nullptr;
    emit captureStopped();
    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder",
            QString("Audio capture stopped on '%1'").arg(m_device.description()));
    }
}

void AudioCapture::onReadyRead()
{
    if (!m_io) return;
    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty()) return;

    // Expect int16 mono samples. If format differs (preferred fallback), the
    // sample count/pattern may not be int16 — best-effort cast; Goertzel will
    // tolerate slight scale differences on the amplitude but not on rate.
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const size_t count = chunk.size() / sizeof(int16_t);

    if (m_format.sampleRate() == kSampleRateHz) {
        m_ring.push(samples, count);
    } else {
        // Simple nearest-neighbor downsample to 8 kHz if device gave us a
        // higher rate. Acceptable for CW decode quality at this stage.
        const double ratio = static_cast<double>(m_format.sampleRate()) / kSampleRateHz;
        const size_t outCount = static_cast<size_t>(count / ratio);
        QVector<int16_t> resampled(outCount);
        for (size_t i = 0; i < outCount; ++i) {
            size_t src = static_cast<size_t>(i * ratio);
            if (src >= count) src = count - 1;
            resampled[i] = samples[src];
        }
        m_ring.push(resampled.constData(), resampled.size());
    }

    emit audioBlockReady();
}

} // namespace clx::audio
