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
    // Bootstrap: need enough boundary-gap samples for the largest-jump
    // analysis to be meaningful.
    if (m_recentBoundaryGaps.size() < 6) return fallback;

    std::vector<int> sorted(m_recentBoundaryGaps.begin(), m_recentBoundaryGaps.end());
    std::sort(sorted.begin(), sorted.end());

    // Find the largest consecutive jump in the sorted gap list.
    int bestJumpSize = 0;
    int bestJumpMidpoint = 0;
    for (size_t i = 1; i < sorted.size(); ++i) {
        const int jump = sorted[i] - sorted[i - 1];
        if (jump > bestJumpSize) {
            bestJumpSize = jump;
            bestJumpMidpoint = (sorted[i] + sorted[i - 1]) / 2;
        }
    }
    // Significance check: the jump must be larger than 1.5 dot-units to
    // count as a real char-vs-word split. If gaps are clustered uniformly
    // (compressed contest CW with no real word spacing), there's no jump,
    // and falling back to the fixed threshold avoids spurious word breaks.
    if (bestJumpSize >= (dotBaselineMs * 3 / 2)) {
        return bestJumpMidpoint;
    }
    return fallback;
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

    // Record element duration in the rolling window BEFORE updating the
    // estimate — the percentile-based estimator uses the latest sample.
    m_recentElementMs.push_back(durationMs);
    while (m_recentElementMs.size() > 20) m_recentElementMs.pop_front();

    // Update estimate after recording. This also refreshes lockState.
    updateWpmEstimate();

    // Classify the current element using the REFRESHED estimate. Anything
    // below 2× the current dot-length estimate is a dot; anything above is
    // a dash. Since the estimator tracks the lower quartile of all recent
    // elements, it's anchored to the true dot length even when dashes
    // dominate the recent content.
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
    double normMag = std::sqrt(std::max(0.0, mag2)) / kScale;
    if (normMag > 1.0) normMag = 1.0;

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
