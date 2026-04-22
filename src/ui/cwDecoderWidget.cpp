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

#include <QAbstractItemView>
#include <QAudioDevice>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
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

    // When this dock is floated, lock its height to its natural size hint
    // so the operator can grow it horizontally (to see more decoded text
    // scrolling per row) but can't inflate it vertically (which would
    // only add whitespace — the row count is already set by the bin
    // config). When docked again, release the constraint so Qt's dock
    // layout can manage vertical size normally.
    connect(this, &QDockWidget::topLevelChanged, this, [this](bool floating) {
        if (floating) {
            const int h = sizeHint().height();
            setFixedHeight(h > 0 ? h : height());
        } else {
            setMinimumHeight(0);
            setMaximumHeight(QWIDGETSIZE_MAX);
        }
    });
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

    // Controls row: audio device, passband, bin count, wpm range, squelch, mute indicator, Clear.
    QHBoxLayout* controls = new QHBoxLayout;
    controls->setSpacing(6);

    // Audio input device — same list as in Rig Connection Settings, so the
    // operator can switch source without opening a dialog. Changes write
    // back to the per-radio rig settings and restart the decoder's capture.
    // The combo is deliberately narrow in the header so the decoder fits
    // on low-resolution displays; the popup list below is sized wider so
    // full device names are visible when the operator clicks to change.
    controls->addWidget(new QLabel(tr("Audio")));
    m_audioDeviceCombo = new QComboBox(this);
    m_audioDeviceCombo->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_audioDeviceCombo->setMinimumContentsLength(8);
    m_audioDeviceCombo->setMaximumWidth(160);
    m_audioDeviceCombo->addItem(tr("(none)"), QString());
    for (const QAudioDevice& d : QMediaDevices::audioInputs()) {
        m_audioDeviceCombo->addItem(d.description(), d.description());
    }
    // Widen the popup so long device names don't truncate when the
    // operator is picking.
    if (m_audioDeviceCombo->view()) {
        m_audioDeviceCombo->view()->setMinimumWidth(400);
    }
    // Tooltip mirrors the current selection so hovering reveals the full
    // name even when the combo itself is showing a truncated label.
    m_audioDeviceCombo->setToolTip(m_audioDeviceCombo->currentText());
    connect(m_audioDeviceCombo, &QComboBox::currentTextChanged, this,
            [this](const QString& text) {
                if (m_audioDeviceCombo) m_audioDeviceCombo->setToolTip(text);
            });
    controls->addWidget(m_audioDeviceCombo);

    // Center frequency + bin count together determine the passband, with a
    // fixed 50 Hz bin spacing:
    //    low  = center - (bins × 25)
    //    high = center + (bins × 25)
    // This exposes the two knobs operators actually reason about — "where
    // is my signal?" and "how wide a net?" — and eliminates the redundant
    // passband-low/high controls that were always derivable from them.
    controls->addWidget(new QLabel(tr("Center")));
    m_centerHzSpin = new QSpinBox(this);
    m_centerHzSpin->setRange(400, 1500);
    m_centerHzSpin->setSingleStep(50);
    m_centerHzSpin->setSuffix(QStringLiteral(" Hz"));
    controls->addWidget(m_centerHzSpin);

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
    connect(m_centerHzSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onCenterOrBinsChanged);
    connect(m_binCountSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onCenterOrBinsChanged);
    connect(m_squelchSlider, &QSlider::valueChanged,
            this, &CwDecoderWidget::onSquelchChanged);
    connect(m_wpmMinSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onWpmRangeChanged);
    connect(m_wpmMaxSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onWpmRangeChanged);
    connect(m_audioDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &CwDecoderWidget::onAudioDeviceChanged);
}

void CwDecoderWidget::loadSettings()
{
    m_applyingSettings = true;
    Settings& s = Settings::instance();
    const bool r = isRightRadio();

    m_centerHzSpin->setValue(s.getCwDecoderCenterHz(r));
    m_binCountSpin->setValue(s.getCwDecoderBinCount(r));
    m_squelchSlider->setValue(static_cast<int>(s.getCwDecoderSquelch(r) * 100.0));

    // Sync the audio combo to the persisted device (same setting the Rig
    // Connection dialog edits). If the previously-saved device is no
    // longer present, fall back to "(none)".
    const QString device = isRightRadio()
        ? s.getRadioRAudioInputDevice()
        : s.getRadioLAudioInputDevice();
    if (m_audioDeviceCombo) {
        int idx = m_audioDeviceCombo->findData(device);
        m_audioDeviceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_wpmMinSpin->setValue(s.getCwDecoderWpmMin(r));
    m_wpmMaxSpin->setValue(s.getCwDecoderWpmMax(r));

    m_applyingSettings = false;
}

void CwDecoderWidget::saveSettings()
{
    Settings& s = Settings::instance();
    const bool r = isRightRadio();
    s.setCwDecoderCenterHz(r, m_centerHzSpin->value());
    s.setCwDecoderBinCount(r, m_binCountSpin->value());
    s.setCwDecoderSquelch(r, m_squelchSlider->value() / 100.0);
    s.setCwDecoderWpmMin(r, m_wpmMinSpin->value());
    s.setCwDecoderWpmMax(r, m_wpmMaxSpin->value());
}

void CwDecoderWidget::beginDecoding(const QString& audioDeviceDescription)
{
    endDecoding();

    // Keep the widget's audio combo in sync when beginDecoding is driven
    // externally (e.g., operator changed the device in the Rig Connection
    // dialog). Guard against re-firing onAudioDeviceChanged while we're
    // just reflecting state.
    if (m_audioDeviceCombo) {
        m_applyingSettings = true;
        int idx = m_audioDeviceCombo->findData(audioDeviceDescription);
        m_audioDeviceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        m_applyingSettings = false;
    }

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

    // Derive passband from center + bins (50 Hz fixed spacing).
    const int center = m_centerHzSpin->value();
    const int bins = m_binCountSpin->value();
    const int lowHz = center - bins * 25;
    const int highHz = center + bins * 25;
    // Invoke startCapture on the worker thread.
    QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection,
                              Q_ARG(clx::audio::AudioCapture*, capture),
                              Q_ARG(int, lowHz),
                              Q_ARG(int, highHz),
                              Q_ARG(int, bins),
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
    // Clear previous rows. Use synchronous delete (not deleteLater) so new
    // rows added below are the ONLY children of m_rowsLayout — otherwise
    // the deferred deletion leaves stale containers in the layout at the
    // moment new rows are inserted, and the user sees (N-1) rows or an
    // incorrectly sized container.
    for (auto& row : m_rows) {
        if (row.container) {
            m_rowsLayout->removeWidget(row.container);
            delete row.container;
        }
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
        row.text->setTextInteractionFlags(
            Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        QFont txtFont = row.text->font();
        txtFont.setFamily(QStringLiteral("monospace"));
        row.text->setFont(txtFont);
        // Cursor becomes a pointing-hand over clickable tokens; default I-beam
        // elsewhere. Enable tracking so cursor shape can change on hover.
        row.text->viewport()->setMouseTracking(true);
        // Install filter to catch operator clicks on decoded tokens (T040).
        row.text->viewport()->installEventFilter(this);

        h->addWidget(row.freqLabel);
        h->addWidget(row.wpmLabel);
        h->addWidget(row.text, /*stretch*/ 1);

        m_rowsLayout->addWidget(row.container);
        m_rows.append(row);
    }

    // Trigger size reconsideration so the dock shrinks when bin count is
    // reduced. Qt doesn't auto-shrink floating docks when children change;
    // invalidating the layout + adjustSize forces a fresh size-hint.
    m_rowsContainer->adjustSize();
    if (widget()) widget()->adjustSize();
    adjustSize();

    // If we're currently floating, the operator's bin-count change has
    // altered the natural height — re-pin to the new size hint so the
    // window resizes to match (and height stays locked horizontally-only).
    if (isFloating()) {
        setFixedHeight(sizeHint().height());
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

// Shared regex patterns for token detection and click validation.
// Callsign: standard international pattern incl. slash-notation portable.
// RST: three-digit, CW short form (5NN/4NN/3NN), or two-digit.
static const QRegularExpression& kCallRe()
{
    // Callsign token. Three-part structure, all parts optional except the
    // central "main call":
    //
    //   (prefix/)?   main_call   (/suffix)?
    //
    // - prefix (1-4 alphanumerics + slash): country or zone prefix for
    //   operation in another entity, e.g. "IT9/DK6XZ" (DK6XZ in Italy),
    //   "PA3/W1AW", "DL/K1ABC". Capped at 4 chars so we don't fold
    //   arbitrary long word prefixes into a callsign match.
    // - main_call ([A-Z0-9]*[A-Z][0-9][A-Z0-9]*[A-Z]): the classic
    //   callsign skeleton — letter, digit, letter, with optional
    //   surrounding alphanumerics. Must end in a letter.
    // - suffix (/alphanumerics): portable/mobile/zone modifier, e.g.
    //   "K1ABC/P", "YB1AR/2", "W1AW/MM".
    //
    // \b word boundaries keep the match isolated. Note: "/" is a
    // non-word char to QRegularExpression, so \b at the start sits
    // before any alphanumeric even if there's a stray slash upstream.
    static const QRegularExpression re(
        QStringLiteral(R"(\b(?:[A-Z0-9]{1,4}/)?[A-Z0-9]*[A-Z][0-9][A-Z0-9]*[A-Z](?:/[A-Z0-9]+)?\b)"));
    return re;
}
static const QRegularExpression& kRstRe()
{
    // RST is three characters: readability [1-5], signal strength [1-9],
    // tone [1-9]. CW operators commonly send N as cut-number shorthand
    // for 9 in the strength and tone positions ("5NN" = 599, "55N" = 559,
    // "5N9" = 599), so we accept [1-9N] in positions 2 and 3. First
    // position stays [1-5] since readability never uses a cut number.
    //
    // Bounded by actual whitespace (or start/end of string) to avoid
    // matching substrings inside longer numbers (e.g., serial "5599"
    // or a callsign's digit run). Lookbehind/ahead don't consume the
    // boundary characters, so the match itself is exactly the three
    // RST characters.
    static const QRegularExpression re(
        QStringLiteral(R"((?<=^|\s)[1-5][1-9N][1-9N](?=\s|$))"));
    return re;
}

void CwDecoderWidget::rescanTokensForRow(int binIndex)
{
    if (binIndex < 0 || binIndex >= m_rows.size()) return;
    QPlainTextEdit* te = m_rows[binIndex].text;
    if (!te) return;

    // Apply visual formatting to clickable tokens so the operator knows
    // what's interactive. This runs on every decoded character — cheap for
    // the 1-line-per-row widget. The actual click-to-fill is driven by
    // eventFilter() on mouse release, not by regex match here.
    QTextDocument* doc = te->document();
    if (!doc) return;

    // Reset all formatting on the block, then re-apply token styling.
    QTextCursor cursor(doc);
    cursor.select(QTextCursor::Document);
    QTextCharFormat defaultFmt;
    cursor.setCharFormat(defaultFmt);

    const QString fullText = te->toPlainText();

    auto applyFormat = [&](const QRegularExpression& re, const QColor& color) {
        QTextCharFormat fmt;
        fmt.setFontUnderline(true);
        fmt.setForeground(color);
        auto it = re.globalMatch(fullText);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (!m.hasMatch()) continue;
            QTextCursor c(doc);
            c.setPosition(m.capturedStart());
            c.setPosition(m.capturedEnd(), QTextCursor::KeepAnchor);
            c.mergeCharFormat(fmt);
        }
    };
    applyFormat(kCallRe(), QColor(0x4d, 0xa6, 0xff));  // blue — callsigns
    applyFormat(kRstRe(),  QColor(0xff, 0xc1, 0x07));  // amber — RST

    // Keep the cursor at the end so the view auto-scrolls to show new chars.
    te->moveCursor(QTextCursor::End);
}

int CwDecoderWidget::rowIndexForViewport(QObject* viewport) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].text && m_rows[i].text->viewport() == viewport) {
            return i;
        }
    }
    return -1;
}

bool CwDecoderWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() != QEvent::MouseButtonRelease &&
        event->type() != QEvent::MouseMove) {
        return QDockWidget::eventFilter(obj, event);
    }

    const int row = rowIndexForViewport(obj);
    if (row < 0) return QDockWidget::eventFilter(obj, event);

    QMouseEvent* me = static_cast<QMouseEvent*>(event);
    QPlainTextEdit* te = m_rows[row].text;
    if (!te) return QDockWidget::eventFilter(obj, event);

    // Find the token under the pointer. QTextCursor::WordUnderCursor stops
    // at the slash character, which would split "IT9/DK6XZ" into just
    // "IT9" or "DK6XZ" depending on click position — so we hand-roll the
    // boundary scan to include alphanumerics AND slashes in the token.
    // That way clicking anywhere in "IT9/DK6XZ" grabs the full call.
    QTextCursor cursor = te->cursorForPosition(me->pos());
    const int clickPos = cursor.position();
    const QString fullText = te->toPlainText();
    auto isTokenChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('/');
    };
    int start = clickPos;
    while (start > 0 && isTokenChar(fullText[start - 1])) --start;
    int end = clickPos;
    while (end < fullText.size() && isTokenChar(fullText[end])) ++end;
    QString word = fullText.mid(start, end - start);
    // Trim stray leading/trailing slashes (shouldn't normally happen, but
    // guards against odd decodes where a "/" sits alone next to a token).
    while (word.startsWith(QLatin1Char('/'))) word.remove(0, 1);
    while (word.endsWith(QLatin1Char('/'))) word.chop(1);

    const bool isCall = !word.isEmpty() && kCallRe().match(word).hasMatch();
    const bool isRst  = !word.isEmpty() && kRstRe().match(word).hasMatch();

    // On hover, swap cursor to pointing-hand when over a token — gives
    // a visual affordance the same way a hyperlink would.
    if (event->type() == QEvent::MouseMove) {
        te->viewport()->setCursor((isCall || isRst)
            ? Qt::PointingHandCursor
            : Qt::IBeamCursor);
        return false;  // let the event propagate for selection
    }

    // MouseButtonRelease: only left-button clicks trigger fill.
    if (me->button() != Qt::LeftButton) return false;

    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder",
            QString("Click on row %1 (%2): word='%3' isCall=%4 isRst=%5")
                .arg(row)
                .arg(isRightRadio() ? "Radio R" : "Radio L")
                .arg(word)
                .arg(isCall ? "true" : "false")
                .arg(isRst ? "true" : "false"));
    }

    if (isCall) {
        emit callClicked(word, row);
        return true;   // consume — operator acted on a token
    }
    if (isRst) {
        // Normalize CW cut-numbers to digits before filling: N → 9, T → 0.
        // So clicking "5NN" writes "599" into the RSTr field (the canonical
        // numeric form the contest engine and logs expect), not literal N's.
        QString normalized = word;
        normalized.replace(QChar('N'), QChar('9'));
        normalized.replace(QChar('T'), QChar('0'));
        if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
            DebugLogger::instance().log("CwDecoder",
                QString("Emitting rstClicked('%1', bin=%2) from %3")
                    .arg(normalized).arg(row)
                    .arg(isRightRadio() ? "Radio R" : "Radio L"));
        }
        emit rstClicked(normalized, row);
        return true;
    }
    return false;       // non-token click falls through (normal selection)
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

void CwDecoderWidget::onCenterOrBinsChanged()
{
    if (m_applyingSettings || !m_worker) return;
    saveSettings();
    // Derive the Goertzel passband from center + bin count, using a fixed
    // 50 Hz bin spacing. Half the bins sit below center, half above, so
    // the low/high offset from center is bins × 25 Hz.
    const int center = m_centerHzSpin->value();
    const int bins = m_binCountSpin->value();
    const int lowHz = center - bins * 25;
    const int highHz = center + bins * 25;
    QMetaObject::invokeMethod(m_worker, "reconfigure", Qt::QueuedConnection,
                              Q_ARG(int, lowHz),
                              Q_ARG(int, highHz),
                              Q_ARG(int, bins));
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

void CwDecoderWidget::onAudioDeviceChanged(int comboIndex)
{
    if (m_applyingSettings) return;
    if (!m_audioDeviceCombo) return;

    const QString device = m_audioDeviceCombo->itemData(comboIndex).toString();

    // Persist the change to the same per-radio rig setting that the
    // Rig Connection dialog edits. Keeps both UIs showing the same
    // value across app restarts.
    Settings& s = Settings::instance();
    if (isRightRadio()) {
        s.setRadioRAudioInputDevice(device);
    } else {
        s.setRadioLAudioInputDevice(device);
    }

    // Restart capture against the new device (or tear it down if set to
    // "(none)"). beginDecoding() handles stopping the previous capture.
    if (device.isEmpty()) {
        endDecoding();
    } else {
        beginDecoding(device);
    }
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
