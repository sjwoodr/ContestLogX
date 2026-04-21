/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "cwDecoderWidget.h"
#include "audio/cwDecoderWorker.h"
#include "audio/audioCapture.h"
#include "settings.h"
#include "debugLogger.h"

#include <QAudioDevice>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

using namespace clx::audio;

CwDecoderWidget::CwDecoderWidget(RadioSide owningRadio, QWidget* parent)
    : QDockWidget(parent)
    , m_owningRadio(owningRadio)
{
    const bool isR = isRightRadio();
    setObjectName(isR ? "CwDecoderWidgetRight" : "CwDecoderWidgetLeft");
    setWindowTitle(isR ? QStringLiteral("Radio R — CW Decoder")
                       : QStringLiteral("Radio L — CW Decoder"));
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);
    buildUi();
    loadSettings();
}

CwDecoderWidget::~CwDecoderWidget()
{
    endDecoding();
}

void CwDecoderWidget::buildUi()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* outer = new QVBoxLayout(central);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(4);

    // Controls row: passband, bin count, wpm range, squelch, spotlight, mute indicator, Clear.
    QHBoxLayout* controls = new QHBoxLayout;
    controls->setSpacing(6);

    controls->addWidget(new QLabel(tr("Passband")));
    m_passbandLowSpin = new QSpinBox(this);
    m_passbandLowSpin->setRange(200, 2400);
    m_passbandLowSpin->setSuffix(QStringLiteral(" Hz"));
    m_passbandHighSpin = new QSpinBox(this);
    m_passbandHighSpin->setRange(300, 2500);
    m_passbandHighSpin->setSuffix(QStringLiteral(" Hz"));
    controls->addWidget(m_passbandLowSpin);
    controls->addWidget(new QLabel(QStringLiteral("–")));
    controls->addWidget(m_passbandHighSpin);

    controls->addWidget(new QLabel(tr("Bins")));
    m_binCountSpin = new QSpinBox(this);
    m_binCountSpin->setRange(1, kMaxBinCount);
    controls->addWidget(m_binCountSpin);

    controls->addWidget(new QLabel(tr("WPM")));
    m_wpmMinSpin = new QSpinBox(this);
    m_wpmMinSpin->setRange(3, 99);
    m_wpmMaxSpin = new QSpinBox(this);
    m_wpmMaxSpin->setRange(4, 100);
    controls->addWidget(m_wpmMinSpin);
    controls->addWidget(new QLabel(QStringLiteral("–")));
    controls->addWidget(m_wpmMaxSpin);

    controls->addWidget(new QLabel(tr("Squelch")));
    m_squelchSlider = new QSlider(Qt::Horizontal, this);
    m_squelchSlider->setRange(0, 100);
    m_squelchSlider->setMinimumWidth(80);
    controls->addWidget(m_squelchSlider);

    controls->addWidget(new QLabel(tr("Spotlight row")));
    m_spotlightSpin = new QSpinBox(this);
    m_spotlightSpin->setRange(-1, kMaxBinCount - 1);
    m_spotlightSpin->setSpecialValueText(tr("none"));
    controls->addWidget(m_spotlightSpin);

    m_muteIndicator = new QLabel(tr(""), this);
    m_muteIndicator->setStyleSheet(QStringLiteral("color: #c0392b; font-weight: bold;"));
    controls->addWidget(m_muteIndicator);

    controls->addStretch();

    m_clearButton = new QPushButton(tr("Clear"), this);
    controls->addWidget(m_clearButton);

    outer->addLayout(controls);

    // Rows container — populated in rebuildRows() once the worker emits
    // binLayoutChanged() with the actual center frequencies.
    m_rowsContainer = new QWidget(this);
    m_rowsLayout = new QVBoxLayout(m_rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    outer->addWidget(m_rowsContainer, /*stretch*/ 1);

    setWidget(central);

    // Control-change signal wiring.
    connect(m_clearButton, &QPushButton::clicked,
            this, &CwDecoderWidget::onClearClicked);
    connect(m_passbandLowSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onPassbandChanged);
    connect(m_passbandHighSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onPassbandChanged);
    connect(m_binCountSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onBinCountChanged);
    connect(m_squelchSlider, &QSlider::valueChanged,
            this, &CwDecoderWidget::onSquelchChanged);
    connect(m_wpmMinSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onWpmRangeChanged);
    connect(m_wpmMaxSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onWpmRangeChanged);
    connect(m_spotlightSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onSpotlightRowChanged);
}

void CwDecoderWidget::loadSettings()
{
    m_applyingSettings = true;
    Settings& s = Settings::instance();
    const bool r = isRightRadio();

    m_passbandLowSpin->setValue(s.getCwDecoderPassbandLowHz(r));
    m_passbandHighSpin->setValue(s.getCwDecoderPassbandHighHz(r));
    m_binCountSpin->setValue(s.getCwDecoderBinCount(r));
    m_spotlightRow = s.getCwDecoderSpotlightRowIndex(r);
    m_spotlightSpin->setValue(m_spotlightRow);
    m_squelchSlider->setValue(static_cast<int>(s.getCwDecoderSquelch(r) * 100.0));
    m_wpmMinSpin->setValue(s.getCwDecoderWpmMin(r));
    m_wpmMaxSpin->setValue(s.getCwDecoderWpmMax(r));

    m_applyingSettings = false;
}

void CwDecoderWidget::saveSettings()
{
    Settings& s = Settings::instance();
    const bool r = isRightRadio();
    s.setCwDecoderPassbandLowHz(r, m_passbandLowSpin->value());
    s.setCwDecoderPassbandHighHz(r, m_passbandHighSpin->value());
    s.setCwDecoderBinCount(r, m_binCountSpin->value());
    s.setCwDecoderSpotlightRowIndex(r, m_spotlightSpin->value());
    s.setCwDecoderSquelch(r, m_squelchSlider->value() / 100.0);
    s.setCwDecoderWpmMin(r, m_wpmMinSpin->value());
    s.setCwDecoderWpmMax(r, m_wpmMaxSpin->value());
}

void CwDecoderWidget::beginDecoding(const QString& audioDeviceDescription)
{
    endDecoding();

    // Resolve the device from QMediaDevices by description.
    QAudioDevice chosen;
    for (const QAudioDevice& d : QMediaDevices::audioInputs()) {
        if (d.description() == audioDeviceDescription) {
            chosen = d;
            break;
        }
    }
    if (chosen.isNull()) {
        if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
            DebugLogger::instance().log("CwDecoder",
                QString("Audio device '%1' not found; decoder disabled for %2")
                    .arg(audioDeviceDescription,
                         isRightRadio() ? "Radio R" : "Radio L"));
        }
        return;
    }

    AudioCapture* capture = new AudioCapture(chosen);

    m_workerThread = new QThread(this);
    m_worker = new CwDecoderWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_worker, &CwDecoderWorker::charDecoded,
            this, &CwDecoderWidget::onCharDecoded, Qt::QueuedConnection);
    connect(m_worker, &CwDecoderWorker::wpmUpdated,
            this, &CwDecoderWidget::onWpmUpdated, Qt::QueuedConnection);
    connect(m_worker, &CwDecoderWorker::binLayoutChanged,
            this, &CwDecoderWidget::onBinLayoutChanged, Qt::QueuedConnection);
    connect(m_worker, &CwDecoderWorker::muteStateChanged,
            this, &CwDecoderWidget::onMuteStateChanged, Qt::QueuedConnection);
    connect(m_worker, &CwDecoderWorker::pttFallbackLogged, this, [this]() {
        // FR-019b fallback notice — always log this one since it indicates a
        // real functional limitation the operator needs to know about.
        DebugLogger::instance().log("CwDecoder",
            QString("Rig backend for %1 does not report PTT state; decoder relies "
                    "on internal-send signalling only (FR-019b).")
                .arg(isRightRadio() ? "Radio R" : "Radio L"));
    }, Qt::QueuedConnection);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();

    // Invoke startCapture on the worker thread.
    QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection,
                              Q_ARG(clx::audio::AudioCapture*, capture),
                              Q_ARG(int, m_passbandLowSpin->value()),
                              Q_ARG(int, m_passbandHighSpin->value()),
                              Q_ARG(int, m_binCountSpin->value()),
                              Q_ARG(int, m_wpmMinSpin->value()),
                              Q_ARG(int, m_wpmMaxSpin->value()),
                              Q_ARG(float, static_cast<float>(m_squelchSlider->value() / 100.0)));
}

void CwDecoderWidget::endDecoding()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stopCapture", Qt::BlockingQueuedConnection);
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(500);
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    m_worker = nullptr;
}

void CwDecoderWidget::muteForInternalSend(int durationMs)
{
    if (!m_worker) return;
    QMetaObject::invokeMethod(m_worker, "muteForInternalSend", Qt::QueuedConnection,
                              Q_ARG(int, durationMs));
}

void CwDecoderWidget::setPttMute(bool active)
{
    if (!m_worker) return;
    QMetaObject::invokeMethod(m_worker, "setPttMute", Qt::QueuedConnection,
                              Q_ARG(bool, active));
}

void CwDecoderWidget::rebuildRows(const QList<double>& centerFrequencies)
{
    // Clear previous rows.
    for (auto& row : m_rows) {
        if (row.container) row.container->deleteLater();
    }
    m_rows.clear();

    for (int i = 0; i < centerFrequencies.size(); ++i) {
        CwDecoderRow row;
        row.container = new QWidget(m_rowsContainer);
        QHBoxLayout* h = new QHBoxLayout(row.container);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);

        row.freqLabel = new QLabel(
            QStringLiteral("%1 Hz").arg(static_cast<int>(centerFrequencies[i])),
            row.container);
        row.freqLabel->setMinimumWidth(60);
        row.freqLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont freqFont = row.freqLabel->font();
        freqFont.setFamily(QStringLiteral("monospace"));
        row.freqLabel->setFont(freqFont);

        row.wpmLabel = new QLabel(QStringLiteral("— WPM"), row.container);
        row.wpmLabel->setMinimumWidth(60);
        row.wpmLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row.text = new QPlainTextEdit(row.container);
        row.text->setReadOnly(true);
        row.text->setMaximumBlockCount(1);           // one logical line, scrolling
        row.text->setLineWrapMode(QPlainTextEdit::NoWrap);
        row.text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        row.text->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        row.text->setFixedHeight(24);
        row.text->setTextInteractionFlags(Qt::TextBrowserInteraction);
        QFont txtFont = row.text->font();
        txtFont.setFamily(QStringLiteral("monospace"));
        row.text->setFont(txtFont);

        h->addWidget(row.freqLabel);
        h->addWidget(row.wpmLabel);
        h->addWidget(row.text, /*stretch*/ 1);

        m_rowsLayout->addWidget(row.container);
        m_rows.append(row);
    }

    applySpotlightVisuals();
}

void CwDecoderWidget::applySpotlightVisuals()
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (!m_rows[i].container) continue;
        if (i == m_spotlightRow) {
            m_rows[i].container->setStyleSheet(
                QStringLiteral("background-color: rgba(255, 236, 139, 60);"));
        } else {
            m_rows[i].container->setStyleSheet(QString());
        }
    }
}

void CwDecoderWidget::appendCharToRow(int binIndex, QChar ch)
{
    if (binIndex < 0 || binIndex >= m_rows.size()) return;
    QPlainTextEdit* te = m_rows[binIndex].text;
    if (!te) return;
    te->moveCursor(QTextCursor::End);
    te->insertPlainText(QString(ch));
    te->moveCursor(QTextCursor::End);
}

void CwDecoderWidget::onCharDecoded(int binIndex, QChar ch, qint64 timestampMs)
{
    Q_UNUSED(timestampMs);
    appendCharToRow(binIndex, ch);
    rescanTokensForRow(binIndex);
}

void CwDecoderWidget::rescanTokensForRow(int binIndex)
{
    if (binIndex < 0 || binIndex >= m_rows.size()) return;
    QPlainTextEdit* te = m_rows[binIndex].text;
    if (!te) return;

    // Scan tail 20 chars for callsign / RST shapes. When a match is found,
    // emit the corresponding signal. NOTE: this is a simple behavior —
    // token hit-testing on click is a stretch goal; for now, callsigns and
    // RSTs are automatically emitted as they complete, and MainWindow can
    // optionally surface them as hints. Click-to-fill implementation uses
    // QPlainTextEdit's contextMenuEvent or a custom click handler (TODO).
    static const QRegularExpression callRe(
        QStringLiteral(R"(\b[A-Z0-9]*[A-Z][0-9][A-Z0-9]*[A-Z](?:/[A-Z0-9]+)?\b)"));
    static const QRegularExpression rstRe(
        QStringLiteral(R"(\b(?:5NN|4NN|3NN|[1-5][1-9][1-9]|[1-5][1-9])\b)"));

    const QString tail = te->toPlainText().right(24);
    QRegularExpressionMatchIterator itCall = callRe.globalMatch(tail);
    while (itCall.hasNext()) {
        QRegularExpressionMatch m = itCall.next();
        if (!m.hasMatch()) continue;
        // Emit every time a match is detected — downstream handles dedup.
        emit callClicked(m.captured(0), binIndex);
    }
    QRegularExpressionMatchIterator itRst = rstRe.globalMatch(tail);
    while (itRst.hasNext()) {
        QRegularExpressionMatch m = itRst.next();
        if (!m.hasMatch()) continue;
        // NOTE: spec requires clickable tokens, not auto-emit on sight. For MVP
        // we emit on detection; future iteration should wrap tokens as clickable
        // anchors and emit only on actual operator click. Left as TODO.
        // Temporarily disabled auto-emit of RST — would otherwise spam the field.
        Q_UNUSED(m);
    }
}

void CwDecoderWidget::onWpmUpdated(int binIndex, int wpm)
{
    if (binIndex < 0 || binIndex >= m_rows.size()) return;
    QLabel* lbl = m_rows[binIndex].wpmLabel;
    if (!lbl) return;
    if (wpm <= 0) {
        lbl->setText(QStringLiteral("— WPM"));
    } else {
        lbl->setText(QStringLiteral("%1 WPM").arg(wpm));
    }
}

void CwDecoderWidget::onBinLayoutChanged(const QList<double>& centerFrequencies)
{
    rebuildRows(centerFrequencies);
}

void CwDecoderWidget::onMuteStateChanged(bool muted)
{
    m_muted = muted;
    m_muteIndicator->setText(muted ? tr("MUTED (TX)") : QString());
}

void CwDecoderWidget::onClearClicked()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "clearBuffers", Qt::QueuedConnection);
    }
    for (auto& row : m_rows) {
        if (row.text) row.text->clear();
    }
}

void CwDecoderWidget::onPassbandChanged()
{
    if (m_applyingSettings || !m_worker) return;
    saveSettings();
    QMetaObject::invokeMethod(m_worker, "reconfigure", Qt::QueuedConnection,
                              Q_ARG(int, m_passbandLowSpin->value()),
                              Q_ARG(int, m_passbandHighSpin->value()),
                              Q_ARG(int, m_binCountSpin->value()));
}

void CwDecoderWidget::onBinCountChanged()
{
    if (m_applyingSettings || !m_worker) return;
    // Clamp spotlight if it falls outside new range.
    const int newCount = m_binCountSpin->value();
    if (m_spotlightRow >= newCount) {
        m_spotlightRow = -1;
        m_spotlightSpin->setValue(-1);
    }
    saveSettings();
    QMetaObject::invokeMethod(m_worker, "reconfigure", Qt::QueuedConnection,
                              Q_ARG(int, m_passbandLowSpin->value()),
                              Q_ARG(int, m_passbandHighSpin->value()),
                              Q_ARG(int, m_binCountSpin->value()));
}

void CwDecoderWidget::onSquelchChanged(int sliderValue)
{
    if (m_applyingSettings) return;
    saveSettings();
    if (!m_worker) return;
    QMetaObject::invokeMethod(m_worker, "setSquelch", Qt::QueuedConnection,
                              Q_ARG(float, static_cast<float>(sliderValue / 100.0)));
}

void CwDecoderWidget::onWpmRangeChanged()
{
    if (m_applyingSettings) return;
    // Ensure max > min.
    if (m_wpmMaxSpin->value() <= m_wpmMinSpin->value()) {
        m_wpmMaxSpin->setValue(m_wpmMinSpin->value() + 1);
        return;
    }
    saveSettings();
    if (!m_worker) return;
    QMetaObject::invokeMethod(m_worker, "setWpmRange", Qt::QueuedConnection,
                              Q_ARG(int, m_wpmMinSpin->value()),
                              Q_ARG(int, m_wpmMaxSpin->value()));
}

void CwDecoderWidget::onSpotlightRowChanged(int rowIndex)
{
    m_spotlightRow = rowIndex;
    applySpotlightVisuals();
    if (!m_applyingSettings) saveSettings();
}

int CwDecoderWidget::currentWpm(int binIndex) const
{
    if (binIndex < 0 || binIndex >= m_rows.size()) return 0;
    QLabel* lbl = m_rows[binIndex].wpmLabel;
    if (!lbl) return 0;
    const QString txt = lbl->text();
    QRegularExpression re(QStringLiteral(R"((\d+)\s*WPM)"));
    QRegularExpressionMatch m = re.match(txt);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

QString CwDecoderWidget::bufferText(int binIndex) const
{
    if (binIndex < 0 || binIndex >= m_rows.size()) return QString();
    QPlainTextEdit* te = m_rows[binIndex].text;
    return te ? te->toPlainText() : QString();
}
