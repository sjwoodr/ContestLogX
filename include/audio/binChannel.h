/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * BinChannel — a single-frequency Goertzel tone detector + dot/dash classifier
 * + rolling-median WPM estimator + Morse decoder (SPEC-005). One instance per
 * decoder bin (row in the UI).
 */

#ifndef AUDIO_BINCHANNEL_H
#define AUDIO_BINCHANNEL_H

#include <QString>
#include <QChar>
#include <QList>
#include <deque>
#include <cstdint>
#include "audio/audioTypes.h"

namespace clx::audio {

struct CharEvent {
    int binIndex;
    QChar ch;
    qint64 timestampMs;
};

class BinChannel {
public:
    // Construct with the bin's center frequency and the operator's WPM bounds.
    BinChannel(int binIndex, double centerFreqHz, int sampleRateHz, int wpmMin, int wpmMax);

    // Reset the DSP state (keeps the index and coefficient). Used on reconfigure.
    void reset();

    // Process one 10 ms block of 80 samples. Returns decoded character events
    // produced during this block (typically zero or one).
    // squelchThreshold is in [0.0, 1.0] normalized against int16 full-scale.
    // muted gates character emission but still runs the Goertzel recursion so
    // state does not go stale.
    QList<CharEvent> processBlock(const int16_t* samples, int count,
                                  qint64 timestampMs, float squelchThreshold,
                                  bool muted);

    // Live per-bin state.
    int binIndex() const { return m_binIndex; }
    double centerFreqHz() const { return m_centerFreqHz; }
    int currentWpm() const { return m_currentWpm; }
    LockState lockState() const { return m_lockState; }
    const QString& textBuffer() const { return m_textBuffer; }

    // Update the bin's WPM bounds (called when the operator changes the
    // bounding range). Does not reset the rolling window.
    void setWpmBounds(int wpmMin, int wpmMax);

    // Clear ONLY the scrolling text buffer. Preserves Goertzel state,
    // WPM estimator, and the in-progress Morse element buffer — decoding
    // continues without a re-convergence penalty (FR-012).
    void clearTextBuffer();

private:
    void closeElement(int durationMs, qint64 timestampMs, QList<CharEvent>& out);
    void closeCharacter(qint64 timestampMs, QList<CharEvent>& out);
    void updateWpmEstimate();

    // Current dot-length estimate (ms) based on recent-element distribution.
    // Falls back to a bootstrap value derived from wpmMin when not enough
    // samples have been collected.
    int currentDotEstimateMs() const;

    // Adaptive word-gap threshold (ms). Uses largest-jump analysis on
    // m_recentBoundaryGaps; falls back to dotBaseline*4 during bootstrap
    // or when the jump is not significant.
    int wordGapThresholdMs(int dotBaselineMs) const;

    int m_binIndex;
    double m_centerFreqHz;
    int m_sampleRateHz;

    // Goertzel state — reset at each block boundary.
    double m_coeff = 0.0;
    double m_sPrev = 0.0;
    double m_sPrev2 = 0.0;

    // Tone-edge tracking.
    bool m_toneActive = false;
    qint64 m_elementStartMs = 0;    // start of current tone-on or tone-off run
    qint64 m_elapsedAudioMs = 0;    // monotonic counter of processed audio time
    // Debounce: require N consecutive blocks of the same tone-detect result
    // before committing a transition. At 10 ms/block, a 2-block debounce
    // filters out <20 ms jitter without affecting dots (≥~48 ms at 25 WPM).
    int m_pendingBlocks = 0;        // how many blocks disagreeing with m_toneActive

    // Morse element accumulation.
    QString m_morseBuffer;           // ".-" etc.

    // Rolling window of RECENT ELEMENT DURATIONS (dot or dash, ms). The
    // 25th percentile of this window is the dot-length estimate — this is
    // robust even when the classifier is initially wrong, because dots are
    // always shorter than dashes (roughly 1:3), so the lower quartile
    // reliably points at the dot cluster once a few elements are seen.
    std::deque<int> m_recentElementMs;

    // Rolling window of RECENT BOUNDARY GAPS (ms) — only gaps ≥ 2 dot-units
    // (character or word boundaries). Used for adaptive word-gap
    // classification via "largest-jump" analysis.
    std::deque<int> m_recentBoundaryGaps;

    // Small rolling window of raw Goertzel magnitudes. Averaging across a
    // few blocks (~30 ms) smooths the ~20 ms beating pattern that a
    // Goertzel detector exhibits when the actual signal frequency is
    // offset from the bin center (common for signals that land between
    // bins). Legitimate CW elements at typical speeds are well above
    // 30 ms, so the smoothing does not blur them.
    std::deque<double> m_recentMagnitudes;

    int m_currentWpm = 0;            // 0 = no lock
    LockState m_lockState = LockState::NoLock;

    int m_wpmMin;
    int m_wpmMax;

    // Scrolling decoded text for this bin.
    QString m_textBuffer;
};

} // namespace clx::audio

#endif // AUDIO_BINCHANNEL_H
