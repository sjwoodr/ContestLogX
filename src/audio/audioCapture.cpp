/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/audioCapture.h"
#include "debugLogger.h"

#include <algorithm>
#include <cstdlib>

namespace clx::audio {

AudioCapture::AudioCapture(const QAudioDevice& device, QObject* parent)
    : QObject(parent)
    // Ring buffer sized for 1 second at the DEVICE's preferred sample rate;
    // since we no longer downsample, this is whatever the mic / virtual
    // device actually runs at (typically 44100 or 48000). Worst-case
    // memory: 96 kB for 1 s at 48 kHz int16. Negligible. Falls back to a
    // default rate for null devices (used by PracticeAudioSource, which
    // initializes with a null QAudioDevice and sets its own rate later).
    , m_ring(device.preferredFormat().sampleRate() > 0
             ? device.preferredFormat().sampleRate() * kRingBufferSeconds
             : kSampleRateHz * kRingBufferSeconds)
    , m_device(device)
{
    // Use the device's preferred format unchanged. We deliberately do NOT
    // override channel count or sample format here: on Windows / WASAPI the
    // backend will sometimes report `isFormatSupported(monoInt16) == true`
    // even though the driver only ever delivers data in its native format
    // (e.g. stereo Float). When that mismatch happens the stream opens
    // successfully and the OS shows the app as "currently using the
    // microphone," but the readyRead callbacks deliver only zero-filled
    // buffers — the exact "decoder shows nothing" symptom users hit. By
    // taking the device's native format directly and converting to mono
    // int16 ourselves in onReadyRead() (which already handles every Qt
    // sample format and arbitrary channel counts) we avoid that path.
    m_format = device.preferredFormat();
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
        // The device's preferred format isn't actually supported — extremely
        // unusual (preferredFormat() is supposed to return something the
        // device can do natively) but possible if a virtual / aggregate
        // device misreports. Nothing to fall back to in that case.
        emit deviceError(QStringLiteral("No supported audio format for device"));
        return false;
    }

    m_source.reset(new QAudioSource(m_device, m_format, this));

    // Diagnostic: log every QAudioSource state transition. On Windows we've
    // seen the source go silently Idle or Stopped after the initial buffered
    // burst (~20 ms at 48 kHz), without any visible error — readyRead just
    // stops firing and the decoder appears dead. Logging the state changes
    // makes the failure mode visible. Always-on (not gated by debug toggle)
    // because state transitions are rare events and exactly the smoking
    // gun for "decoder shows nothing" reports.
    connect(m_source.get(), &QAudioSource::stateChanged, this,
            [this](QAudio::State state) {
                const char* stateStr =
                    (state == QAudio::ActiveState)    ? "Active"
                  : (state == QAudio::SuspendedState) ? "Suspended"
                  : (state == QAudio::StoppedState)   ? "Stopped"
                  : (state == QAudio::IdleState)      ? "Idle"
                  : "Unknown";
                const QAudio::Error err = m_source ? m_source->error()
                                                   : QAudio::NoError;
                const char* errStr =
                    (err == QAudio::NoError)        ? "NoError"
                  : (err == QAudio::OpenError)      ? "OpenError"
                  : (err == QAudio::IOError)        ? "IOError"
                  : (err == QAudio::UnderrunError)  ? "UnderrunError"
                  : (err == QAudio::FatalError)     ? "FatalError"
                  : "Unknown";
                DebugLogger::instance().log("CwDecoder",
                    QString("QAudioSource state -> %1 (error=%2) on '%3'")
                        .arg(stateStr, errStr, m_device.description()));
            });

    m_io = m_source->start();
    if (!m_io) {
        emit deviceError(QStringLiteral("QAudioSource::start() returned null I/O"));
        m_source.reset();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioCapture::onReadyRead);
    m_running = true;

    // Reset silence-detection state and arm a single-shot watchdog. The
    // callback fires once if no non-zero sample has been seen by then —
    // distinguishing "device is genuinely silent / muted at OS or driver
    // level" from "decoder is too slow / misconfigured." Logged
    // unconditionally because it's exactly the data needed to triage
    // "decoder shows nothing" reports without asking the operator to
    // enable per-component debug first.
    m_seenNonZeroSample = false;
    m_silenceWarned = false;
    m_chunkLogCount = 0;
    if (!m_silenceTimer) {
        m_silenceTimer = new QTimer(this);
        m_silenceTimer->setSingleShot(true);
        connect(m_silenceTimer, &QTimer::timeout, this, [this]() {
            if (!m_running || m_seenNonZeroSample || m_silenceWarned) return;
            m_silenceWarned = true;
            const QString msg = QStringLiteral(
                "No audio detected from '%1' after %2 ms. The capture stream "
                "is open but every sample so far is zero. Possible causes:\n"
                "  • Device input is muted or its level is at zero in OS sound settings\n"
                "  • Wrong device selected (USB CODECs often expose multiple endpoints)\n"
                "  • Another app has the device open in exclusive mode\n"
                "  • OS-level microphone privacy is blocking desktop apps "
                "(Windows: Settings → Privacy & security → Microphone → "
                "Let desktop apps access your microphone)")
                .arg(m_device.description())
                .arg(kSilenceDetectionMs);
            DebugLogger::instance().log("CwDecoder", msg);
            emit deviceError(msg);
        });
    }
    m_silenceTimer->start(kSilenceDetectionMs);

    DebugLogger::instance().log("CwDecoder",
        QString("Audio capture started on '%1' — negotiated format %2 Hz, "
                "%3 channel(s), sampleFormat=%4")
            .arg(m_device.description())
            .arg(m_format.sampleRate())
            .arg(m_format.channelCount())
            .arg(static_cast<int>(m_format.sampleFormat())));
    emit captureStarted();
    return true;
}

void AudioCapture::stop()
{
    if (!m_running) return;
    m_running = false;
    if (m_silenceTimer) m_silenceTimer->stop();
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

    // Convert each interleaved frame to a single int16 mono sample by
    // averaging every channel the device delivered. Earlier versions took
    // only channel 0, which silently failed on stereo USB CODECs that route
    // the rig's audio to the right channel only — averaging captures the
    // signal regardless of which side the operator wired up. NO resampling:
    // we operate at the device's native rate end-to-end, eliminating the
    // nearest-neighbor alias fold that was corrupting the CW detection
    // band with noise from above Nyquist/2.
    std::vector<int16_t> mono;
    mono.reserve(totalFrames);

    int peakAbs = 0;        // for the first-chunk diagnostic log
    const char* data = chunk.constData();
    for (size_t i = 0; i < totalFrames; ++i) {
        const char* frame = data + i * bytesPerSample * channels;
        int32_t accum = 0;  // sum of channels at int16 scale; divide at end
        for (int ch = 0; ch < channels; ++ch) {
            const char* sptr = frame + ch * bytesPerSample;
            int16_t sample = 0;
            switch (sfmt) {
            case QAudioFormat::Int16: {
                sample = *reinterpret_cast<const int16_t*>(sptr);
                break;
            }
            case QAudioFormat::Int32: {
                const int32_t v = *reinterpret_cast<const int32_t*>(sptr);
                sample = static_cast<int16_t>(v >> 16);
                break;
            }
            case QAudioFormat::Float: {
                float f = *reinterpret_cast<const float*>(sptr);
                if (f > 1.0f) f = 1.0f;
                if (f < -1.0f) f = -1.0f;
                sample = static_cast<int16_t>(f * 32767.0f);
                break;
            }
            case QAudioFormat::UInt8: {
                const uint8_t v = *reinterpret_cast<const uint8_t*>(sptr);
                sample = static_cast<int16_t>((static_cast<int>(v) - 128) * 256);
                break;
            }
            default:
                break;
            }
            accum += sample;
        }
        const int16_t mixed = static_cast<int16_t>(accum / channels);
        mono.push_back(mixed);
        const int absVal = std::abs(static_cast<int>(mixed));
        if (absVal > peakAbs) peakAbs = absVal;
    }

    if (peakAbs > 0) m_seenNonZeroSample = true;

    // First few chunks get an unconditional triage log — chunk size, frames,
    // and peak amplitude. If the operator reports the decoder as dead we can
    // see immediately whether the stream is delivering data and whether the
    // bytes are non-zero, without asking them to flip the per-component
    // debug toggle first. After the first three, when CW Decoder Debug is
    // enabled we drop a heartbeat every kHeartbeatChunks chunks (≈ 5 s at
    // 10 ms/chunk) so we can tell whether the audio path is still flowing
    // — the failure mode of "Windows WASAPI delivers an initial buffered
    // burst and then silently stops calling readyRead" is otherwise
    // invisible to the operator and to us.
    ++m_chunkLogCount;
    constexpr int kHeartbeatChunks = 500;
    if (m_chunkLogCount <= 3) {
        DebugLogger::instance().log("CwDecoder",
            QString("Audio chunk #%1 from '%2': %3 bytes, %4 frames, peak |sample|=%5")
                .arg(m_chunkLogCount)
                .arg(m_device.description())
                .arg(chunk.size())
                .arg(totalFrames)
                .arg(peakAbs));
    } else if (DebugLogger::instance().isCwDecoderDebugEnabled()
               && (m_chunkLogCount % kHeartbeatChunks) == 0) {
        DebugLogger::instance().log("CwDecoder",
            QString("Audio heartbeat from '%1': chunk #%2, %3 bytes, peak |sample|=%4")
                .arg(m_device.description())
                .arg(m_chunkLogCount)
                .arg(chunk.size())
                .arg(peakAbs));
    }

    m_ring.push(mono.data(), mono.size());
    emit audioBlockReady();
}

} // namespace clx::audio
