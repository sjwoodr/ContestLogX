/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/practiceAudioSource.h"
#include "audio/morseEncoder.h"
#include "debugLogger.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace clx::audio {

namespace {
constexpr int    kTickMs             = 10;       // matches decoder block cadence
constexpr int    kEnvRiseFallMs      = 5;        // key-click suppression ramp
constexpr int    kInterFragmentGapMs = 2500;
constexpr double kAmplitude          = 0.25;     // -12 dB, comfortable monitor volume
constexpr double kPi                 = 3.141592653589793;
constexpr double kTwoPi              = 6.283185307179586;
} // namespace

PracticeAudioSource::PracticeAudioSource(PracticeMode mode, QObject* parent)
    // Pass a null QAudioDevice to the base — m_ring will be sized using
    // kSampleRateHz * kRingBufferSeconds from the base ctor's fallback
    // branch. We then overwrite m_format with our fixed 48 kHz mono
    // int16 format before start().
    : AudioCapture(QAudioDevice{}, parent)
    , m_mode(mode)
{
    m_format.setSampleRate(m_sampleRate);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
    m_envSamples = (kEnvRiseFallMs * m_sampleRate) / 1000;
    m_samplesPerTick = (kTickMs * m_sampleRate) / 1000;
}

PracticeAudioSource::~PracticeAudioSource()
{
    stop();
}

bool PracticeAudioSource::start()
{
    if (m_running) return true;

    // Playback pipeline: default output device at 48 kHz mono int16.
    const QAudioDevice out = QMediaDevices::defaultAudioOutput();
    if (!out.isNull()) {
        QAudioFormat playFmt;
        playFmt.setSampleRate(m_sampleRate);
        playFmt.setChannelCount(1);
        playFmt.setSampleFormat(QAudioFormat::Int16);
        if (out.isFormatSupported(playFmt)) {
            m_sink = std::make_unique<QAudioSink>(out, playFmt, this);
            m_sinkIO = m_sink->start();   // push mode
        } else {
            if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
                DebugLogger::instance().log("CwDecoder",
                    "Practice: default output doesn't support 48 kHz int16 mono; "
                    "no audio playback but decoder feed will still work.");
            }
        }
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &PracticeAudioSource::onTick);
    m_timer->start();

    m_running = true;
    emit captureStarted();
    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder",
            QString("Practice audio source started (%1, %2 Hz, tone %3 Hz)")
                .arg(m_mode == PracticeMode::Contest ? "contest" : "ragchew")
                .arg(m_sampleRate).arg(m_toneHz));
    }
    return true;
}

void PracticeAudioSource::stop()
{
    if (!m_running) {
        // Make sure base state is consistent in case we were constructed
        // but never started.
        AudioCapture::stop();
        return;
    }
    m_running = false;

    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    m_sinkIO = nullptr;
    m_queue.clear();
    m_currentRemaining = 0;
    m_currentToneOn = false;
    m_phase = 0.0;

    emit captureStopped();
    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder", "Practice audio source stopped");
    }
}

void PracticeAudioSource::onTick()
{
    if (!m_running) return;

    std::vector<int16_t> block(m_samplesPerTick, 0);
    fillBlock(block.data(), m_samplesPerTick);

    // Feed the decoder through the same ring buffer real audio uses.
    m_ring.push(block.data(), block.size());
    emit audioBlockReady();

    // Play the audio for the operator to copy in their head.
    if (m_sinkIO) {
        m_sinkIO->write(reinterpret_cast<const char*>(block.data()),
                        block.size() * sizeof(int16_t));
    }
}

int PracticeAudioSource::dotUnitSamples(int wpm) const
{
    const int wpmClamped = qBound(5, wpm, 60);
    const double dotMs = 1200.0 / wpmClamped;
    return static_cast<int>(dotMs * m_sampleRate / 1000.0);
}

void PracticeAudioSource::enqueueChar(QChar c, int unitSamples)
{
    if (c == QLatin1Char(' ')) {
        // Inter-word gap = 7 units total. The previous char already queued
        // a 3-unit inter-char gap at its tail; subtract that so we don't
        // stack gaps.
        m_queue.push_back({false, qMax(0, unitSamples * 7 - unitSamples * 3)});
        return;
    }
    const QString pattern = encodeChar(c);
    if (pattern.isEmpty()) return;
    for (int i = 0; i < pattern.size(); ++i) {
        const int units = (pattern[i] == QLatin1Char('.')) ? 1 : 3;
        m_queue.push_back({true, unitSamples * units});
        if (i + 1 < pattern.size()) {
            m_queue.push_back({false, unitSamples});  // intra-char gap
        }
    }
    // Inter-char gap at end of char (3 units).
    m_queue.push_back({false, unitSamples * 3});
}

void PracticeAudioSource::enqueueFragment(const QString& text)
{
    const int wpm = m_wpmProvider ? m_wpmProvider() : 25;
    const int u = dotUnitSamples(wpm);
    for (const QChar c : text) {
        enqueueChar(c, u);
    }
    // Pad with a long silence between fragments so the operator has time
    // to read the decoder output before the next transmission begins.
    const int gapSamples = (kInterFragmentGapMs * m_sampleRate) / 1000;
    m_queue.push_back({false, gapSamples});
}

void PracticeAudioSource::fillBlock(int16_t* out, int count)
{
    int written = 0;
    const double phaseInc = kTwoPi * m_toneHz / m_sampleRate;
    while (written < count) {
        if (m_currentRemaining <= 0) {
            // Load next element. Pull a fresh fragment if the queue is empty.
            if (m_queue.empty()) {
                enqueueFragment(m_generator.nextFragment(m_mode));
                if (m_queue.empty()) {
                    // Content generator produced nothing encodable — fill
                    // the rest of the block with silence and bail.
                    std::memset(out + written, 0,
                                (count - written) * sizeof(int16_t));
                    return;
                }
            }
            const Element e = m_queue.front();
            m_queue.pop_front();
            m_currentToneOn = e.toneOn;
            m_currentRemaining = e.samples;
            m_currentTotal = e.samples;
            // Reset phase at each tone onset so dits/dahs start at zero
            // amplitude — combined with the raised-cosine envelope this
            // suppresses audible key clicks.
            if (m_currentToneOn) m_phase = 0.0;
        }

        const int n = std::min(count - written, m_currentRemaining);
        if (m_currentToneOn) {
            const int rise = std::min(m_envSamples, m_currentTotal / 2);
            const int fall = rise;
            const int elementPos = m_currentTotal - m_currentRemaining;
            for (int i = 0; i < n; ++i) {
                const int pos = elementPos + i;
                double env = 1.0;
                if (pos < rise) {
                    env = 0.5 * (1.0 - std::cos(kPi * pos / rise));
                } else if (pos > m_currentTotal - fall) {
                    const int tailPos = m_currentTotal - pos;
                    env = 0.5 * (1.0 - std::cos(kPi * tailPos / fall));
                }
                const double s = std::sin(m_phase) * kAmplitude * env;
                out[written + i] = static_cast<int16_t>(s * 32767.0);
                m_phase += phaseInc;
                if (m_phase >= kTwoPi) m_phase -= kTwoPi;
            }
        } else {
            std::memset(out + written, 0, n * sizeof(int16_t));
        }
        written += n;
        m_currentRemaining -= n;
    }
}

} // namespace clx::audio
