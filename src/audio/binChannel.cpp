/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/binChannel.h"
#include "audio/morseTable.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace clx::audio {

BinChannel::BinChannel(int binIndex, double centerFreqHz, int sampleRateHz,
                       int wpmMin, int wpmMax)
    : m_binIndex(binIndex)
    , m_centerFreqHz(centerFreqHz)
    , m_sampleRateHz(sampleRateHz)
    , m_wpmMin(wpmMin)
    , m_wpmMax(wpmMax)
{
    const double omega = 2.0 * M_PI * centerFreqHz / sampleRateHz;
    m_coeff = 2.0 * std::cos(omega);
}

void BinChannel::reset()
{
    m_sPrev = 0.0;
    m_sPrev2 = 0.0;
    m_toneActive = false;
    m_elementStartMs = 0;
    m_elapsedAudioMs = 0;
    m_pendingBlocks = 0;
    m_morseBuffer.clear();
    m_recentElementMs.clear();
    m_recentBoundaryGaps.clear();
    m_recentMagnitudes.clear();
    m_currentWpm = 0;
    m_lockState = LockState::NoLock;
    m_textBuffer.clear();
}

void BinChannel::setWpmBounds(int wpmMin, int wpmMax)
{
    m_wpmMin = wpmMin;
    m_wpmMax = wpmMax;
}

void BinChannel::clearTextBuffer()
{
    m_textBuffer.clear();
}

int BinChannel::currentDotEstimateMs() const
{
    // Bootstrap: not enough samples yet — return a baseline derived from
    // the SLOWEST allowed WPM (wpmMin). This gives the largest possible
    // dot length, so the classifier's dot-vs-dash threshold (2×dot) is
    // permissive and even slow signals' dots get counted as dots
    // initially. The estimator refines rapidly as elements accumulate.
    if (m_recentElementMs.size() < 4) {
        const int wpm = qMax(3, m_wpmMin);
        return 1200 / wpm;
    }
    // 25th percentile of recent element durations = dot-length estimate.
    // Dots are ~1 unit, dashes ~3 units; in a typical mix, the lower
    // quartile lands squarely inside the dot cluster regardless of the
    // dot:dash ratio in the operator's sent content.
    std::vector<int> sorted(m_recentElementMs.begin(), m_recentElementMs.end());
    std::sort(sorted.begin(), sorted.end());
    const size_t q = sorted.size() / 4;  // 25th percentile
    return sorted[q];
}

int BinChannel::wordGapThresholdMs(int dotBaselineMs) const
{
    const int fallback = dotBaselineMs * 4;
    // Never let the word-gap threshold drop below 3 dot-units; below that
    // it would overlap the 2-dot-unit character-gap threshold and produce
    // a word split on every inter-character boundary.
    const int floorMs = dotBaselineMs * 3;

    // Bootstrap: need enough boundary-gap samples for any statistical
    // analysis to be meaningful. Use the fixed fallback until then.
    if (m_recentBoundaryGaps.size() < 6) return qMax(fallback, floorMs);

    std::vector<int> sorted(m_recentBoundaryGaps.begin(), m_recentBoundaryGaps.end());
    std::sort(sorted.begin(), sorted.end());

    // -------- Trigger 1: largest-jump analysis (bimodal distributions) --
    // Find the largest consecutive jump in the sorted gap list. If it's
    // significant (≥ 1.5 dot-units), the midpoint of that jump is an
    // operator-style-adaptive threshold. Otherwise fall back.
    int bestJumpSize = 0;
    int bestJumpMidpoint = 0;
    for (size_t i = 1; i < sorted.size(); ++i) {
        const int jump = sorted[i] - sorted[i - 1];
        if (jump > bestJumpSize) {
            bestJumpSize = jump;
            bestJumpMidpoint = (sorted[i] + sorted[i - 1]) / 2;
        }
    }
    const int primary = (bestJumpSize >= (dotBaselineMs * 3 / 2))
                            ? bestJumpMidpoint
                            : fallback;

    // -------- Trigger 2: median-multiplier outlier detection -----------
    // Any gap ≥ 2× the median of recent gaps is likely a word boundary,
    // even when the overall distribution is too unimodal for largest-jump
    // analysis to find a clean split. This catches occasional word
    // outliers in otherwise-compressed sending.
    const int median = sorted[sorted.size() / 2];
    const int secondary = median * 2;

    // Effective threshold is the LOWER of the two — OR semantics: a gap
    // that exceeds EITHER trigger counts as a word boundary. The floor
    // keeps us above the character-gap threshold at all times.
    const int effective = qMin(primary, secondary);
    return qMax(effective, floorMs);
}

void BinChannel::updateWpmEstimate()
{
    const int dotMs = currentDotEstimateMs();
    const int wpm = (dotMs > 0) ? static_cast<int>(1200.0 / dotMs + 0.5) : 0;
    if (wpm >= m_wpmMin && wpm <= m_wpmMax && m_recentElementMs.size() >= 4) {
        m_currentWpm = wpm;
        m_lockState = LockState::Locked;
    } else {
        m_currentWpm = 0;
        m_lockState = LockState::NoLock;
    }
}

void BinChannel::closeElement(int durationMs, qint64 timestampMs,
                              QList<CharEvent>& out)
{
    (void)out;
    Q_UNUSED(timestampMs);
    if (durationMs <= 0) return;

    // Decide whether to feed this element into the rolling window that
    // drives the percentile-based dot-length estimator.
    //
    // During bootstrap (NoLock) we accept every element, since the
    // estimator needs to converge and can't be trusted yet to decide
    // what's an outlier.
    //
    // Once locked, filter outliers that would destabilize the estimate:
    // a brief beat-fragmentation burst can split a real dash into two
    // short pieces; without filtering, those short pieces drop the 25th
    // percentile, which shrinks the classifier threshold, which causes
    // more real elements to fragment — a self-reinforcing runaway that
    // carries a stable 24 WPM lock up to a bogus 40 WPM within seconds.
    // The [D/3, D×6] window still admits genuine speed changes up to
    // about 3× faster or 6× slower before requiring a re-lock, so the
    // estimator remains adaptive without being fragile.
    const bool acceptSample = (m_lockState != LockState::Locked) ||
        (durationMs >= currentDotEstimateMs() / 3 &&
         durationMs <= currentDotEstimateMs() * 6);
    if (acceptSample) {
        m_recentElementMs.push_back(durationMs);
        while (m_recentElementMs.size() > 20) m_recentElementMs.pop_front();
        updateWpmEstimate();
    }

    // Classify the current element using the (possibly unchanged)
    // estimate. Even rejected-from-window elements still get classified
    // so character decoding proceeds — only the estimator is protected
    // from outlier contamination.
    const int dotBaseline = currentDotEstimateMs();
    if (durationMs < dotBaseline * 2) {
        m_morseBuffer.append('.');
    } else {
        m_morseBuffer.append('-');
    }
}

void BinChannel::closeCharacter(qint64 timestampMs, QList<CharEvent>& out)
{
    if (m_morseBuffer.isEmpty()) return;

    const QString decoded = morseLookup(m_morseBuffer.toStdString());
    m_morseBuffer.clear();
    if (!decoded.isEmpty()) {
        // If it's a single printable char append directly; if a prosign expansion
        // like "<SK>", append full text.
        m_textBuffer.append(decoded);
        if (m_textBuffer.size() > kTextBufferCapChars) {
            m_textBuffer.remove(0, m_textBuffer.size() - kTextBufferCapChars);
        }
        for (QChar c : decoded) {
            out.append({m_binIndex, c, timestampMs});
        }
    }
}

QList<CharEvent> BinChannel::processBlock(const int16_t* samples, int count,
                                          qint64 timestampMs,
                                          float squelchThreshold,
                                          bool muted)
{
    QList<CharEvent> out;

    // Goertzel recursion over the block.
    m_sPrev = 0.0;
    m_sPrev2 = 0.0;
    for (int i = 0; i < count; ++i) {
        const double x = static_cast<double>(samples[i]);
        const double s = x + m_coeff * m_sPrev - m_sPrev2;
        m_sPrev2 = m_sPrev;
        m_sPrev = s;
    }
    // Magnitude squared (Goertzel second-order output).
    const double mag2 = (m_sPrev * m_sPrev + m_sPrev2 * m_sPrev2
                        - m_coeff * m_sPrev * m_sPrev2);
    // Normalize so typical receiver audio at ~10% full-scale produces
    // normMag in the 0.25–0.5 range and a loud signal saturates near 1.0.
    // This gives the operator's 0–100% squelch slider a useful dynamic
    // range: noise-floor around 0.05–0.15, comfortable operating around
    // 0.2–0.4, strong signals always pass. Clamped to [0,1].
    constexpr double kScale = static_cast<double>(kBlockSamples) * 6553.6;  // 5× more sensitive
    double rawMag = std::sqrt(std::max(0.0, mag2)) / kScale;
    if (rawMag > 1.0) rawMag = 1.0;

    // 4-block (≈40 ms) moving-average smoothing. Off-center signals (e.g.,
    // a 700 Hz tone landing between the 650 Hz and 750 Hz bins) produce
    // Goertzel magnitude oscillation at the bin-offset frequency. For a
    // 50 Hz offset the beat period is 20 ms; a moving average of exactly
    // 2 periods (40 ms) fully cancels. Longer windows (60 ms) cancel
    // better on a broader range of offsets but pull peak smoothed
    // magnitude below typical squelch on marginal-amplitude signals —
    // 40 ms is the empirical sweet spot. Legitimate CW elements at
    // ≤ 45 WPM (dot ≥ 27 ms) still peak above squelch after smoothing;
    // element durations are uniformly stretched by ~1 block on each end
    // but the dot:dash ratio is preserved so classification still works.
    m_recentMagnitudes.push_back(rawMag);
    while (m_recentMagnitudes.size() > 4) m_recentMagnitudes.pop_front();
    double normMag = 0.0;
    for (double m : m_recentMagnitudes) normMag += m;
    normMag /= static_cast<double>(m_recentMagnitudes.size());

    const int blockMs = (count * 1000) / m_sampleRateHz;
    m_elapsedAudioMs += blockMs;

    // Schmitt-trigger hysteresis: a tone is detected with a high threshold
    // (the operator's squelch setting) but only released when the magnitude
    // drops to 70% of that. This prevents brief mid-element dips from
    // fragmenting a dash while not inflating legitimate element durations
    // too much (too-sticky hysteresis stretches dashes enough that the WPM
    // estimator biases low).
    const float offThreshold = squelchThreshold * 0.7f;
    bool toneDetected;
    if (m_toneActive) {
        toneDetected = normMag > offThreshold;       // sticky — hold on until big drop
    } else {
        toneDetected = normMag > squelchThreshold;   // need full threshold to turn on
    }

    // Commit transition immediately (hysteresis handles the jitter cleanup
    // that a multi-block debounce was trying to solve).
    bool committedTransition = (toneDetected != m_toneActive);
    m_pendingBlocks = 0;  // retained for potential future multi-block smoothing

    if (committedTransition) {
        const int runMs = static_cast<int>(m_elapsedAudioMs - m_elementStartMs);
        if (m_toneActive) {
            // Tone just went off → close an element.
            if (!muted) {
                closeElement(runMs, timestampMs, out);
            }
        } else {
            // Tone just turned on → classify the preceding gap (intra-element
            // vs. character boundary vs. word boundary).
            if (!muted) {
                const int dotBaseline = currentDotEstimateMs();
                const int charThreshold = dotBaseline * 2;
                const int wordThreshold = wordGapThresholdMs(dotBaseline);

                if (runMs >= charThreshold) {
                    // Record this boundary gap for adaptive-threshold history
                    // (only real boundary gaps, never intra-element).
                    m_recentBoundaryGaps.push_back(runMs);
                    while (m_recentBoundaryGaps.size() > 16) {
                        m_recentBoundaryGaps.pop_front();
                    }

                    if (runMs >= wordThreshold) {
                        // Word boundary: close character + emit a visible space.
                        closeCharacter(timestampMs, out);
                        m_textBuffer.append(' ');
                        if (m_textBuffer.size() > kTextBufferCapChars) {
                            m_textBuffer.remove(0, m_textBuffer.size() - kTextBufferCapChars);
                        }
                        out.append({m_binIndex, QChar(' '), timestampMs});
                    } else {
                        // Character boundary only — no visible space.
                        closeCharacter(timestampMs, out);
                    }
                }
                // else: intra-element gap; keep accumulating current char.
            }
        }
        m_toneActive = toneDetected;
        m_elementStartMs = m_elapsedAudioMs;
    } else if (!m_toneActive) {
        // Continuously off — no-op until next transition.
    }

    return out;
}

} // namespace clx::audio
