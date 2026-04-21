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
    m_dotLengths.clear();
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

void BinChannel::updateWpmEstimate(int dotLenMs)
{
    if (dotLenMs <= 0) return;
    m_dotLengths.push_back(dotLenMs);
    while (m_dotLengths.size() > static_cast<size_t>(kDotLengthWindow)) {
        m_dotLengths.pop_front();
    }
    // Median of the rolling window.
    std::vector<int> sorted(m_dotLengths.begin(), m_dotLengths.end());
    std::sort(sorted.begin(), sorted.end());
    const int median = sorted[sorted.size() / 2];
    const int wpm = (median > 0) ? static_cast<int>(1200.0 / median + 0.5) : 0;
    if (wpm >= m_wpmMin && wpm <= m_wpmMax) {
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
    if (durationMs <= 0) return;

    // Classify as dot (.) or dash (-).
    // Dot threshold = 2× current dot estimate if locked; else default based on
    // mid-range of the operator's WPM bounds.
    int dotBaseline;
    if (m_lockState == LockState::Locked && m_currentWpm > 0) {
        dotBaseline = 1200 / m_currentWpm;
    } else {
        const int midWpm = (m_wpmMin + m_wpmMax) / 2;
        dotBaseline = (midWpm > 0) ? (1200 / midWpm) : 50;
    }

    if (durationMs < dotBaseline * 2) {
        // dot — feeds the WPM estimator directly
        m_morseBuffer.append('.');
        updateWpmEstimate(durationMs);
    } else {
        // dash — do NOT feed the WPM estimator with dashes/3. The classifier
        // uses the current estimate to decide dot-vs-dash, so dashes are
        // self-selecting as "long" — pushing them back into the estimator
        // creates a bias that prevents lock on a stable dot length. Dots
        // alone converge faster and more accurately. Dashes only contribute
        // to character decoding.
        m_morseBuffer.append('-');
    }
    Q_UNUSED(timestampMs);
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
            // Tone just turned on → check gap length (character/word boundary).
            if (!muted) {
                int dotBaseline;
                if (m_lockState == LockState::Locked && m_currentWpm > 0) {
                    dotBaseline = 1200 / m_currentWpm;
                } else {
                    const int midWpm = (m_wpmMin + m_wpmMax) / 2;
                    dotBaseline = (midWpm > 0) ? (1200 / midWpm) : 50;
                }
                // Word vs. character gap classification.
                //   gap ≥ 4 dot-units → word boundary (close char + space)
                //   gap ≥ 2 dot-units → character boundary (close char only)
                //   gap < 2 dot-units → intra-element (keep accumulating)
                // 4 is a practical compromise: textbook Morse says 7 dot-units
                // for word gaps, but contest operators commonly compress to
                // 4-5 units at speed. At 4, occasional intra-word gaps that
                // stretch to ~4 units will produce false word breaks (e.g.,
                // "P OTA" instead of "POTA"), which is less damaging than
                // fusing whole transmissions into one word.
                // TODO (follow-up): make this adaptive by tracking recent gap
                // distribution per bin, since operator sending style varies.
                if (runMs >= dotBaseline * 4) {
                    closeCharacter(timestampMs, out);
                    m_textBuffer.append(' ');
                    if (m_textBuffer.size() > kTextBufferCapChars) {
                        m_textBuffer.remove(0, m_textBuffer.size() - kTextBufferCapChars);
                    }
                    out.append({m_binIndex, QChar(' '), timestampMs});
                } else if (runMs >= dotBaseline * 2) {
                    // character gap (≥ 3 dot-units, but <5) → close character
                    // only — no visible space (standard Morse rendering).
                    closeCharacter(timestampMs, out);
                }
                // else: still within element — keep accumulating current char
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
