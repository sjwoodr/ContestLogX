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
    m_currentToneOnPeak = 0.0;
    m_morseBuffer.clear();
    m_recentElementMs.clear();
    m_recentBoundaryGaps.clear();
    m_recentMagnitudes.clear();
    m_noiseFloorWindow.clear();
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

double BinChannel::estimateNoiseFloor() const
{
    // Require enough samples to be meaningful — 20 blocks = 200 ms of
    // magnitude history. During warmup, return 0 (no lift from baseline
    // Schmitt behavior).
    if (m_noiseFloorWindow.size() < 20) return 0.0;

    // 10th-percentile of recent smoothed magnitudes. During decoding,
    // the quietest 10% of recent blocks are inter-character gaps —
    // exactly the level the off-threshold must clear to release the
    // Schmitt. During silence, it's near zero.
    std::vector<double> sorted(m_noiseFloorWindow.begin(), m_noiseFloorWindow.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted[sorted.size() / 10];
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
    //
    // Scale factor tracks block size (we now run at the device's native
    // sample rate, so `count` may be 80, 441, 480 etc. depending on rate).
    // 6553.6 = 32768 / 5 gives the "5× more sensitive" calibration.
    const double kScale = static_cast<double>(count) * 6553.6;
    double rawMag = std::sqrt(std::max(0.0, mag2)) / kScale;
    if (rawMag > 1.0) rawMag = 1.0;

    // Magnitude smoothing: currently disabled (window = 1, raw Goertzel
    // magnitude passes straight through to the Schmitt).
    //
    // Earlier versions used a 4-block (40 ms) moving average to damp the
    // Goertzel magnitude oscillation that appears when a signal lands
    // between bin centers. That smoothing worked for off-center signals
    // but introduced a ~35 ms decay latency on every element — which
    // stretched legitimate dots and dashes enough to make the WPM
    // estimator lock 30-50% too slow on bins with strong signal. The
    // classifier's dot/dash threshold scaled up to 2× the wrong dot
    // length, causing real dashes to be classified as dots — the source
    // of the "==5=" stuck pattern on middle bins.
    //
    // With native-rate processing via pavucontrol monitor routing (no
    // aliasing), well-aligned passbands, and the Schmitt's hysteresis +
    // adaptive noise-floor lift handling noise rejection, smoothing is
    // no longer required for correctness. The mechanism is kept in place
    // (window capped at 1) so we can re-enable it cheaply if off-center
    // beating resurfaces.
    m_recentMagnitudes.push_back(rawMag);
    while (m_recentMagnitudes.size() > 1) m_recentMagnitudes.pop_front();
    const double normMag = m_recentMagnitudes.back();

    // Feed the noise-floor window AFTER smoothing. 50 blocks = 500 ms of
    // history — long enough to cover several character gaps so the 10th
    // percentile reliably lands on the inter-character floor, short
    // enough to adapt to changing conditions within a second or two.
    m_noiseFloorWindow.push_back(normMag);
    while (m_noiseFloorWindow.size() > 50) m_noiseFloorWindow.pop_front();

    const int blockMs = (count * 1000) / m_sampleRateHz;
    m_elapsedAudioMs += blockMs;

    // Track the peak magnitude observed during the current tone-active
    // run. This is what the relative off-threshold is measured against,
    // so a strong signal's Schmitt releases when magnitude falls to a
    // configured fraction of its actual peak rather than all the way to
    // a fraction of squelch. Resets to 0 on every tone-off transition
    // (below), so each element tracks its own peak independently.
    if (m_toneActive && normMag > m_currentToneOnPeak) {
        m_currentToneOnPeak = normMag;
    }

    // Schmitt-trigger hysteresis with TWO adaptive components:
    //   (a) noise-floor-lifted floor — keeps off above the bin's
    //       rolling 10th-percentile magnitude so gaps can be detected
    //       on bins close to strong signal bleed.
    //   (b) peak-relative ceiling — off rises to 30% of the current
    //       tone's peak on strong signals. Without this, strong signals
    //       stretch every element by ~7 ms (the time it takes the
    //       transition block's partial-tone magnitude to drop below a
    //       squelch-based threshold), biasing the WPM estimator ~20%
    //       low and misclassifying dashes as dots.
    //
    // Both are capped at 90% of the on-threshold so hysteresis still
    // guarantees on > off.
    const double noiseFloor = estimateNoiseFloor();
    const float baseOff = squelchThreshold * 0.7f;
    const float floorLifted = static_cast<float>(noiseFloor * 1.3);
    const float peakRelative = static_cast<float>(m_currentToneOnPeak * 0.3);
    const float offThreshold = qMin(squelchThreshold * 0.9f,
                                    qMax(baseOff,
                                         qMax(floorLifted, peakRelative)));
    bool toneDetected;
    if (m_toneActive) {
        toneDetected = normMag > offThreshold;       // sticky — hold on until drop
    } else {
        toneDetected = normMag > squelchThreshold;   // need full threshold to turn on
    }

    // Commit transition immediately (hysteresis handles the jitter cleanup
    // that a multi-block debounce was trying to solve).
    bool committedTransition = (toneDetected != m_toneActive);
    m_pendingBlocks = 0;  // retained for potential future multi-block smoothing

    // ---- Stuck-tone safety release -----------------------------------
    // If the Schmitt trigger has been "on" for longer than any plausible
    // CW element (6× current dot estimate = 2× a full-length dash), the
    // detector is wedged — most likely the noise floor crept above the
    // hysteresis off-threshold and the tone never formally "turns off".
    // Without release, no character gap is ever measured, the morse
    // buffer accumulates silently, and the decoder appears to stop even
    // though audio keeps flowing.
    //
    // Force-close the element, reset the smoothing buffer so the next
    // tone-detect decision starts fresh, and treat this as an ordinary
    // tone-off transition so downstream classification/gap logic still
    // runs on the next real tone-on.
    if (!committedTransition && m_toneActive && !muted) {
        const int onRunMs = static_cast<int>(m_elapsedAudioMs - m_elementStartMs);
        const int stuckThresholdMs = currentDotEstimateMs() * 6;
        if (stuckThresholdMs > 0 && onRunMs > stuckThresholdMs) {
            closeElement(onRunMs, timestampMs, out);
            closeCharacter(timestampMs, out);
            m_toneActive = false;
            m_elementStartMs = m_elapsedAudioMs;
            m_recentMagnitudes.clear();    // force fresh Schmitt decision
            m_currentToneOnPeak = 0.0;     // reset peak tracker
            // Don't fall into the transition branch below — we've already
            // handled the state change manually.
            committedTransition = false;
            toneDetected = false;
        }
    }

    if (committedTransition) {
        const int runMs = static_cast<int>(m_elapsedAudioMs - m_elementStartMs);
        if (m_toneActive) {
            // Tone just went off → close an element. Also reset the
            // per-element peak tracker so the next element's off-threshold
            // is calibrated to its own peak, not this element's.
            if (!muted) {
                closeElement(runMs, timestampMs, out);
            }
            m_currentToneOnPeak = 0.0;
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
