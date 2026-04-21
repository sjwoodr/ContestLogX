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
        // dot
        m_morseBuffer.append('.');
        updateWpmEstimate(durationMs);
    } else {
        // dash — dash = 3 × dot, so derive a dot estimate from duration/3
        m_morseBuffer.append('-');
        updateWpmEstimate(durationMs / 3);
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
    // Magnitude squared (normalized by block size² and full-scale²).
    const double mag2 = (m_sPrev * m_sPrev + m_sPrev2 * m_sPrev2
                        - m_coeff * m_sPrev * m_sPrev2);
    // Normalize against a rough scale: (count * full-scale)² ≈ (80 * 32767)² ~= 6.87e12
    constexpr double kScale = static_cast<double>(kBlockSamples) * 32768.0;
    const double normMag = std::sqrt(std::max(0.0, mag2)) / kScale;

    const bool toneDetected = normMag > squelchThreshold;
    const int blockMs = (count * 1000) / m_sampleRateHz;
    m_elapsedAudioMs += blockMs;

    // While muted, still advance elapsed time and transition-detect so state
    // stays sane; but skip character emission.
    if (toneDetected != m_toneActive) {
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
                if (runMs >= dotBaseline * 5) {
                    // word gap (≥ 5 dot-units) → close character plus add space
                    closeCharacter(timestampMs, out);
                    m_textBuffer.append(' ');
                    if (m_textBuffer.size() > kTextBufferCapChars) {
                        m_textBuffer.remove(0, m_textBuffer.size() - kTextBufferCapChars);
                    }
                } else if (runMs >= dotBaseline * 2) {
                    // character gap (≥ 3 dot-units, but <5) → close character
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
