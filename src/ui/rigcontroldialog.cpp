/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "rigcontroldialog.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>

RigControlDialog::RigControlDialog(FlrigClient* client, QWidget *parent)
    : QDialog(parent)
    , m_flrigClient(client)
    , m_hostEdit(nullptr)
    , m_portSpin(nullptr)
    , m_connectButton(nullptr)
    , m_disconnectButton(nullptr)
    , m_testButton(nullptr)
    , m_statusLabel(nullptr)
    , m_rigNameLabel(nullptr)
{
    setWindowTitle("flrig Connection Settings");
    setupUi();
    loadSettings();
    
    // Update status based on actual connection state
    if (m_flrigClient->isConnected()) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        QString rigName = m_flrigClient->getRigName();
        if (!rigName.isEmpty()) {
            m_rigNameLabel->setText(rigName);
        }
    }
    updateConnectionStatus();
    
    connect(m_flrigClient, &FlrigClient::connected, this, &RigControlDialog::onRigConnected);
    connect(m_flrigClient, &FlrigClient::disconnected, this, &RigControlDialog::onRigDisconnected);
    connect(m_flrigClient, &FlrigClient::error, this, &RigControlDialog::onRigError);
}

RigControlDialog::~RigControlDialog()
{
}

void RigControlDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Connection settings group
    QGroupBox *connGroup = new QGroupBox("flrig Server Settings", this);
    QFormLayout *formLayout = new QFormLayout(connGroup);
    
    m_hostEdit = new QLineEdit();
    m_hostEdit->setPlaceholderText("localhost or 127.0.0.1");
    formLayout->addRow("Host:", m_hostEdit);
    
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(12345);
    formLayout->addRow("Port:", m_portSpin);
    
    m_pollIntervalSpin = new QSpinBox();
    m_pollIntervalSpin->setRange(100, 5000);
    m_pollIntervalSpin->setSingleStep(100);
    m_pollIntervalSpin->setValue(500);
    m_pollIntervalSpin->setSuffix(" ms");
    formLayout->addRow("Poll Interval:", m_pollIntervalSpin);
    
    m_autoConnectCheck = new QCheckBox("Auto-connect on startup");
    formLayout->addRow("", m_autoConnectCheck);
    
    mainLayout->addWidget(connGroup);
    
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
    
    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);
    
    connect(m_connectButton, &QPushButton::clicked, this, &RigControlDialog::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &RigControlDialog::onDisconnectClicked);
    connect(m_testButton, &QPushButton::clicked, this, &RigControlDialog::onTestClicked);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RigControlDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void RigControlDialog::loadSettings()
{
    Settings& settings = Settings::instance();
    m_hostEdit->setText(settings.getFlrigHost());
    m_portSpin->setValue(settings.getFlrigPort());
    m_pollIntervalSpin->setValue(settings.getFlrigPollInterval());
    m_autoConnectCheck->setChecked(settings.getFlrigAutoConnect());
}

void RigControlDialog::saveSettings()
{
    Settings& settings = Settings::instance();
    settings.setFlrigHost(m_hostEdit->text());
    settings.setFlrigPort(m_portSpin->value());
    settings.setFlrigPollInterval(m_pollIntervalSpin->value());
    settings.setFlrigAutoConnect(m_autoConnectCheck->isChecked());
}

void RigControlDialog::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Invalid Host", "Please enter a host name or IP address.");
        return;
    }
    
    m_statusLabel->setText("Connecting...");
    m_statusLabel->setStyleSheet("QLabel { color: orange; font-weight: bold; }");
    
    if (m_flrigClient->connectToRig(host, port)) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        
        // Get rig name
        QString rigName = m_flrigClient->getRigName();
        if (!rigName.isEmpty()) {
            m_rigNameLabel->setText(rigName);
        }
        
        // Save connection settings immediately on successful connection
        Settings& settings = Settings::instance();
        settings.setFlrigHost(host);
        settings.setFlrigPort(port);
        settings.setFlrigAutoConnect(true); // Enable auto-connect on successful connection
        m_autoConnectCheck->setChecked(true);
        
        updateConnectionStatus();
    } else {
        m_statusLabel->setText("Connection Failed");
        m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        QMessageBox::critical(this, "Connection Failed", 
            "Failed to connect to flrig server.\n\n"
            "Make sure flrig is running and configured to accept connections.");
    }
}

void RigControlDialog::onDisconnectClicked()
{
    m_flrigClient->disconnectFromRig();
    m_statusLabel->setText("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_rigNameLabel->setText("N/A");
    
    // Disable auto-connect when user manually disconnects
    Settings& settings = Settings::instance();
    settings.setFlrigAutoConnect(false);
    m_autoConnectCheck->setChecked(false);
    
    updateConnectionStatus();
}

void RigControlDialog::onTestClicked()
{
    if (!m_flrigClient->isConnected()) {
        QMessageBox::information(this, "Not Connected", "Please connect to flrig first.");
        return;
    }
    
    double freq = m_flrigClient->getFrequency();
    QString mode = m_flrigClient->getMode();
    
    QString msg = QString("Current Settings:\n\nFrequency: %1 Hz\nMode: %2")
        .arg(QString::number(freq, 'f', 0))
        .arg(mode);
    
    QMessageBox::information(this, "flrig Test", msg);
}

void RigControlDialog::onAccepted()
{
    saveSettings();
    
    // Apply poll interval change immediately if connected
    if (m_flrigClient->isConnected()) {
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
    bool connected = m_flrigClient->isConnected();
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_testButton->setEnabled(connected);
}
