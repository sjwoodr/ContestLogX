/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * CwDecoder — owns an array of BinChannels and applies them to incoming
 * audio blocks (SPEC-005). Pure C++ — no Qt threading; host code on a
 * QThread wraps it.
 */

#ifndef AUDIO_CWDECODER_H
#define AUDIO_CWDECODER_H

#include <QList>
#include <QVector>
#include <memory>

#include "audio/audioTypes.h"
#include "audio/binChannel.h"

namespace clx::audio {

class CwDecoder {
public:
    explicit CwDecoder(int sampleRateHz = kSampleRateHz);

    // Configure bins. Safe to call at any time; rebuilds bins and clears state.
    // Returns true on success; false if parameters violate validation rules.
    // `sampleRateHz` must be the actual capture rate (e.g. 44100, 48000) so
    // Goertzel coefficients align with the audio we'll be fed. If
    // `sampleRateHz <= 0`, the decoder's current stored rate is kept.
    bool configure(int passbandLowHz, int passbandHighHz, int binCount,
                   int wpmMin, int wpmMax, float squelchThreshold,
                   int sampleRateHz = 0);

    // Accessors
    int binCount() const { return static_cast<int>(m_bins.size()); }
    double binCenterFreq(int binIndex) const;
    int binCurrentWpm(int binIndex) const;
    LockState binLockState(int binIndex) const;
    QString binTextBuffer(int binIndex) const;
    QList<double> binCenterFrequencies() const;
    int sampleRateHz() const { return m_sampleRateHz; }

    // Diagnostic accessors used by the worker's periodic stats logger.
    double binLastMagnitude(int binIndex) const {
        return (binIndex < 0 || binIndex >= static_cast<int>(m_bins.size()))
            ? 0.0 : m_bins[binIndex]->lastNormalizedMagnitude();
    }
    bool binToneActive(int binIndex) const {
        return (binIndex >= 0 && binIndex < static_cast<int>(m_bins.size()))
            && m_bins[binIndex]->toneActive();
    }
    float squelch() const { return m_squelch; }

    // Runtime tuning
    void setSquelch(float threshold) { m_squelch = threshold; }
    void setWpmBounds(int wpmMin, int wpmMax);
    void setWordGapMultiplier(float multiplier);
    void setMuted(bool muted) { m_muted = muted; }
    bool isMuted() const { return m_muted; }

    // Clear the on-screen text buffers in every bin; DSP state preserved (FR-012).
    void clearAllBuffers();

    // Process one audio block across all bins. Returns decoded char events.
    QList<CharEvent> processBlock(const int16_t* samples, int count,
                                  qint64 timestampMs);

private:
    int m_sampleRateHz;
    std::vector<std::unique_ptr<BinChannel>> m_bins;
    int m_passbandLowHz  = kDefaultPassbandLowHz;
    int m_passbandHighHz = kDefaultPassbandHighHz;
    int m_wpmMin = kDefaultWpmMin;
    int m_wpmMax = kDefaultWpmMax;
    float m_squelch = kDefaultSquelch;
    bool m_muted = false;
};

} // namespace clx::audio

#endif // AUDIO_CWDECODER_H
