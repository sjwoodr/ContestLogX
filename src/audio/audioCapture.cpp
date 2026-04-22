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
    // Ring buffer sized for 1 second at the DEVICE's preferred sample rate;
    // since we no longer downsample, this is whatever the mic / virtual
    // device actually runs at (typically 44100 or 48000). Worst-case
    // memory: 96 kB for 1 s at 48 kHz int16. Negligible.
    , m_ring(device.preferredFormat().sampleRate() > 0
             ? device.preferredFormat().sampleRate() * kRingBufferSeconds
             : kSampleRateHz * kRingBufferSeconds)
{
    // Use the device's preferred format as our starting point. This gives
    // us native rate + native sample format, eliminating the need for
    // in-process resampling (which was aliasing noise into the CW band).
    m_format = device.preferredFormat();
    m_format.setChannelCount(1);
    // Force int16 samples — simplest hot-path. If the device doesn't
    // support int16 at its preferred rate, we'll fall back in start().
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
        // Our preferred-rate-with-mono-int16 isn't directly supported.
        // Fall back to the device's preferred format as-is — even if it
        // uses a different sample format or channel count we'll make it
        // work (float32 → int16 conversion happens in onReadyRead).
        QAudioFormat preferred = m_device.preferredFormat();
        if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
            DebugLogger::instance().log("CwDecoder",
                QString("Requested audio format not supported by '%1'; "
                        "using device preferred (%2 Hz, %3 ch, format=%4)")
                    .arg(m_device.description())
                    .arg(preferred.sampleRate())
                    .arg(preferred.channelCount())
                    .arg(static_cast<int>(preferred.sampleFormat())));
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

    const int channels = qMax(1, m_format.channelCount());
    const QAudioFormat::SampleFormat sfmt = m_format.sampleFormat();
    const int bytesPerSample = (sfmt == QAudioFormat::Float) ? 4
                             : (sfmt == QAudioFormat::Int32) ? 4
                             : (sfmt == QAudioFormat::Int16) ? 2
                             : (sfmt == QAudioFormat::UInt8) ? 1
                             : 2;
    const size_t totalFrames = chunk.size() / (bytesPerSample * channels);
    if (totalFrames == 0) return;

    // Convert to int16 mono, taking only channel 0 when stereo is provided.
    // NO resampling — we operate at the device's native rate end-to-end.
    // This eliminates the nearest-neighbor alias fold that was corrupting
    // the CW detection band with noise from above Nyquist/2.
    std::vector<int16_t> mono;
    mono.reserve(totalFrames);

    const char* data = chunk.constData();
    for (size_t i = 0; i < totalFrames; ++i) {
        const char* frame = data + i * bytesPerSample * channels;
        // Take channel 0 from interleaved frame.
        int16_t sample = 0;
        switch (sfmt) {
        case QAudioFormat::Int16: {
            sample = *reinterpret_cast<const int16_t*>(frame);
            break;
        }
        case QAudioFormat::Int32: {
            const int32_t v = *reinterpret_cast<const int32_t*>(frame);
            sample = static_cast<int16_t>(v >> 16);
            break;
        }
        case QAudioFormat::Float: {
            float f = *reinterpret_cast<const float*>(frame);
            if (f > 1.0f) f = 1.0f;
            if (f < -1.0f) f = -1.0f;
            sample = static_cast<int16_t>(f * 32767.0f);
            break;
        }
        case QAudioFormat::UInt8: {
            const uint8_t v = *reinterpret_cast<const uint8_t*>(frame);
            sample = static_cast<int16_t>((static_cast<int>(v) - 128) * 256);
            break;
        }
        default:
            break;
        }
        mono.push_back(sample);
    }

    m_ring.push(mono.data(), mono.size());
    emit audioBlockReady();
}

} // namespace clx::audio
