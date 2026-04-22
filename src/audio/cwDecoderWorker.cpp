/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/cwDecoderWorker.h"
#include "debugLogger.h"

#include <QDateTime>
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
    m_capture->setParent(this);

    if (!m_decoder.configure(passbandLowHz, passbandHighHz, binCount,
                             wpmMin, wpmMax, squelchThreshold)) {
        const QString msg = QString(
            "Invalid decoder configuration: passband=%1-%2 Hz, bins=%3, "
            "wpm=%4-%5, squelch=%6 (spacing=%7 Hz, min=%8 Hz)")
            .arg(passbandLowHz).arg(passbandHighHz).arg(binCount)
            .arg(wpmMin).arg(wpmMax).arg(squelchThreshold)
            .arg(binCount > 0 ? (passbandHighHz - passbandLowHz) / binCount : 0)
            .arg(kMinBinSpacingHz);
        DebugLogger::instance().log("CwDecoder", msg);
        emit errorOccurred(msg);
        return;
    }

    m_lastReportedWpm.fill(-1, binCount);

    connect(m_capture, &AudioCapture::audioBlockReady,
            this, &CwDecoderWorker::onAudioBlockReady, Qt::QueuedConnection);
    connect(m_capture, &AudioCapture::deviceError,
            this, &CwDecoderWorker::errorOccurred);

    if (!m_capture->start()) {
        emit errorOccurred(QStringLiteral("AudioCapture::start() failed"));
        return;
    }
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
    if (!m_decoder.configure(passbandLowHz, passbandHighHz, binCount,
                             m_decoder.binCenterFrequencies().isEmpty() ? kDefaultWpmMin : kDefaultWpmMin,
                             kDefaultWpmMax, kDefaultSquelch)) {
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

    // Process audio in fixed 80-sample blocks (10 ms at 8 kHz).
    int16_t block[kBlockSamples];
    while (m_capture->availableSamples() >= static_cast<size_t>(kBlockSamples)) {
        size_t got = m_capture->popSamples(block, kBlockSamples);
        if (got < static_cast<size_t>(kBlockSamples)) break;

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
        QList<CharEvent> events = m_decoder.processBlock(block, kBlockSamples, ts);
        for (const auto& ev : events) {
            emit charDecoded(ev.binIndex, ev.ch, ev.timestampMs);
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
