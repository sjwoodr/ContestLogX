/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/cwDecoder.h"

namespace clx::audio {

CwDecoder::CwDecoder(int sampleRateHz)
    : m_sampleRateHz(sampleRateHz)
{
    configure(kDefaultPassbandLowHz, kDefaultPassbandHighHz, kDefaultBinCount,
              kDefaultWpmMin, kDefaultWpmMax, kDefaultSquelch, sampleRateHz);
}

bool CwDecoder::configure(int passbandLowHz, int passbandHighHz, int binCount,
                          int wpmMin, int wpmMax, float squelchThreshold,
                          int sampleRateHz)
{
    // Update the sample rate (used by BinChannel for Goertzel coefficients).
    if (sampleRateHz > 0) m_sampleRateHz = sampleRateHz;

    // Validation (data-model §Validation rules).
    if (passbandLowHz < 200 || passbandLowHz > 2400) return false;
    if (passbandHighHz <= passbandLowHz || passbandHighHz > 2500) return false;
    if (binCount < 1 || binCount > kMaxBinCount) return false;
    const int spacing = (passbandHighHz - passbandLowHz) / binCount;
    if (spacing < kMinBinSpacingHz) return false;
    if (wpmMin < 3 || wpmMax <= wpmMin || wpmMax > 100) return false;
    if (squelchThreshold < 0.0f || squelchThreshold > 1.0f) return false;

    m_passbandLowHz = passbandLowHz;
    m_passbandHighHz = passbandHighHz;
    m_wpmMin = wpmMin;
    m_wpmMax = wpmMax;
    m_squelch = squelchThreshold;

    // Rebuild bins. Bin centers are evenly spaced across the passband.
    m_bins.clear();
    m_bins.reserve(binCount);
    const double span = passbandHighHz - passbandLowHz;
    for (int i = 0; i < binCount; ++i) {
        const double center = passbandLowHz + (span * (i + 0.5) / binCount);
        m_bins.emplace_back(std::make_unique<BinChannel>(
            i, center, m_sampleRateHz, m_wpmMin, m_wpmMax));
    }
    return true;
}

double CwDecoder::binCenterFreq(int binIndex) const
{
    if (binIndex < 0 || binIndex >= static_cast<int>(m_bins.size())) return 0.0;
    return m_bins[binIndex]->centerFreqHz();
}

int CwDecoder::binCurrentWpm(int binIndex) const
{
    if (binIndex < 0 || binIndex >= static_cast<int>(m_bins.size())) return 0;
    return m_bins[binIndex]->currentWpm();
}

LockState CwDecoder::binLockState(int binIndex) const
{
    if (binIndex < 0 || binIndex >= static_cast<int>(m_bins.size())) return LockState::NoLock;
    return m_bins[binIndex]->lockState();
}

QString CwDecoder::binTextBuffer(int binIndex) const
{
    if (binIndex < 0 || binIndex >= static_cast<int>(m_bins.size())) return QString();
    return m_bins[binIndex]->textBuffer();
}

QList<double> CwDecoder::binCenterFrequencies() const
{
    QList<double> out;
    out.reserve(static_cast<int>(m_bins.size()));
    for (const auto& b : m_bins) out.append(b->centerFreqHz());
    return out;
}

void CwDecoder::setWpmBounds(int wpmMin, int wpmMax)
{
    if (wpmMin < 3 || wpmMax <= wpmMin || wpmMax > 100) return;
    m_wpmMin = wpmMin;
    m_wpmMax = wpmMax;
    for (auto& b : m_bins) b->setWpmBounds(wpmMin, wpmMax);
}

void CwDecoder::clearAllBuffers()
{
    for (auto& b : m_bins) b->clearTextBuffer();
}

QList<CharEvent> CwDecoder::processBlock(const int16_t* samples, int count,
                                         qint64 timestampMs)
{
    QList<CharEvent> out;
    for (auto& b : m_bins) {
        QList<CharEvent> binOut = b->processBlock(samples, count, timestampMs,
                                                   m_squelch, m_muted);
        for (const auto& ev : binOut) out.append(ev);
    }
    return out;
}

} // namespace clx::audio
