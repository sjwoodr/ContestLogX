/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "rigControlDialog.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QStyle>
#include <QLabel>
#include <QMessageBox>

RigControlDialog::RigControlDialog(RigInterface* client, QWidget *parent, bool isRadioR)
    : QDialog(parent)
    , m_rigClient(client)
    , m_isRadioR(isRadioR)
{
    setWindowTitle("Rig Connection Settings");
    setupUi();
    loadSettings();

    // Update status based on actual connection state
    if (m_rigClient->isConnected()) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        QString rigName = m_rigClient->getRigName();
        if (!rigName.isEmpty()) {
            m_rigNameLabel->setText(rigName);
        }
    }
    updateConnectionStatus();
}

RigControlDialog::~RigControlDialog()
{
}

QString RigControlDialog::selectedBackend() const
{
    return m_backendCombo->currentIndex() == 0 ? "flrig" : "hamlib";
}

void RigControlDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Backend selection
    QGroupBox *backendGroup = new QGroupBox("Rig Interface", this);
    QVBoxLayout *backendLayout = new QVBoxLayout(backendGroup);

    QHBoxLayout *comboLayout = new QHBoxLayout();
    comboLayout->addWidget(new QLabel("Backend:"));
    m_backendCombo = new QComboBox();
    m_backendCombo->addItem("flrig (XML-RPC)");
    m_backendCombo->addItem("Hamlib (rigctld)");
    comboLayout->addWidget(m_backendCombo);
    comboLayout->addStretch();
    backendLayout->addLayout(comboLayout);

    m_featureNoteLabel = new QLabel();
    m_featureNoteLabel->setWordWrap(true);
    m_featureNoteLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    backendLayout->addWidget(m_featureNoteLabel);

    mainLayout->addWidget(backendGroup);

    // Stacked settings pages
    m_settingsStack = new QStackedWidget(this);

    // --- flrig settings page ---
    QWidget *flrigPage = new QWidget();
    QFormLayout *flrigForm = new QFormLayout(flrigPage);

    m_flrigHostEdit = new QLineEdit();
    m_flrigHostEdit->setPlaceholderText("localhost or 127.0.0.1");
    flrigForm->addRow("Host:", m_flrigHostEdit);

    m_flrigPortSpin = new QSpinBox();
    m_flrigPortSpin->setRange(1, 65535);
    m_flrigPortSpin->setValue(12345);
    flrigForm->addRow("Port:", m_flrigPortSpin);

    m_flrigAutoConnectCheck = new QCheckBox("Auto-connect on startup");
    flrigForm->addRow("", m_flrigAutoConnectCheck);

    m_settingsStack->addWidget(flrigPage);

    // --- Hamlib settings page ---
    QWidget *hamlibPage = new QWidget();
    QFormLayout *hamlibForm = new QFormLayout(hamlibPage);

    m_hamlibHostEdit = new QLineEdit();
    m_hamlibHostEdit->setPlaceholderText("localhost or 127.0.0.1");
    hamlibForm->addRow("Host:", m_hamlibHostEdit);

    m_hamlibPortSpin = new QSpinBox();
    m_hamlibPortSpin->setRange(1, 65535);
    m_hamlibPortSpin->setValue(4532);
    hamlibForm->addRow("Port:", m_hamlibPortSpin);

    m_hamlibAutoConnectCheck = new QCheckBox("Auto-connect on startup");
    hamlibForm->addRow("", m_hamlibAutoConnectCheck);

    m_settingsStack->addWidget(hamlibPage);

    mainLayout->addWidget(m_settingsStack);

    // Shared poll interval
    QHBoxLayout *pollLayout = new QHBoxLayout();
    pollLayout->addWidget(new QLabel("Poll Interval:"));
    m_pollIntervalSpin = new QSpinBox();
    m_pollIntervalSpin->setRange(100, 5000);
    m_pollIntervalSpin->setSingleStep(100);
    m_pollIntervalSpin->setValue(500);
    m_pollIntervalSpin->setSuffix(" ms");
    pollLayout->addWidget(m_pollIntervalSpin);
    pollLayout->addStretch();
    mainLayout->addLayout(pollLayout);

    // Connection control buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton("Connect");
    m_disconnectButton = new QPushButton("Disconnect");
    m_testButton = new QPushButton("Test");

    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    buttonLayout->addWidget(m_testButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // Status group
    QGroupBox *statusGroup = new QGroupBox("Status", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    m_statusLabel = new QLabel("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);

    QLabel *rigLabel = new QLabel("Rig:");
    m_rigNameLabel = new QLabel("N/A");
    QHBoxLayout *rigLayout = new QHBoxLayout();
    rigLayout->addWidget(rigLabel);
    rigLayout->addWidget(m_rigNameLabel);
    rigLayout->addStretch();
    statusLayout->addLayout(rigLayout);

    mainLayout->addWidget(statusGroup);

    // Attribution label (changes with backend)
    m_attributionLabel = new QLabel(this);
    m_attributionLabel->setTextFormat(Qt::RichText);
    m_attributionLabel->setOpenExternalLinks(true);
    m_attributionLabel->setAlignment(Qt::AlignCenter);
    m_attributionLabel->setStyleSheet("QLabel { color: gray; }");
    mainLayout->addWidget(m_attributionLabel);
    mainLayout->addSpacing(8);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *btn = buttonBox->button(QDialogButtonBox::Ok))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    if (auto *btn = buttonBox->button(QDialogButtonBox::Cancel))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    mainLayout->addWidget(buttonBox);

    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RigControlDialog::onBackendChanged);
    connect(m_connectButton, &QPushButton::clicked, this, &RigControlDialog::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &RigControlDialog::onDisconnectClicked);
    connect(m_testButton, &QPushButton::clicked, this, &RigControlDialog::onTestClicked);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RigControlDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void RigControlDialog::onBackendChanged(int index)
{
    m_settingsStack->setCurrentIndex(index);

    if (index == 0) {
        // flrig
        m_featureNoteLabel->setText(
            "flrig provides full rig control including CW keying, PTT, power, and bandwidth.");
        m_attributionLabel->setText(
            "Rig control powered by <b>flrig</b>"
            " — <a href=\"https://www.w1hkj.org/\">https://www.w1hkj.org/</a>");
    } else {
        // Hamlib
        m_featureNoteLabel->setText(
            "Hamlib provides frequency and mode control. CW keying and other features "
            "depend on rig capabilities. Requires rigctld to be running.");
        m_attributionLabel->setText(
            "Rig control powered by <b>Hamlib</b>"
            " — <a href=\"https://hamlib.github.io/\">https://hamlib.github.io/</a>");
    }
}

void RigControlDialog::loadSettings()
{
    Settings& settings = Settings::instance();

    if (m_isRadioR) {
        // Radio R settings
        QString backend = settings.getRadioRRigBackend();
        m_backendCombo->setCurrentIndex(backend == "hamlib" ? 1 : 0);

        m_flrigHostEdit->setText(settings.getRadioRFlrigHost());
        m_flrigPortSpin->setValue(settings.getRadioRFlrigPort());
        m_flrigAutoConnectCheck->setChecked(settings.getRadioRFlrigAutoConnect());

        m_hamlibHostEdit->setText(settings.getRadioRHamlibHost());
        m_hamlibPortSpin->setValue(settings.getRadioRHamlibPort());
        m_hamlibAutoConnectCheck->setChecked(settings.getRadioRHamlibAutoConnect());
    } else {
        // Radio L settings (existing)
        QString backend = settings.getRigBackend();
        m_backendCombo->setCurrentIndex(backend == "hamlib" ? 1 : 0);

        m_flrigHostEdit->setText(settings.getFlrigHost());
        m_flrigPortSpin->setValue(settings.getFlrigPort());
        m_flrigAutoConnectCheck->setChecked(settings.getFlrigAutoConnect());

        m_hamlibHostEdit->setText(settings.getHamlibHost());
        m_hamlibPortSpin->setValue(settings.getHamlibPort());
        m_hamlibAutoConnectCheck->setChecked(settings.getHamlibAutoConnect());
    }

    // Shared
    m_pollIntervalSpin->setValue(settings.getFlrigPollInterval());

    // Trigger UI update for current backend
    onBackendChanged(m_backendCombo->currentIndex());
}

void RigControlDialog::saveSettings()
{
    Settings& settings = Settings::instance();

    if (m_isRadioR) {
        // Radio R settings
        settings.setRadioRRigBackend(selectedBackend());

        settings.setRadioRFlrigHost(m_flrigHostEdit->text());
        settings.setRadioRFlrigPort(m_flrigPortSpin->value());
        settings.setRadioRFlrigAutoConnect(m_flrigAutoConnectCheck->isChecked());

        settings.setRadioRHamlibHost(m_hamlibHostEdit->text());
        settings.setRadioRHamlibPort(m_hamlibPortSpin->value());
        settings.setRadioRHamlibAutoConnect(m_hamlibAutoConnectCheck->isChecked());
    } else {
        // Radio L settings (existing)
        settings.setRigBackend(selectedBackend());

        settings.setFlrigHost(m_flrigHostEdit->text());
        settings.setFlrigPort(m_flrigPortSpin->value());
        settings.setFlrigAutoConnect(m_flrigAutoConnectCheck->isChecked());

        settings.setHamlibHost(m_hamlibHostEdit->text());
        settings.setHamlibPort(m_hamlibPortSpin->value());
        settings.setHamlibAutoConnect(m_hamlibAutoConnectCheck->isChecked());
    }

    // Shared
    settings.setFlrigPollInterval(m_pollIntervalSpin->value());
}

void RigControlDialog::onConnectClicked()
{
    QString host;
    int port;

    if (selectedBackend() == "flrig") {
        host = m_flrigHostEdit->text();
        port = m_flrigPortSpin->value();
    } else {
        host = m_hamlibHostEdit->text();
        port = m_hamlibPortSpin->value();
    }

    if (host.isEmpty()) {
        QMessageBox::warning(this, "Invalid Host", "Please enter a host name or IP address.");
        return;
    }

    m_statusLabel->setText("Connecting...");
    m_statusLabel->setStyleSheet("QLabel { color: orange; font-weight: bold; }");

    if (m_rigClient->connectToRig(host, port)) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");

        QString rigName = m_rigClient->getRigName();
        if (!rigName.isEmpty()) {
            m_rigNameLabel->setText(rigName);
        }

        // Save connection settings on successful connection
        Settings& settings = Settings::instance();
        if (m_isRadioR) {
            if (selectedBackend() == "flrig") {
                settings.setRadioRFlrigHost(host);
                settings.setRadioRFlrigPort(port);
                settings.setRadioRFlrigAutoConnect(true);
            } else {
                settings.setRadioRHamlibHost(host);
                settings.setRadioRHamlibPort(port);
                settings.setRadioRHamlibAutoConnect(true);
            }
            settings.setRadioRRigBackend(selectedBackend());
        } else {
            if (selectedBackend() == "flrig") {
                settings.setFlrigHost(host);
                settings.setFlrigPort(port);
                settings.setFlrigAutoConnect(true);
            } else {
                settings.setHamlibHost(host);
                settings.setHamlibPort(port);
                settings.setHamlibAutoConnect(true);
            }
            settings.setRigBackend(selectedBackend());
        }
        m_flrigAutoConnectCheck->setChecked(true);

        updateConnectionStatus();
    } else {
        m_statusLabel->setText("Connection Failed");
        m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");

        QString serverName = selectedBackend() == "flrig" ? "flrig" : "rigctld";
        QMessageBox::critical(this, "Connection Failed",
            QString("Failed to connect to %1 server.\n\n"
                    "Make sure %1 is running and configured to accept connections.")
                .arg(serverName));
    }
}

void RigControlDialog::onDisconnectClicked()
{
    m_rigClient->disconnectFromRig();
    m_statusLabel->setText("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_rigNameLabel->setText("N/A");

    // Disable auto-connect when user manually disconnects
    Settings& settings = Settings::instance();
    if (m_isRadioR) {
        if (selectedBackend() == "flrig") {
            settings.setRadioRFlrigAutoConnect(false);
        } else {
            settings.setRadioRHamlibAutoConnect(false);
        }
    } else {
        if (selectedBackend() == "flrig") {
            settings.setFlrigAutoConnect(false);
        } else {
            settings.setHamlibAutoConnect(false);
        }
    }
    m_flrigAutoConnectCheck->setChecked(false);
    m_hamlibAutoConnectCheck->setChecked(false);

    updateConnectionStatus();
}

void RigControlDialog::onTestClicked()
{
    if (!m_rigClient->isConnected()) {
        QString serverName = selectedBackend() == "flrig" ? "flrig" : "rigctld";
        QMessageBox::information(this, "Not Connected",
            QString("Please connect to %1 first.").arg(serverName));
        return;
    }

    double freq = m_rigClient->getFrequency();
    QString mode = m_rigClient->getMode();

    QString msg = QString("Current Settings:\n\nFrequency: %1 Hz\nMode: %2")
        .arg(QString::number(freq, 'f', 0))
        .arg(mode);

    QMessageBox::information(this, "Rig Test", msg);
}

void RigControlDialog::onAccepted()
{
    saveSettings();

    // Notify about backend change
    emit backendChanged(selectedBackend());

    // Apply poll interval change immediately if connected
    if (m_rigClient->isConnected()) {
        emit pollIntervalChanged(m_pollIntervalSpin->value());
    }

    accept();
}

void RigControlDialog::onRigConnected()
{
    m_statusLabel->setText("Connected");
    m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    updateConnectionStatus();
}

void RigControlDialog::onRigDisconnected()
{
    m_statusLabel->setText("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_rigNameLabel->setText("N/A");
    updateConnectionStatus();
}

void RigControlDialog::onRigError(const QString& error)
{
    m_statusLabel->setText("Error: " + error);
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
}

void RigControlDialog::updateConnectionStatus()
{
    bool connected = m_rigClient->isConnected();
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_testButton->setEnabled(connected);
}
