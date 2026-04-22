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
constexpr double kPrimaryAmplitude   = 0.25;     // -12 dB, comfortable monitor volume
constexpr double kPi                 = 3.141592653589793;
constexpr double kTwoPi              = 6.283185307179586;

// Asymmetric QRM offsets chosen to land in the decoder's edge bins when
// the operator uses the default 700 Hz center with 6 bins (passband
// 550-850 Hz, bin edges at 575/625/675/725/775/825 Hz). Offsets are
// prime-ish fractions apart so adjacent-bin bleed differs on each side.
constexpr double kQrm1ToneOffsetHz = +130.0;  // above primary
constexpr double kQrm2ToneOffsetHz = -110.0;  // below primary
constexpr double kQrm1Amplitude    = 0.09;    // ~-9 dB below primary
constexpr double kQrm2Amplitude    = 0.07;    // slightly weaker still
constexpr int    kQrm1WpmOffset    = -3;      // slower than primary
constexpr int    kQrm2WpmOffset    = +2;      // faster than primary

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

    // Build the voice list. Ragchew mode is a single clean signal — the
    // operator is learning to copy pure CW, not fight QRM. Contest mode
    // adds two QRM stations (separate simulated ops running rag-chew
    // chatter at different tone frequencies, lower amplitude, and
    // slightly different WPM) — mimicking what a contest band sounds
    // like when you're trying to copy the exchange right on your
    // frequency with adjacent-channel interference.
    Voice primary;
    primary.mode      = mode;
    primary.toneHz    = 700.0;
    primary.amplitude = kPrimaryAmplitude;
    primary.wpmOffset = 0;
    m_voices.append(primary);

    if (mode == PracticeMode::Contest) {
        Voice qrm1;
        qrm1.mode      = PracticeMode::Ragchew;
        qrm1.toneHz    = 700.0 + kQrm1ToneOffsetHz;
        qrm1.amplitude = kQrm1Amplitude;
        qrm1.wpmOffset = kQrm1WpmOffset;
        m_voices.append(qrm1);

        Voice qrm2;
        qrm2.mode      = PracticeMode::Ragchew;
        qrm2.toneHz    = 700.0 + kQrm2ToneOffsetHz;
        qrm2.amplitude = kQrm2Amplitude;
        qrm2.wpmOffset = kQrm2WpmOffset;
        m_voices.append(qrm2);
    }
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
            QString("Practice audio source started (%1, %2 Hz primary, %3 voices)")
                .arg(m_mode == PracticeMode::Contest ? "contest" : "ragchew")
                .arg(m_voices.isEmpty() ? 0.0 : m_voices.first().toneHz)
                .arg(m_voices.size()));
    }
    return true;
}

void PracticeAudioSource::stop()
{
    if (!m_running) {
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
    for (Voice& v : m_voices) {
        v.queue.clear();
        v.currentRemaining = 0;
        v.currentToneOn = false;
        v.phase = 0.0;
    }

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

    m_ring.push(block.data(), block.size());
    emit audioBlockReady();

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

void PracticeAudioSource::enqueueCharFor(Voice& v, QChar c, int unitSamples)
{
    if (c == QLatin1Char(' ')) {
        v.queue.push_back({false, qMax(0, unitSamples * 7 - unitSamples * 3)});
        return;
    }
    const QString pattern = encodeChar(c);
    if (pattern.isEmpty()) return;
    for (int i = 0; i < pattern.size(); ++i) {
        const int units = (pattern[i] == QLatin1Char('.')) ? 1 : 3;
        v.queue.push_back({true, unitSamples * units});
        if (i + 1 < pattern.size()) {
            v.queue.push_back({false, unitSamples});  // intra-char gap
        }
    }
    v.queue.push_back({false, unitSamples * 3});  // inter-char gap
}

void PracticeAudioSource::enqueueFragmentFor(Voice& v, const QString& text)
{
    const int primaryWpm = m_wpmProvider ? m_wpmProvider() : 25;
    const int voiceWpm = primaryWpm + v.wpmOffset;
    const int u = dotUnitSamples(voiceWpm);
    for (const QChar c : text) {
        enqueueCharFor(v, c, u);
    }
    const int gapSamples = (kInterFragmentGapMs * m_sampleRate) / 1000;
    v.queue.push_back({false, gapSamples});
}

double PracticeAudioSource::sampleFromVoice(Voice& v, double phaseInc)
{
    // Advance to next element if current one is exhausted. A voice may
    // need to skip multiple zero-length elements or fetch a new fragment
    // here; the while-loop handles that cleanly.
    while (v.currentRemaining <= 0) {
        if (v.queue.empty()) {
            enqueueFragmentFor(v, m_generator.nextFragment(v.mode));
            if (v.queue.empty()) return 0.0;  // generator emptied the well
        }
        const Element e = v.queue.front();
        v.queue.pop_front();
        v.currentToneOn = e.toneOn;
        v.currentRemaining = e.samples;
        v.currentTotal = e.samples;
        if (v.currentToneOn) v.phase = 0.0;
    }

    double s = 0.0;
    if (v.currentToneOn) {
        const int rise = std::min(m_envSamples, v.currentTotal / 2);
        const int fall = rise;
        const int pos = v.currentTotal - v.currentRemaining;
        double env = 1.0;
        if (pos < rise) {
            env = 0.5 * (1.0 - std::cos(kPi * pos / rise));
        } else if (pos > v.currentTotal - fall) {
            const int tailPos = v.currentTotal - pos;
            env = 0.5 * (1.0 - std::cos(kPi * tailPos / fall));
        }
        s = std::sin(v.phase) * v.amplitude * env;
        v.phase += phaseInc;
        if (v.phase >= kTwoPi) v.phase -= kTwoPi;
    }
    v.currentRemaining -= 1;
    return s;
}

void PracticeAudioSource::fillBlock(int16_t* out, int count)
{
    // Per-sample mixing across all voices. Each voice has its own phase,
    // element queue, and content stream, so they drift in and out of sync
    // naturally — just like real operators on a crowded band.
    //
    // We cap total summed amplitude at ~0.9 (hard clip fallback at 32767)
    // because simultaneous tone peaks across voices can briefly exceed
    // unity. In practice with our 0.25/0.09/0.07 amplitudes the worst-case
    // sum is 0.41 so clipping never engages, but the guard keeps the
    // output well-behaved if anyone ever turns up a voice's amplitude.
    std::vector<double> phaseInc(m_voices.size());
    for (int i = 0; i < m_voices.size(); ++i) {
        phaseInc[i] = kTwoPi * m_voices[i].toneHz / m_sampleRate;
    }
    for (int i = 0; i < count; ++i) {
        double mixed = 0.0;
        for (int v = 0; v < m_voices.size(); ++v) {
            mixed += sampleFromVoice(m_voices[v], phaseInc[v]);
        }
        if (mixed > 0.95) mixed = 0.95;
        if (mixed < -0.95) mixed = -0.95;
        out[i] = static_cast<int16_t>(mixed * 32767.0);
    }
}

} // namespace clx::audio
