/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * PracticeAudioSource - virtual audio source that synthesizes CW training
 * material and feeds it into both the decoder pipeline (via the inherited
 * AudioCapture ring buffer) and the default playback device (so the
 * operator can hear and copy it in their head). Fills the same role as a
 * real QAudioSource in the decoder worker, which means the decoder sees
 * and exercises the same DSP path against synthesized input.
 */

#ifndef AUDIO_PRACTICEAUDIOSOURCE_H
#define AUDIO_PRACTICEAUDIOSOURCE_H

#include "audio/audioCapture.h"
#include "audio/practiceContentGenerator.h"

#include <QAudioSink>
#include <QIODevice>
#include <QTimer>
#include <QVector>
#include <deque>
#include <functional>
#include <memory>

class ContestEngine;

namespace clx::audio {

class PracticeAudioSource : public AudioCapture {
    Q_OBJECT
public:
    explicit PracticeAudioSource(PracticeMode mode, QObject* parent = nullptr);
    ~PracticeAudioSource() override;

    bool start() override;
    void stop() override;

    void setContestEngine(const ContestEngine* engine) {
        m_generator.setContestEngine(engine);
    }

    // Provider is called each time a new fragment begins - this way
    // mid-session WPM changes in the CW console take effect on the next
    // fragment without any signal/slot plumbing.
    void setWpmProvider(std::function<int()> provider) {
        m_wpmProvider = std::move(provider);
    }

private slots:
    void onTick();

private:
    // Synthesized queue entry - contiguous tone-on or tone-off chunk.
    struct Element {
        bool  toneOn;
        int   samples;
    };

    // One voice = one simulated CW station. The primary voice uses the
    // operator's configured mode (Ragchew or Contest); QRM voices are
    // always Ragchew chatter at reduced amplitude and offset tone
    // frequency so they show up in the decoder's edge bins without
    // dominating the primary bin's decode.
    struct Voice {
        PracticeMode mode      = PracticeMode::Ragchew;
        double       toneHz    = 700.0;
        double       amplitude = 0.25;
        int          wpmOffset = 0;    // added to primary WPM, clamped at use

        std::deque<Element> queue;
        bool   currentToneOn    = false;
        int    currentRemaining = 0;
        int    currentTotal     = 0;
        double phase            = 0.0;
    };

    void enqueueFragmentFor(Voice& v, const QString& text);
    void enqueueCharFor(Voice& v, QChar c, int unitSamples);
    int  dotUnitSamples(int wpm) const;   // 1200 / WPM ms → samples at m_sampleRate
    void fillBlock(int16_t* out, int count);
    double sampleFromVoice(Voice& v, double phaseInc);

    PracticeMode m_mode;
    PracticeContentGenerator m_generator;
    std::function<int()> m_wpmProvider;

    // Voice[0] is the primary (center frequency, full amplitude); additional
    // entries are QRM stations at asymmetric offsets and reduced amplitude.
    QVector<Voice> m_voices;

    const int m_sampleRate = 48000;   // fixed - matches typical output devices
    int m_envSamples = 0;             // rise/fall ramp length, ~5 ms

    // Playback path (what the operator hears on their speakers/headphones)
    std::unique_ptr<QAudioSink> m_sink;
    QIODevice* m_sinkIO = nullptr;

    // Real-time pacing - a 10 ms tick generates 480 samples at 48 kHz
    QTimer* m_timer = nullptr;
    int m_samplesPerTick = 0;
};

} // namespace clx::audio

#endif // AUDIO_PRACTICEAUDIOSOURCE_H
