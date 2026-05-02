/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "cwDecoderWidget.h"
#include "audio/cwDecoderWorker.h"
#include "audio/audioCapture.h"
#include "audio/practiceAudioSource.h"
#include "contestEngine.h"
#include "settings.h"
#include "debugLogger.h"

#include <QAbstractItemView>
#include <QAudioDevice>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QMediaDevices>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPalette>
#include <QtGlobal>
// QPermission was introduced in Qt 6.5, but the header is gated behind
// QT_REQUIRE_CONFIG(permissions) — many distro packages and prebuilt Qt
// archives (including the one our CI gets via aqtinstall) ship Qt 6.5+
// with the permissions feature *disabled*, in which case qpermissions.h
// simply isn't installed. Compile-time-detect the header instead of
// version-gating: if it's present we use the API, otherwise we degrade
// gracefully (the silence watchdog already covers the actual "no audio"
// failure mode regardless of whether we can ask the OS about permission).
//
// Include only <QPermission> (not <QMicrophonePermission>): the QPermission
// header forwards to qpermissions.h which declares every permission class
// in one shot, so QMicrophonePermission becomes available transitively. Some
// Qt installs (e.g. CI's macOS Qt 6.5.* via aqtinstall) ship the QPermission
// forwarder but NOT the per-class QMicrophonePermission forwarder, so trying
// to include the latter directly fails even when the API is fully available.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && __has_include(<QPermission>)
#  include <QPermission>
#  define CLX_HAS_QPERMISSION_API 1
#endif
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QStandardItemModel>
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

    // One-shot audio device enumeration log on widget construction.
    // Unconditional (not gated by isCwDecoderDebugEnabled()) because
    // the volume is small and this is exactly the data needed to
    // triage "decoder shows nothing" reports — what devices the OS
    // exposed, which one is the system default, and which one CLX has
    // saved for this radio. Runs once per widget spawn (plus once
    // more for the other radio in SO2R).
    {
        DebugLogger& dl = DebugLogger::instance();
        const QString side = isR ? QStringLiteral("Radio R") : QStringLiteral("Radio L");
        const QAudioDevice defaultIn = QMediaDevices::defaultAudioInput();
        const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
        dl.log("CwDecoder",
            QString("Enumerating audio inputs for %1: %2 device(s) detected; OS default = '%3'")
                .arg(side,
                     QString::number(inputs.size()),
                     defaultIn.isNull() ? QStringLiteral("(none)") : defaultIn.description()));
        int idx = 0;
        for (const QAudioDevice& d : inputs) {
            const QAudioFormat pref = d.preferredFormat();
            dl.log("CwDecoder",
                QString("  [%1] description='%2' id='%3' isDefault=%4 "
                        "preferred=%5Hz/%6ch/sampleFormat=%7")
                    .arg(QString::number(idx++),
                         d.description(),
                         QString::fromUtf8(d.id()),
                         d.id() == defaultIn.id() ? QStringLiteral("true")
                                                  : QStringLiteral("false"),
                         QString::number(pref.sampleRate()),
                         QString::number(pref.channelCount()),
                         QString::number(static_cast<int>(pref.sampleFormat()))));
        }
        const QString saved = isR
            ? Settings::instance().getRadioRAudioInputDevice()
            : Settings::instance().getRadioLAudioInputDevice();
        dl.log("CwDecoder", QString("%1 saved audio device setting: '%2'")
            .arg(side, saved.isEmpty() ? QStringLiteral("(none)") : saved));

        // Log the OS-level microphone permission state alongside the device
        // enumeration. On macOS this reflects TCC; on Windows it reflects
        // the Privacy & Security → Microphone → "Let desktop apps access
        // your microphone" toggle (and any per-app override below it); on
        // Linux QtCore returns Granted unconditionally. Doing this here
        // (not just inside beginDecoding) means the debug log answers
        // "did the OS deny CLX?" the moment the widget exists, even if
        // the operator hasn't tried to start the decoder yet. Requires
        // Qt 6.5+ built with QT_FEATURE_permissions enabled; on builds
        // where it's unavailable we just say so (the silence watchdog
        // still catches actual access failures).
#ifdef CLX_HAS_QPERMISSION_API
        QMicrophonePermission micPerm;
        const Qt::PermissionStatus permStatus = qApp->checkPermission(micPerm);
        const char* permStr = (permStatus == Qt::PermissionStatus::Granted)      ? "Granted"
                            : (permStatus == Qt::PermissionStatus::Denied)        ? "Denied"
                            : (permStatus == Qt::PermissionStatus::Undetermined)  ? "Undetermined"
                            : "Unknown";
        dl.log("CwDecoder",
            QString("%1 OS microphone permission: %2").arg(side, permStr));
#else
        dl.log("CwDecoder",
            QString("%1 OS microphone permission: (QPermission API unavailable in this Qt build)")
                .arg(side));
#endif
    }

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
    // Virtual "Practice" entries live above the real devices. These are
    // sentinel device names handled in beginDecoding() — selecting either
    // spawns a PracticeAudioSource that synthesizes CW training audio
    // (plays on the default output + feeds the decoder pipeline). The
    // "practice-test" entry is enabled/disabled dynamically based on
    // whether a contest is currently loaded (see refreshPracticeContestAvailability).
    m_audioDeviceCombo->addItem(tr("Practice — CW Rag Chew"),
                                QStringLiteral("practice-cw"));
    m_audioDeviceCombo->addItem(tr("Practice — Contest Exchange"),
                                QStringLiteral("practice-test"));
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

    // The WPM bounds spinboxes were removed — the decoder hard-codes the
    // operator-relevant range (5–60 WPM) which already covers every CW
    // operator on the bands. The toolbar real-estate is reused for the
    // Word Gap control below.

    controls->addWidget(new QLabel(tr("Squelch")));
    m_squelchSlider = new QSlider(Qt::Horizontal, this);
    m_squelchSlider->setRange(0, 100);
    m_squelchSlider->setMinimumWidth(80);
    controls->addWidget(m_squelchSlider);

    // Word Gap multiplier — controls how aggressive the decoder is about
    // detecting word boundaries. Default 4.0 is a contest-friendly
    // compromise between textbook 7× spacing and tightly-sent QRQ contest
    // CW where operators compress inter-word gaps to ~3×. Lower = more
    // spaces inserted (helpful when the decoder is running characters
    // together); higher = stricter (helpful for textbook CW so no spaces
    // appear inside callsigns).
    controls->addWidget(new QLabel(tr("Word Gap")));
    m_wordGapSpin = new QDoubleSpinBox(this);
    m_wordGapSpin->setRange(3.0, 8.0);
    m_wordGapSpin->setSingleStep(0.5);
    m_wordGapSpin->setDecimals(1);
    m_wordGapSpin->setToolTip(
        tr("Word boundary multiplier (× dot length).\n"
           "Lower = more aggressive (more spaces inserted) — good for tightly-sent contest CW.\n"
           "Higher = stricter (textbook 7× standard).\n"
           "Default 4.0 splits the difference."));
    controls->addWidget(m_wordGapSpin);

    m_muteIndicator = new QLabel(tr(""), this);
    m_muteIndicator->setStyleSheet(QStringLiteral("color: #c0392b; font-weight: bold;"));
    controls->addWidget(m_muteIndicator);

    controls->addStretch();

    m_clearButton = new QPushButton(tr("Clear"), this);
    controls->addWidget(m_clearButton);

    // Start/Stop toggle. Label tracks the current decoding state and is
    // updated centrally by updateStartStopButton() — which is called from
    // beginDecoding() / endDecoding() so external callers (e.g. MainWindow
    // re-driving the decoder, or a device change in the combo) keep the
    // button in sync without having to touch it directly.
    m_startStopButton = new QPushButton(this);
    m_startStopButton->setToolTip(
        tr("Stop the decoder (and any practice audio); click again to restart"));
    controls->addWidget(m_startStopButton);

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
    connect(m_startStopButton, &QPushButton::clicked,
            this, &CwDecoderWidget::onStartStopClicked);
    connect(m_centerHzSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onCenterOrBinsChanged);
    connect(m_binCountSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &CwDecoderWidget::onCenterOrBinsChanged);
    connect(m_squelchSlider, &QSlider::valueChanged,
            this, &CwDecoderWidget::onSquelchChanged);
    connect(m_wordGapSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &CwDecoderWidget::onWordGapChanged);
    connect(m_audioDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &CwDecoderWidget::onAudioDeviceChanged);

    // Initial Start/Stop label reflects actual state (worker is null at
    // construction, so this resolves to "Start" disabled until either
    // loadSettings selects a device or MainWindow calls beginDecoding).
    updateStartStopButton();
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
    m_wordGapSpin->setValue(s.getCwDecoderWordGap(r));

    // Apply the saved Preferences → Fonts "CW Decoder" font at
    // construction so the widget picks it up on first spawn. Without
    // this, the font only applies after the operator opens Preferences
    // and clicks OK.
    const QFont decoderFont = s.getPanelFont(QStringLiteral("cwDecoder"));
    if (!decoderFont.family().isEmpty()) {
        setBaseFont(decoderFont);
    }

    // Re-evaluate the Start/Stop button now that the combo has its
    // saved selection — enables the button if a real device is picked.
    updateStartStopButton();

    m_applyingSettings = false;
}

void CwDecoderWidget::saveSettings()
{
    Settings& s = Settings::instance();
    const bool r = isRightRadio();
    s.setCwDecoderCenterHz(r, m_centerHzSpin->value());
    s.setCwDecoderBinCount(r, m_binCountSpin->value());
    s.setCwDecoderSquelch(r, m_squelchSlider->value() / 100.0);
    s.setCwDecoderWordGap(r, m_wordGapSpin->value());
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

    // Practice mode: the device name is a sentinel, not a real audio
    // device. Spawn a PracticeAudioSource that synthesizes CW from either
    // a rag-chew template pool or the active contest's exchange format.
    // The WPM comes from Settings::getCwWpm() — same setting the CW
    // console uses — so practice speed follows whatever the operator
    // has the keyer set to.
    const bool isPracticeCw   = (audioDeviceDescription == QLatin1String("practice-cw"));
    const bool isPracticeTest = (audioDeviceDescription == QLatin1String("practice-test"));

    // Real-device path needs OS-level microphone permission. Practice
    // virtual sources don't (they synthesize audio internally) so we
    // skip the check for them. ensureMicrophonePermissionFor() handles
    // the three cases: Granted (returns true, fall through), Denied
    // (shows dialog, returns false), Undetermined (kicks off async
    // request, returns false; on grant it re-invokes beginDecoding).
    if (!isPracticeCw && !isPracticeTest) {
        if (!ensureMicrophonePermissionFor(audioDeviceDescription)) return;
    }

    AudioCapture* capture = nullptr;
    if (isPracticeCw || isPracticeTest) {
        if (isPracticeTest && (!m_contestEngine
                               || m_contestEngine->getContestName().isEmpty())) {
            if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
                DebugLogger::instance().log("CwDecoder",
                    "Practice contest mode requires a loaded contest; ignoring.");
            }
            return;
        }
        auto* practice = new PracticeAudioSource(
            isPracticeTest ? PracticeMode::Contest : PracticeMode::Ragchew);
        practice->setContestEngine(m_contestEngine);
        practice->setWpmProvider([]() { return Settings::instance().getCwWpm(); });
        capture = practice;
    } else {
        // Resolve the real device from QMediaDevices by description.
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
        capture = new AudioCapture(chosen);
    }

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
    // Surface real audio-path errors (silence watchdog, format negotiation,
    // null I/O) to the operator instead of letting them vanish into the
    // debug log. Fires at most once per session per device-start because
    // we tear the decoder down right after — the next click on Start will
    // re-arm the watchdog. Use queued connection because errorOccurred is
    // emitted from the worker thread.
    connect(m_worker, &CwDecoderWorker::errorOccurred,
            this, [this](const QString& message) {
        DebugLogger::instance().log("CwDecoder",
            QString("Audio path error for %1: %2")
                .arg(isRightRadio() ? "Radio R" : "Radio L", message));
        endDecoding();
        QMessageBox::warning(this, tr("CW Decoder — no audio"), message);
    }, Qt::QueuedConnection);
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

    // Push the capture onto the worker thread BEFORE handing the pointer
    // off via invokeMethod. moveToThread() must be called from the
    // object's *current* thread (here, the main thread — capture was
    // just constructed a few lines above). Doing this on the worker
    // side is technically forbidden by Qt and on Windows leaves Qt's
    // WASAPI plugin uncertain about which thread owns its internal
    // polling timer — manifesting as a silent stall after the initial
    // buffered burst (only the first 2-3 readyRead callbacks fire,
    // then nothing further). The ASSERT in CwDecoderWorker::startCapture
    // catches future regressions if anyone moves this call back.
    capture->moveToThread(m_workerThread);

    // Derive passband from center + bins (50 Hz fixed spacing).
    const int center = m_centerHzSpin->value();
    const int bins = m_binCountSpin->value();
    const int lowHz = center - bins * 25;
    const int highHz = center + bins * 25;
    // Invoke startCapture on the worker thread. WPM bounds are
    // hard-coded to the broadly-useful 5-60 range — every CW operator
    // on the bands falls inside it, so a per-operator UI knob added no
    // value and just consumed widget toolbar real estate.
    QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection,
                              Q_ARG(clx::audio::AudioCapture*, capture),
                              Q_ARG(int, lowHz),
                              Q_ARG(int, highHz),
                              Q_ARG(int, bins),
                              Q_ARG(int, clx::audio::kDefaultWpmMin),
                              Q_ARG(int, clx::audio::kDefaultWpmMax),
                              Q_ARG(float, static_cast<float>(m_squelchSlider->value() / 100.0)));
    // Apply the operator's word-gap preference — must be sent after
    // startCapture has built the bins, so do it via a queued setter
    // here rather than as a startCapture argument.
    QMetaObject::invokeMethod(m_worker, "setWordGapMultiplier", Qt::QueuedConnection,
                              Q_ARG(float, static_cast<float>(m_wordGapSpin->value())));
    updateStartStopButton();
}

bool CwDecoderWidget::ensureMicrophonePermissionFor(const QString& audioDeviceDescription)
{
#ifdef CLX_HAS_QPERMISSION_API
    QMicrophonePermission micPerm;
    const Qt::PermissionStatus status = qApp->checkPermission(micPerm);

    if (status == Qt::PermissionStatus::Granted) {
        return true;
    }

    if (status == Qt::PermissionStatus::Undetermined) {
        // Trigger the OS to record / surface a decision. On macOS this
        // pops the TCC consent prompt the first time. On Windows for
        // unpackaged desktop apps the OS rarely shows an interactive
        // prompt — Undetermined collapses straight to Granted or Denied
        // based on the global "Let desktop apps access your microphone"
        // toggle, which is fine: we get a definitive status either way
        // and the lambda below routes accordingly.
        DebugLogger::instance().log("CwDecoder",
            QString("%1 microphone permission undetermined — requesting from OS")
                .arg(isRightRadio() ? "Radio R" : "Radio L"));
        qApp->requestPermission(micPerm, this,
            [this, audioDeviceDescription](const QPermission& p) {
                DebugLogger::instance().log("CwDecoder",
                    QString("%1 microphone permission resolved: %2")
                        .arg(isRightRadio() ? "Radio R" : "Radio L",
                             p.status() == Qt::PermissionStatus::Granted
                                 ? QStringLiteral("Granted")
                                 : QStringLiteral("Denied")));
                if (p.status() == Qt::PermissionStatus::Granted) {
                    beginDecoding(audioDeviceDescription);
                } else {
                    showMicrophonePermissionDeniedDialog();
                }
            });
        return false;
    }

    // Denied.
    DebugLogger::instance().log("CwDecoder",
        QString("%1 microphone permission denied — decoder cannot capture from '%2'")
            .arg(isRightRadio() ? "Radio R" : "Radio L", audioDeviceDescription));
    showMicrophonePermissionDeniedDialog();
    return false;
#else
    // QPermission API not compiled in (older Qt, or a Qt 6.5+ build with
    // QT_FEATURE_permissions disabled — common in distro/aqtinstall-bundled
    // archives). Treat as Granted: if the OS has actually denied access,
    // the silence watchdog in AudioCapture will catch it within 3 seconds
    // and surface the same platform-specific guidance.
    Q_UNUSED(audioDeviceDescription);
    return true;
#endif
}

void CwDecoderWidget::showMicrophonePermissionDeniedDialog()
{
    // Platform-specific instructions because the fix path differs. The
    // generic case covers Linux (where Qt currently always reports
    // Granted, so reaching this branch usually means a custom Qt build
    // or a future Linux permission system).
    QString detail;
#ifdef Q_OS_WIN
    detail = tr(
        "Open Settings → Privacy & security → Microphone and enable both:\n"
        "  • \"Microphone access\"\n"
        "  • \"Let desktop apps access your microphone\"\n\n"
        "If ContestLogX is listed below the toggle, make sure it is allowed. "
        "Then click Start in the CW Decoder again.");
#elif defined(Q_OS_MACOS)
    detail = tr(
        "Open System Settings → Privacy & Security → Microphone and enable "
        "ContestLogX. You may need to restart the app for the change to take "
        "effect, then click Start in the CW Decoder again.");
#else
    detail = tr(
        "ContestLogX is not allowed to access the microphone. Check your "
        "platform's audio permission settings, then click Start in the CW "
        "Decoder again.");
#endif

    QMessageBox::warning(
        this,
        tr("CW Decoder — microphone access denied"),
        tr("The operating system has not granted ContestLogX access to your "
           "microphone, so the CW Decoder cannot capture audio.\n\n%1").arg(detail));
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
    updateStartStopButton();
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

void CwDecoderWidget::setBaseFont(const QFont& font)
{
    if (font.family().isEmpty()) return;
    // Apply to the whole widget so toolbar controls (combos, spins,
    // labels) and the per-bin rows (freq label, WPM label, and the
    // scrolling decoded text) all pick it up consistently. Individual
    // children can still override via stylesheet if needed.
    this->setFont(font);
    for (CwDecoderRow& row : m_rows) {
        if (row.container) row.container->setFont(font);
        if (row.freqLabel) row.freqLabel->setFont(font);
        if (row.wpmLabel)  row.wpmLabel->setFont(font);
        if (row.text)      row.text->setFont(font);
    }
}

void CwDecoderWidget::setContestEngine(ContestEngine* engine)
{
    m_contestEngine = engine;
    refreshPracticeContestAvailability();
    // ContestEngine doesn't expose a "contest loaded/unloaded" signal, so
    // MainWindow is responsible for calling refreshPracticeContestAvailability()
    // again whenever it swaps the active contest.
}

void CwDecoderWidget::refreshPracticeContestAvailability()
{
    if (!m_audioDeviceCombo) return;
    const int idx = m_audioDeviceCombo->findData(QStringLiteral("practice-test"));
    if (idx < 0) return;
    const bool haveContest = m_contestEngine
                             && !m_contestEngine->getContestName().isEmpty();
    // Disable the item via the model (QComboBox items can be disabled by
    // clearing the ItemIsEnabled flag on their model index).
    if (auto* model = qobject_cast<QStandardItemModel*>(m_audioDeviceCombo->model())) {
        if (auto* item = model->item(idx)) {
            item->setFlags(haveContest
                           ? item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable
                           : item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            item->setToolTip(haveContest
                             ? QString()
                             : tr("Load a contest first"));
        }
    }
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
    // Re-apply the configured CW-decoder font after rebuild so the
    // newly-created per-bin text edits inherit the right font (Qt doesn't
    // propagate a previously-set font to children spawned after the call).
    const QFont decoderFont = Settings::instance().getPanelFont(QStringLiteral("cwDecoder"));
    if (!decoderFont.family().isEmpty()) {
        setBaseFont(decoderFont);
    }

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

void CwDecoderWidget::onStartStopClicked()
{
    if (m_worker) {
        // Currently running — stop the worker (and tear down any
        // PracticeAudioSource the worker owns, which silences practice
        // audio on the speakers as well).
        endDecoding();
        return;
    }
    // Currently stopped — re-start against whatever device the combo
    // is showing. "(none)" is a no-op (button stays as "Start").
    if (!m_audioDeviceCombo) return;
    const QString device =
        m_audioDeviceCombo->itemData(m_audioDeviceCombo->currentIndex()).toString();
    if (device.isEmpty()) return;
    beginDecoding(device);
}

void CwDecoderWidget::updateStartStopButton()
{
    if (!m_startStopButton) return;
    const bool running = (m_worker != nullptr);
    m_startStopButton->setText(running ? tr("Stop") : tr("Start"));
    // With "(none)" selected there's nothing to start; disable the button
    // in that case so the operator picks a device first.
    const QString device = m_audioDeviceCombo
        ? m_audioDeviceCombo->itemData(m_audioDeviceCombo->currentIndex()).toString()
        : QString();
    m_startStopButton->setEnabled(running || !device.isEmpty());
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

void CwDecoderWidget::onWordGapChanged(double multiplier)
{
    if (m_applyingSettings) return;
    saveSettings();
    if (!m_worker) return;
    QMetaObject::invokeMethod(m_worker, "setWordGapMultiplier", Qt::QueuedConnection,
                              Q_ARG(float, static_cast<float>(multiplier)));
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
