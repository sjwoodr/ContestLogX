/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/cwDecoderWorker.h"
#include "debugLogger.h"

#include <QDateTime>
#include <QStringList>
#include <QVector>
#include <vector>

namespace clx::audio {

CwDecoderWorker::CwDecoderWorker(QObject* parent)
    : QObject(parent)
{
}

CwDecoderWorker::~CwDecoderWorker()
{
    stopCapture();
}

qint64 CwDecoderWorker::monotonicNowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

void CwDecoderWorker::startCapture(AudioCapture* capture,
                                   int passbandLowHz, int passbandHighHz,
                                   int binCount, int wpmMin, int wpmMax,
                                   float squelchThreshold)
{
    if (m_capture) {
        stopCapture();
    }
    m_capture = capture;
    if (!m_capture) {
        emit errorOccurred(QStringLiteral("Null AudioCapture pointer"));
        return;
    }
    // The capture object should already be on this worker thread — the
    // widget calls `capture->moveToThread(workerThread)` from the main
    // thread (i.e. on the source side, where the object currently lives)
    // BEFORE invokeMethod hands the pointer here. That's the canonical
    // Qt pattern — moveToThread() is supposed to be called from the
    // object's *current* thread. Calling it from the destination thread
    // (which is what an earlier version of this code did) is technically
    // forbidden by Qt and may not actually move thread affinity reliably,
    // which on Windows leaves the QAudioSource and its WASAPI polling
    // timer on the main thread while the worker drives readyRead from
    // here — manifesting as a silent stall after the initial buffered
    // burst.
    Q_ASSERT_X(m_capture->thread() == this->thread(),
               "CwDecoderWorker::startCapture",
               "AudioCapture must be moved to the worker thread by the caller "
               "(CwDecoderWidget::beginDecoding) before invokeMethod");
    m_capture->setParent(this);

    connect(m_capture, &AudioCapture::audioBlockReady,
            this, &CwDecoderWorker::onAudioBlockReady, Qt::QueuedConnection);
    connect(m_capture, &AudioCapture::deviceError,
            this, &CwDecoderWorker::errorOccurred);

    // Start capture FIRST so the device negotiates its actual sample rate
    // (we no longer resample to a fixed internal rate). Only after that
    // can we configure the decoder with Goertzel coefficients matching
    // the audio we'll receive.
    if (!m_capture->start()) {
        emit errorOccurred(QStringLiteral("AudioCapture::start() failed"));
        return;
    }
    m_sampleRateHz = m_capture->actualSampleRate();
    if (m_sampleRateHz <= 0) m_sampleRateHz = kSampleRateHz;
    m_blockSamples = blockSamplesForRate(m_sampleRateHz);
    if (m_blockSamples <= 0) m_blockSamples = 80;  // safety fallback

    DebugLogger::instance().log("CwDecoder",
        QString("Audio capture at %1 Hz, block size %2 samples (%3 ms)")
            .arg(m_sampleRateHz).arg(m_blockSamples).arg(kBlockDurationMs));

    if (!m_decoder.configure(passbandLowHz, passbandHighHz, binCount,
                             wpmMin, wpmMax, squelchThreshold, m_sampleRateHz)) {
        const QString msg = QString(
            "Invalid decoder configuration: passband=%1-%2 Hz, bins=%3, "
            "wpm=%4-%5, squelch=%6, rate=%7 Hz (spacing=%8 Hz, min=%9 Hz)")
            .arg(passbandLowHz).arg(passbandHighHz).arg(binCount)
            .arg(wpmMin).arg(wpmMax).arg(squelchThreshold).arg(m_sampleRateHz)
            .arg(binCount > 0 ? (passbandHighHz - passbandLowHz) / binCount : 0)
            .arg(kMinBinSpacingHz);
        DebugLogger::instance().log("CwDecoder", msg);
        emit errorOccurred(msg);
        return;
    }

    // Decoder configuration summary, debug-gated. The single most important
    // triage hint when the operator says "the decoder shows nothing" — we
    // can immediately see whether their CW pitch (typically 600-800 Hz on
    // most rigs) lands inside the configured passband and which specific
    // bin centers will fire when it does.
    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        QStringList centerStrs;
        for (double f : m_decoder.binCenterFrequencies()) {
            centerStrs.append(QString::number(static_cast<int>(f)) + "Hz");
        }
        DebugLogger::instance().log("CwDecoder",
            QString("Decoder configured: passband=%1-%2 Hz (%3 bins at %4), "
                    "WPM=%5-%6, squelch=%7")
                .arg(passbandLowHz).arg(passbandHighHz)
                .arg(binCount)
                .arg(centerStrs.join(", "))
                .arg(wpmMin).arg(wpmMax)
                .arg(squelchThreshold, 0, 'f', 3));
    }
    m_lastBinStatsLogMs = 0;   // arm the periodic stats sampler

    m_lastReportedWpm.fill(-1, binCount);
    emit captureStarted();
    emit binLayoutChanged(m_decoder.binCenterFrequencies());
}

void CwDecoderWorker::stopCapture()
{
    if (m_capture) {
        m_capture->stop();
        m_capture->deleteLater();
        m_capture = nullptr;
    }
    emit captureStopped();
}

void CwDecoderWorker::reconfigure(int passbandLowHz, int passbandHighHz, int binCount)
{
    // Reuse the already-negotiated sample rate. If the capture isn't
    // running (reconfigure called before startCapture), use the decoder's
    // default and count on startCapture to reconfigure with the real rate.
    const int rate = (m_sampleRateHz > 0) ? m_sampleRateHz : kSampleRateHz;
    if (!m_decoder.configure(passbandLowHz, passbandHighHz, binCount,
                             kDefaultWpmMin, kDefaultWpmMax, kDefaultSquelch,
                             rate)) {
        emit errorOccurred(QStringLiteral("Invalid bin configuration"));
        return;
    }
    m_lastReportedWpm.fill(-1, binCount);
    emit binLayoutChanged(m_decoder.binCenterFrequencies());
}

void CwDecoderWorker::setWpmRange(int wpmMin, int wpmMax)
{
    m_decoder.setWpmBounds(wpmMin, wpmMax);
}

void CwDecoderWorker::setSquelch(float threshold)
{
    m_decoder.setSquelch(threshold);
}

void CwDecoderWorker::setPttMute(bool active)
{
    QMutexLocker lk(&m_mutex);
    if (active) m_muteState.pttSignalReceivedEver = true;
    m_muteState.rigPttActive = active;
    bool muted = m_muteState.isMuted(monotonicNowMs());
    m_decoder.setMuted(muted);
    emit muteStateChanged(muted);
}

void CwDecoderWorker::muteForInternalSend(int durationMs)
{
    QMutexLocker lk(&m_mutex);
    m_muteState.internalSendMuteUntilMs = monotonicNowMs() + qMax(0, durationMs);
    bool muted = m_muteState.isMuted(monotonicNowMs());
    m_decoder.setMuted(muted);
    emit muteStateChanged(muted);
}

void CwDecoderWorker::clearBuffers()
{
    m_decoder.clearAllBuffers();
}

void CwDecoderWorker::onAudioBlockReady()
{
    drainAndProcess();
}

void CwDecoderWorker::drainAndProcess()
{
    if (!m_capture) return;
    if (m_blockSamples <= 0) return;   // not yet configured

    // Process audio in dynamically-sized blocks — 10 ms at whatever the
    // device's native sample rate turned out to be. No downsampling or
    // resampling: we feed the Goertzel detectors samples at the exact
    // rate the device captured.
    std::vector<int16_t> block(m_blockSamples);
    const size_t blockSz = static_cast<size_t>(m_blockSamples);
    while (m_capture->availableSamples() >= blockSz) {
        size_t got = m_capture->popSamples(block.data(), blockSz);
        if (got < blockSz) break;

        // Update internal-send mute timeout.
        {
            QMutexLocker lk(&m_mutex);
            const qint64 now = monotonicNowMs();
            if (m_muteState.internalSendMuteUntilMs != 0 && now >= m_muteState.internalSendMuteUntilMs) {
                m_muteState.internalSendMuteUntilMs = 0;
                bool stillMuted = m_muteState.isMuted(now);
                m_decoder.setMuted(stillMuted);
                emit muteStateChanged(stillMuted);
            } else if (m_muteState.internalSendMuteUntilMs != 0 && !m_decoder.isMuted()) {
                // An internal-send mute is active but decoder isn't muted — sync.
                m_decoder.setMuted(true);
            }
        }

        const qint64 ts = monotonicNowMs();
        QList<CharEvent> events = m_decoder.processBlock(block.data(), m_blockSamples, ts);
        for (const auto& ev : events) {
            // Per-character emission log — kept commented out because at
            // 25 WPM it's ~50 lines/min, drowning the bin-stats output.
            // Uncomment temporarily if a "characters decode but don't
            // appear in the UI" report comes in (i.e. need to confirm
            // the worker is emitting and only the widget end is broken).
            // if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
            //     DebugLogger::instance().log("CwDecoder",
            //         QString("Char emitted: bin=%1 (%2 Hz), char='%3', ts=%4ms")
            //             .arg(ev.binIndex)
            //             .arg(static_cast<int>(m_decoder.binCenterFreq(ev.binIndex)))
            //             .arg(ev.ch)
            //             .arg(ev.timestampMs));
            // }
            emit charDecoded(ev.binIndex, ev.ch, ev.timestampMs);
        }

        // Periodic per-bin magnitude snapshot, debug-gated. Fires every
        // kBinStatsIntervalMs (5 s) and surfaces the current normalized
        // magnitude per bin plus the configured squelch — so we can
        // immediately tell whether the operator's CW pitch is reaching
        // any bin with usable signal level. A leading '*' marks bins
        // currently in tone-active state. If every bin is well below
        // squelch, the pitch is outside the passband or the rig audio
        // is too quiet. If a bin sits above squelch but no characters
        // arrive, it's a classifier / WPM-bounds issue.
        if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
            const qint64 now = monotonicNowMs();
            if (now - m_lastBinStatsLogMs >= kBinStatsIntervalMs) {
                m_lastBinStatsLogMs = now;
                const int bins = m_decoder.binCount();
                QStringList parts;
                parts.reserve(bins);
                for (int i = 0; i < bins; ++i) {
                    parts.append(QString("%1%2Hz=%3")
                        .arg(m_decoder.binToneActive(i) ? QStringLiteral("*")
                                                        : QStringLiteral(" "))
                        .arg(static_cast<int>(m_decoder.binCenterFreq(i)))
                        .arg(m_decoder.binLastMagnitude(i), 0, 'f', 3));
                }
                DebugLogger::instance().log("CwDecoder",
                    QString("Bin stats (squelch=%1): %2")
                        .arg(m_decoder.squelch(), 0, 'f', 3)
                        .arg(parts.join(" ")));
            }
        }

        // Emit WPM updates on change.
        const int binCount = m_decoder.binCount();
        if (m_lastReportedWpm.size() != binCount) {
            m_lastReportedWpm.fill(-1, binCount);
        }
        for (int i = 0; i < binCount; ++i) {
            const int wpm = m_decoder.binCurrentWpm(i);
            if (wpm != m_lastReportedWpm[i]) {
                m_lastReportedWpm[i] = wpm;
                emit wpmUpdated(i, wpm);
            }
        }
    }
}

} // namespace clx::audio
