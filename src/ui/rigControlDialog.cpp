/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "rigControlDialog.h"
#include "settings.h"
#include "flrigClient.h"
#include "hamlibClient.h"
#include "mockedRigClient.h"
#include "winKeyerClient.h"
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QStyle>
#include <QLabel>
#include <QMessageBox>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QApplication>

RigControlDialog::RigControlDialog(RigInterface* clientL, RigInterface* clientR,
                                   bool so2rEnabled, QWidget *parent)
    : QDialog(parent)
    , m_so2rEnabled(so2rEnabled)
    , m_so2rCheck(nullptr)
    , m_pollIntervalSpin(nullptr)
    , m_tabWidget(nullptr)
    , m_radioRPage(nullptr)
{
    setWindowTitle("Rig Connection Settings");

    Settings& settings = Settings::instance();
    m_radioL.originalClient = clientL;
    m_radioL.rigClient = clientL;
    m_radioL.isRadioR = false;
    m_radioL.originalBackend = settings.getRigBackend();
    m_radioR.originalClient = clientR;
    m_radioR.rigClient = clientR;
    m_radioR.isRadioR = true;
    m_radioR.originalBackend = settings.getRadioRRigBackend();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Tab widget — always used, Radio R tab shown/hidden by SO2R checkbox
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createRadioPage(m_radioL), "Radio L");
    m_radioRPage = createRadioPage(m_radioR);
    if (m_so2rEnabled)
        m_tabWidget->addTab(m_radioRPage, "Radio R");
    mainLayout->addWidget(m_tabWidget);

    // SO2R checkbox + shared poll interval on same row
    QHBoxLayout *bottomRow = new QHBoxLayout();
    m_so2rCheck = new QCheckBox("SO2R (two radios)");
    m_so2rCheck->setChecked(m_so2rEnabled);
    bottomRow->addWidget(m_so2rCheck);
    bottomRow->addStretch();
    bottomRow->addWidget(new QLabel("Poll Interval:"));
    m_pollIntervalSpin = new QSpinBox();
    m_pollIntervalSpin->setRange(100, 5000);
    m_pollIntervalSpin->setSingleStep(100);
    m_pollIntervalSpin->setValue(Settings::instance().getFlrigPollInterval());
    m_pollIntervalSpin->setSuffix(" ms");
    bottomRow->addWidget(m_pollIntervalSpin);
    mainLayout->addLayout(bottomRow);

    mainLayout->addSpacing(8);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *btn = buttonBox->button(QDialogButtonBox::Ok))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    if (auto *btn = buttonBox->button(QDialogButtonBox::Cancel))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    mainLayout->addWidget(buttonBox);

    connect(m_so2rCheck, &QCheckBox::toggled, this, &RigControlDialog::onSo2rToggled);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RigControlDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Initialize pages (load settings, update status)
    initRadioPage(m_radioL);
    initRadioPage(m_radioR);
}

RigControlDialog::~RigControlDialog()
{
    // Clean up any temp clients not adopted on OK
    cleanupTempClient(m_radioL);
    cleanupTempClient(m_radioR);
}

void RigControlDialog::swapToTempClient(RadioWidgets& w, const QString& backend)
{
    cleanupTempClient(w);

    if (backend == "hamlib")
        w.tempClient = new HamlibClient(this);
    else if (backend == "mocked")
        w.tempClient = new MockedRigClient(this);
    else
        w.tempClient = new FlrigClient(this);

    w.rigClient = w.tempClient;

    w.statusLabel->setText("Disconnected");
    w.statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    w.rigNameLabel->setText("N/A");
    updateConnectionStatus(w);
}

void RigControlDialog::cleanupTempClient(RadioWidgets& w)
{
    if (w.tempClient) {
        if (w.tempClient->isConnected())
            w.tempClient->disconnectFromRig();
        delete w.tempClient;
        w.tempClient = nullptr;
    }
}

void RigControlDialog::onSo2rToggled(bool checked)
{
    if (checked) {
        m_tabWidget->addTab(m_radioRPage, "Radio R");
        m_tabWidget->setCurrentWidget(m_radioRPage);
    } else {
        int idx = m_tabWidget->indexOf(m_radioRPage);
        if (idx >= 0)
            m_tabWidget->removeTab(idx);
    }
}

QWidget* RigControlDialog::createRadioPage(RadioWidgets& w)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    // Backend selection
    QGroupBox *backendGroup = new QGroupBox("Rig Interface", page);
    QVBoxLayout *backendLayout = new QVBoxLayout(backendGroup);

    QHBoxLayout *comboLayout = new QHBoxLayout();
    comboLayout->addWidget(new QLabel("Backend:"));
    w.backendCombo = new QComboBox();
    w.backendCombo->addItem("flrig (XML-RPC)");
    w.backendCombo->addItem("Hamlib (rigctld)");
    w.backendCombo->addItem("Mocked (testing)");
    comboLayout->addWidget(w.backendCombo);
    comboLayout->addStretch();
    backendLayout->addLayout(comboLayout);

    w.featureNoteLabel = new QLabel();
    w.featureNoteLabel->setWordWrap(true);
    w.featureNoteLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    backendLayout->addWidget(w.featureNoteLabel);

    layout->addWidget(backendGroup);

    // Stacked settings pages
    w.settingsStack = new QStackedWidget(page);

    // --- flrig settings page ---
    QWidget *flrigPage = new QWidget();
    QFormLayout *flrigForm = new QFormLayout(flrigPage);

    w.flrigHostEdit = new QLineEdit();
    w.flrigHostEdit->setPlaceholderText("localhost or 127.0.0.1");
    flrigForm->addRow("Host:", w.flrigHostEdit);

    w.flrigPortSpin = new QSpinBox();
    w.flrigPortSpin->setRange(1, 65535);
    w.flrigPortSpin->setValue(12345);
    flrigForm->addRow("Port:", w.flrigPortSpin);

    w.flrigAutoConnectCheck = new QCheckBox("Auto-connect on startup");
    flrigForm->addRow("", w.flrigAutoConnectCheck);

    w.settingsStack->addWidget(flrigPage);

    // --- Hamlib settings page ---
    QWidget *hamlibPage = new QWidget();
    QFormLayout *hamlibForm = new QFormLayout(hamlibPage);

    w.hamlibHostEdit = new QLineEdit();
    w.hamlibHostEdit->setPlaceholderText("localhost or 127.0.0.1");
    hamlibForm->addRow("Host:", w.hamlibHostEdit);

    w.hamlibPortSpin = new QSpinBox();
    w.hamlibPortSpin->setRange(1, 65535);
    w.hamlibPortSpin->setValue(4532);
    hamlibForm->addRow("Port:", w.hamlibPortSpin);

    w.hamlibAutoConnectCheck = new QCheckBox("Auto-connect on startup");
    hamlibForm->addRow("", w.hamlibAutoConnectCheck);

    w.settingsStack->addWidget(hamlibPage);

    // --- Mocked settings page ---
    QWidget *mockedPage = new QWidget();
    QVBoxLayout *mockedLayout = new QVBoxLayout(mockedPage);
    w.mockedAutoConnectCheck = new QCheckBox("Auto-connect on startup");
    mockedLayout->addWidget(w.mockedAutoConnectCheck);
    mockedLayout->addStretch();
    w.settingsStack->addWidget(mockedPage);

    layout->addWidget(w.settingsStack);

    // Connection control buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    w.connectButton = new QPushButton("Connect");
    w.disconnectButton = new QPushButton("Disconnect");
    w.testButton = new QPushButton("Test");

    buttonLayout->addWidget(w.connectButton);
    buttonLayout->addWidget(w.disconnectButton);
    buttonLayout->addWidget(w.testButton);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // Status group
    QGroupBox *statusGroup = new QGroupBox("Status", page);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    w.statusLabel = new QLabel("Disconnected");
    w.statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(w.statusLabel);

    QLabel *rigLabel = new QLabel("Rig:");
    w.rigNameLabel = new QLabel("N/A");
    QHBoxLayout *rigLayout = new QHBoxLayout();
    rigLayout->addWidget(rigLabel);
    rigLayout->addWidget(w.rigNameLabel);
    rigLayout->addStretch();
    statusLayout->addLayout(rigLayout);

    layout->addWidget(statusGroup);

    // CW Decoder audio input (SPEC-005)
    QGroupBox* audioGroup = new QGroupBox("CW Decoder — Audio Input", page);
    QFormLayout* audioForm = new QFormLayout(audioGroup);
    w.audioInputCombo = new QComboBox(audioGroup);
    w.audioInputCombo->addItem("(none)", QString());
    for (const QAudioDevice& d : QMediaDevices::audioInputs()) {
        w.audioInputCombo->addItem(d.description(), d.description());
    }
    audioForm->addRow("Audio Input Device:", w.audioInputCombo);

    w.muteDecoderOnPttCheck = new QCheckBox("Mute decoder on PTT", audioGroup);
    w.muteDecoderOnPttCheck->setChecked(true);
    audioForm->addRow("", w.muteDecoderOnPttCheck);

    w.decoderPttGraceSpin = new QSpinBox(audioGroup);
    w.decoderPttGraceSpin->setRange(0, 2000);
    w.decoderPttGraceSpin->setSuffix(" ms");
    w.decoderPttGraceSpin->setValue(250);
    audioForm->addRow("PTT grace window:", w.decoderPttGraceSpin);

    layout->addWidget(audioGroup);

    // --- CW Keyer (WinKeyer) — independent of the rig backend ---
    QGroupBox* keyerGroup = new QGroupBox("CW Keyer", page);
    QFormLayout* keyerForm = new QFormLayout(keyerGroup);

    w.keyerSourceCombo = new QComboBox(keyerGroup);
    w.keyerSourceCombo->addItem("Rig backend (flrig/hamlib)", "rig");
    w.keyerSourceCombo->addItem("WinKeyer (serial)", "winkeyer");
    keyerForm->addRow("CW keying via:", w.keyerSourceCombo);

    QHBoxLayout* keyerPortRow = new QHBoxLayout();
    w.keyerPortCombo = new QComboBox(keyerGroup);
    w.keyerPortCombo->setEditable(true);  // allow a by-id path the list can't enumerate
    w.keyerPortCombo->setMinimumWidth(220);
    w.keyerRefreshButton = new QPushButton("Refresh", keyerGroup);
    keyerPortRow->addWidget(w.keyerPortCombo, 1);
    keyerPortRow->addWidget(w.keyerRefreshButton);
    keyerForm->addRow("Serial port:", keyerPortRow);

    w.keyerAutoConnectCheck = new QCheckBox("Connect automatically on startup", keyerGroup);
    keyerForm->addRow("", w.keyerAutoConnectCheck);

    QHBoxLayout* keyerDetectRow = new QHBoxLayout();
    w.keyerDetectButton = new QPushButton("Detect keyer", keyerGroup);
    w.keyerStatusLabel = new QLabel(QString(), keyerGroup);
    keyerDetectRow->addWidget(w.keyerDetectButton);
    keyerDetectRow->addWidget(w.keyerStatusLabel, 1);
    keyerForm->addRow("", keyerDetectRow);

    populateKeyerPorts(w);
    layout->addWidget(keyerGroup);

    // Attribution label
    w.attributionLabel = new QLabel(page);
    w.attributionLabel->setTextFormat(Qt::RichText);
    w.attributionLabel->setOpenExternalLinks(true);
    w.attributionLabel->setAlignment(Qt::AlignCenter);
    w.attributionLabel->setStyleSheet("QLabel { color: gray; }");
    layout->addWidget(w.attributionLabel);

    // Wire signals using lambdas that capture the RadioWidgets reference
    connect(w.backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, &w](int index) { onBackendChanged(w, index); });
    connect(w.connectButton, &QPushButton::clicked,
            this, [this, &w]() { onConnectClicked(w); });
    connect(w.disconnectButton, &QPushButton::clicked,
            this, [this, &w]() { onDisconnectClicked(w); });
    connect(w.testButton, &QPushButton::clicked,
            this, [this, &w]() { onTestClicked(w); });
    connect(w.keyerSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, &w](int) { onKeyerSourceChanged(w); });
    connect(w.keyerRefreshButton, &QPushButton::clicked,
            this, [this, &w]() { populateKeyerPorts(w); });
    connect(w.keyerDetectButton, &QPushButton::clicked,
            this, [this, &w]() { onKeyerDetectClicked(w); });

    return page;
}

void RigControlDialog::initRadioPage(RadioWidgets& w)
{
    loadSettings(w);

    // Update status based on actual connection state
    if (w.rigClient && w.rigClient->isConnected()) {
        w.statusLabel->setText("Connected");
        w.statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        QString rigName = w.rigClient->getRigName();
        if (!rigName.isEmpty())
            w.rigNameLabel->setText(rigName);
    }
    updateConnectionStatus(w);
}

void RigControlDialog::onBackendChanged(RadioWidgets& w, int index)
{
    w.settingsStack->setCurrentIndex(index);

    if (index == 0) {
        w.featureNoteLabel->setText(
            "flrig provides full rig control including CW keying, PTT, power, and bandwidth.");
        w.attributionLabel->setText(
            "Rig control powered by <b>flrig</b>"
            " — <a href=\"https://www.w1hkj.org/\">https://www.w1hkj.org/</a>");
    } else if (index == 1) {
        w.featureNoteLabel->setText(
            "Hamlib provides frequency and mode control. CW keying and other features "
            "depend on rig capabilities. Requires rigctld to be running.");
        w.attributionLabel->setText(
            "Rig control powered by <b>Hamlib</b>"
            " — <a href=\"https://hamlib.github.io/\">https://hamlib.github.io/</a>");
    } else {
        w.featureNoteLabel->setText(
            "Simulated rig for testing and SO2R practice. No real hardware required. "
            "Defaults to 14.200 MHz USB.");
        w.attributionLabel->setText("");
    }

    // Swap to temp client if backend changed, or restore original
    QString backend = selectedBackend(w);
    if (backend != w.originalBackend) {
        swapToTempClient(w, backend);
    } else {
        cleanupTempClient(w);
        w.rigClient = w.originalClient;
        // Restore original status display
        if (w.rigClient && w.rigClient->isConnected()) {
            w.statusLabel->setText("Connected");
            w.statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
            QString rigName = w.rigClient->getRigName();
            if (!rigName.isEmpty())
                w.rigNameLabel->setText(rigName);
        } else {
            w.statusLabel->setText("Disconnected");
            w.statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
            w.rigNameLabel->setText("N/A");
        }
        updateConnectionStatus(w);
    }
}

void RigControlDialog::loadSettings(RadioWidgets& w)
{
    Settings& settings = Settings::instance();

    auto backendToIndex = [](const QString& b) {
        if (b == "hamlib") return 1;
        if (b == "mocked") return 2;
        return 0;  // flrig
    };

    if (w.isRadioR) {
        w.backendCombo->setCurrentIndex(backendToIndex(settings.getRadioRRigBackend()));

        w.flrigHostEdit->setText(settings.getRadioRFlrigHost());
        w.flrigPortSpin->setValue(settings.getRadioRFlrigPort());
        w.flrigAutoConnectCheck->setChecked(settings.getRadioRFlrigAutoConnect());

        w.hamlibHostEdit->setText(settings.getRadioRHamlibHost());
        w.hamlibPortSpin->setValue(settings.getRadioRHamlibPort());
        w.hamlibAutoConnectCheck->setChecked(settings.getRadioRHamlibAutoConnect());

        w.mockedAutoConnectCheck->setChecked(settings.getRadioRMockedAutoConnect());
    } else {
        w.backendCombo->setCurrentIndex(backendToIndex(settings.getRigBackend()));

        w.flrigHostEdit->setText(settings.getFlrigHost());
        w.flrigPortSpin->setValue(settings.getFlrigPort());
        w.flrigAutoConnectCheck->setChecked(settings.getFlrigAutoConnect());

        w.hamlibHostEdit->setText(settings.getHamlibHost());
        w.hamlibPortSpin->setValue(settings.getHamlibPort());
        w.hamlibAutoConnectCheck->setChecked(settings.getHamlibAutoConnect());

        w.mockedAutoConnectCheck->setChecked(settings.getMockedAutoConnect());
    }

    // Audio input device + mute-on-PTT (SPEC-005)
    if (w.audioInputCombo) {
        const QString persistedDevice = w.isRadioR
            ? settings.getRadioRAudioInputDevice()
            : settings.getRadioLAudioInputDevice();
        int idx = w.audioInputCombo->findData(persistedDevice);
        w.audioInputCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (w.muteDecoderOnPttCheck) {
        w.muteDecoderOnPttCheck->setChecked(
            w.isRadioR ? settings.getRadioRMuteDecoderOnPtt()
                       : settings.getRadioLMuteDecoderOnPtt());
    }
    if (w.decoderPttGraceSpin) {
        w.decoderPttGraceSpin->setValue(
            w.isRadioR ? settings.getRadioRDecoderPttGraceMs()
                       : settings.getRadioLDecoderPttGraceMs());
    }

    // CW keyer (WinKeyer) — per radio
    if (w.keyerSourceCombo) {
        const QString src = settings.getCwKeyerSource(w.isRadioR);
        int si = w.keyerSourceCombo->findData(src);
        w.keyerSourceCombo->setCurrentIndex(si >= 0 ? si : 0);
    }
    if (w.keyerPortCombo) {
        const QString port = settings.getCwKeyerPort(w.isRadioR);
        int pi = w.keyerPortCombo->findData(port);
        if (pi >= 0) w.keyerPortCombo->setCurrentIndex(pi);
        else if (!port.isEmpty()) w.keyerPortCombo->setEditText(port);
    }
    if (w.keyerAutoConnectCheck)
        w.keyerAutoConnectCheck->setChecked(settings.getCwKeyerAutoConnect(w.isRadioR));
    onKeyerSourceChanged(w);  // enable/disable the WinKeyer rows for current source

    // Trigger UI update for current backend
    onBackendChanged(w, w.backendCombo->currentIndex());
}

void RigControlDialog::saveSettings(RadioWidgets& w)
{
    Settings& settings = Settings::instance();

    if (w.isRadioR) {
        settings.setRadioRRigBackend(selectedBackend(w));

        settings.setRadioRFlrigHost(w.flrigHostEdit->text());
        settings.setRadioRFlrigPort(w.flrigPortSpin->value());
        settings.setRadioRFlrigAutoConnect(w.flrigAutoConnectCheck->isChecked());

        settings.setRadioRHamlibHost(w.hamlibHostEdit->text());
        settings.setRadioRHamlibPort(w.hamlibPortSpin->value());
        settings.setRadioRHamlibAutoConnect(w.hamlibAutoConnectCheck->isChecked());

        settings.setRadioRMockedAutoConnect(w.mockedAutoConnectCheck->isChecked());
    } else {
        settings.setRigBackend(selectedBackend(w));

        settings.setFlrigHost(w.flrigHostEdit->text());
        settings.setFlrigPort(w.flrigPortSpin->value());
        settings.setFlrigAutoConnect(w.flrigAutoConnectCheck->isChecked());

        settings.setHamlibHost(w.hamlibHostEdit->text());
        settings.setHamlibPort(w.hamlibPortSpin->value());
        settings.setHamlibAutoConnect(w.hamlibAutoConnectCheck->isChecked());

        settings.setMockedAutoConnect(w.mockedAutoConnectCheck->isChecked());
    }

    // Audio input device + mute-on-PTT (SPEC-005). Detect change so that
    // MainWindow can (re)spawn the CwDecoderWidget appropriately.
    QString newAudioDevice;
    bool newMuteOnPtt = true;
    int newGrace = 250;
    if (w.audioInputCombo) newAudioDevice = w.audioInputCombo->currentData().toString();
    if (w.muteDecoderOnPttCheck) newMuteOnPtt = w.muteDecoderOnPttCheck->isChecked();
    if (w.decoderPttGraceSpin) newGrace = w.decoderPttGraceSpin->value();

    QString oldAudioDevice;
    bool oldMuteOnPtt = true;
    int oldGrace = 250;
    if (w.isRadioR) {
        oldAudioDevice = settings.getRadioRAudioInputDevice();
        oldMuteOnPtt   = settings.getRadioRMuteDecoderOnPtt();
        oldGrace       = settings.getRadioRDecoderPttGraceMs();
        settings.setRadioRAudioInputDevice(newAudioDevice);
        settings.setRadioRMuteDecoderOnPtt(newMuteOnPtt);
        settings.setRadioRDecoderPttGraceMs(newGrace);
    } else {
        oldAudioDevice = settings.getRadioLAudioInputDevice();
        oldMuteOnPtt   = settings.getRadioLMuteDecoderOnPtt();
        oldGrace       = settings.getRadioLDecoderPttGraceMs();
        settings.setRadioLAudioInputDevice(newAudioDevice);
        settings.setRadioLMuteDecoderOnPtt(newMuteOnPtt);
        settings.setRadioLDecoderPttGraceMs(newGrace);
    }
    if (oldAudioDevice != newAudioDevice || oldMuteOnPtt != newMuteOnPtt || oldGrace != newGrace) {
        emit audioConfigChanged(w.isRadioR);
    }

    // CW keyer (WinKeyer) — per radio. Detect change so MainWindow can
    // (re)create the WinKeyer client(s).
    if (w.keyerSourceCombo && w.keyerPortCombo && w.keyerAutoConnectCheck) {
        const QString newSource = w.keyerSourceCombo->currentData().toString();
        QString newPort = w.keyerPortCombo->currentData().toString();
        if (newPort.isEmpty()) newPort = w.keyerPortCombo->currentText().trimmed();
        const bool newAuto = w.keyerAutoConnectCheck->isChecked();

        const bool keyerChanged =
            settings.getCwKeyerSource(w.isRadioR) != newSource ||
            settings.getCwKeyerPort(w.isRadioR) != newPort ||
            settings.getCwKeyerAutoConnect(w.isRadioR) != newAuto;

        settings.setCwKeyerSource(w.isRadioR, newSource);
        settings.setCwKeyerPort(w.isRadioR, newPort);
        settings.setCwKeyerAutoConnect(w.isRadioR, newAuto);

        if (keyerChanged)
            emit cwKeyerConfigChanged();
    }
}

void RigControlDialog::populateKeyerPorts(RadioWidgets& w)
{
    if (!w.keyerPortCombo) return;
    const QString current = w.keyerPortCombo->currentText();
    w.keyerPortCombo->clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        const QString label = info.description().isEmpty()
            ? info.portName()
            : QString("%1 — %2").arg(info.portName(), info.description());
        w.keyerPortCombo->addItem(label, info.systemLocation());
    }
    if (!current.isEmpty()) {
        int idx = w.keyerPortCombo->findData(current);
        if (idx >= 0) w.keyerPortCombo->setCurrentIndex(idx);
        else w.keyerPortCombo->setEditText(current);
    }
}

void RigControlDialog::onKeyerSourceChanged(RadioWidgets& w)
{
    if (!w.keyerSourceCombo) return;
    const bool winkeyer = (w.keyerSourceCombo->currentData().toString() == "winkeyer");
    if (w.keyerPortCombo)        w.keyerPortCombo->setEnabled(winkeyer);
    if (w.keyerRefreshButton)    w.keyerRefreshButton->setEnabled(winkeyer);
    if (w.keyerAutoConnectCheck) w.keyerAutoConnectCheck->setEnabled(winkeyer);
    if (w.keyerDetectButton)     w.keyerDetectButton->setEnabled(winkeyer);
    if (!winkeyer && w.keyerStatusLabel) w.keyerStatusLabel->clear();
}

void RigControlDialog::onKeyerDetectClicked(RadioWidgets& w)
{
    if (!w.keyerPortCombo || !w.keyerStatusLabel) return;
    QString port = w.keyerPortCombo->currentData().toString();
    if (port.isEmpty()) port = w.keyerPortCombo->currentText().trimmed();
    if (port.isEmpty()) {
        w.keyerStatusLabel->setText("<span style='color:orange;'>Select a serial port first</span>");
        return;
    }

    w.keyerStatusLabel->setText("Detecting…");
    w.keyerDetectButton->setEnabled(false);
    QApplication::processEvents();

    WinKeyerClient probe;
    if (probe.openPort(port)) {
        const int rev = probe.revision();
        w.keyerStatusLabel->setText(
            QString("<span style='color:green;'>WinKey detected, rev 0x%1 ✓</span>")
                .arg(rev, 2, 16, QChar('0')));
        probe.closePort();
    } else {
        w.keyerStatusLabel->setText(
            "<span style='color:red;'>No WinKeyer response (check port)</span>");
    }
    w.keyerDetectButton->setEnabled(true);
}

QString RigControlDialog::selectedBackend(const RadioWidgets& w) const
{
    switch (w.backendCombo->currentIndex()) {
    case 0: return "flrig";
    case 1: return "hamlib";
    case 2: return "mocked";
    default: return "flrig";
    }
}

void RigControlDialog::onConnectClicked(RadioWidgets& w)
{
    if (!w.rigClient) {
        QMessageBox::information(this, "SO2R Not Active",
            "Click OK to enable SO2R, then reopen this dialog to connect Radio R.");
        return;
    }

    QString backend = selectedBackend(w);
    QString host;
    int port = 0;

    if (backend == "flrig") {
        host = w.flrigHostEdit->text();
        port = w.flrigPortSpin->value();
    } else if (backend == "hamlib") {
        host = w.hamlibHostEdit->text();
        port = w.hamlibPortSpin->value();
    } else {
        host = "mocked";
    }

    if (host.isEmpty()) {
        QMessageBox::warning(this, "Invalid Host", "Please enter a host name or IP address.");
        return;
    }

    w.statusLabel->setText("Connecting...");
    w.statusLabel->setStyleSheet("QLabel { color: orange; font-weight: bold; }");

    if (w.rigClient->connectToRig(host, port)) {
        w.statusLabel->setText("Connected");
        w.statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");

        QString rigName = w.rigClient->getRigName();
        if (!rigName.isEmpty())
            w.rigNameLabel->setText(rigName);

        // Update auto-connect checkboxes in the UI (saved to Settings on OK)
        if (backend == "flrig") w.flrigAutoConnectCheck->setChecked(true);
        else if (backend == "hamlib") w.hamlibAutoConnectCheck->setChecked(true);
        else w.mockedAutoConnectCheck->setChecked(true);

        updateConnectionStatus(w);
    } else {
        w.statusLabel->setText("Connection Failed");
        w.statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");

        QString serverName = backend == "flrig" ? "flrig" : "rigctld";
        QMessageBox::critical(this, "Connection Failed",
            QString("Failed to connect to %1 server.\n\n"
                    "Make sure %1 is running and configured to accept connections.")
                .arg(serverName));
    }
}

void RigControlDialog::onDisconnectClicked(RadioWidgets& w)
{
    if (!w.rigClient) return;
    w.rigClient->disconnectFromRig();
    w.statusLabel->setText("Disconnected");
    w.statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    w.rigNameLabel->setText("N/A");

    // Update auto-connect checkboxes in the UI (saved to Settings on OK)
    w.flrigAutoConnectCheck->setChecked(false);
    w.hamlibAutoConnectCheck->setChecked(false);
    w.mockedAutoConnectCheck->setChecked(false);

    updateConnectionStatus(w);
}

void RigControlDialog::onTestClicked(RadioWidgets& w)
{
    if (!w.rigClient || !w.rigClient->isConnected()) {
        QString serverName = selectedBackend(w) == "flrig" ? "flrig" : "rigctld";
        QMessageBox::information(this, "Not Connected",
            QString("Please connect to %1 first.").arg(serverName));
        return;
    }

    double freq = w.rigClient->getFrequency();
    QString mode = w.rigClient->getMode();

    QString msg = QString("Current Settings:\n\nFrequency: %1 Hz\nMode: %2")
        .arg(QString::number(freq, 'f', 0))
        .arg(mode);

    QMessageBox::information(this, "Rig Test", msg);
}

void RigControlDialog::onAccepted()
{
    bool so2rNow = m_so2rCheck->isChecked();

    // Save settings for both radios (Radio R settings are always saveable)
    saveSettings(m_radioL);
    saveSettings(m_radioR);

    // Shared poll interval
    Settings::instance().setFlrigPollInterval(m_pollIntervalSpin->value());

    // Notify about SO2R change first (creates/destroys Radio R client)
    if (so2rNow != m_so2rEnabled)
        emit so2rChanged(so2rNow);

    // Notify about backend changes (only if actually changed)
    // Clean up temp clients — MainWindow will create its own
    QString backendL = selectedBackend(m_radioL);
    if (backendL != m_radioL.originalBackend) {
        cleanupTempClient(m_radioL);
        emit backendChanged(backendL);
    }

    QString backendR = selectedBackend(m_radioR);
    if (so2rNow && backendR != m_radioR.originalBackend) {
        cleanupTempClient(m_radioR);
        emit backendChangedR(backendR);
    }

    // Apply poll interval change
    emit pollIntervalChanged(m_pollIntervalSpin->value());

    accept();
}

void RigControlDialog::updateConnectionStatus(RadioWidgets& w)
{
    if (!w.rigClient) {
        w.connectButton->setEnabled(false);
        w.disconnectButton->setEnabled(false);
        w.testButton->setEnabled(false);
        return;
    }
    bool connected = w.rigClient->isConnected();
    w.connectButton->setEnabled(!connected);
    w.disconnectButton->setEnabled(connected);
    w.testButton->setEnabled(connected);
}
