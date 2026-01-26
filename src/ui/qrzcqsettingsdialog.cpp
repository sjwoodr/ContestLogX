/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "qrzcqsettingsdialog.h"
#include "settings.h"
#include "qrzcqapi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QDebug>

QrzcqSettingsDialog::QrzcqSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_api(new QrzcqApi(this))
{
    setWindowTitle("Manage QRZCQ Lookups");
    setMinimumWidth(400);
    setupUI();
    loadSettings();
    
    // Connect API signals
    connect(m_api, &QrzcqApi::sessionObtained, this, &QrzcqSettingsDialog::onSessionObtained);
    connect(m_api, &QrzcqApi::sessionError, this, &QrzcqSettingsDialog::onSessionError);
}

QrzcqSettingsDialog::~QrzcqSettingsDialog()
{
}

void QrzcqSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Auto-lookup group
    QGroupBox* autoLookupGroup = new QGroupBox("Auto Callsign Lookup", this);
    QVBoxLayout* autoLayout = new QVBoxLayout(autoLookupGroup);
    
    m_autoLookupCheckbox = new QCheckBox("Enable automatic QRZCQ lookups when entering callsigns", this);
    autoLayout->addWidget(m_autoLookupCheckbox);
    mainLayout->addWidget(autoLookupGroup);
    
    // Credentials group
    QGroupBox* credsGroup = new QGroupBox("QRZCQ.com Credentials", this);
    QVBoxLayout* credsLayout = new QVBoxLayout(credsGroup);
    
    QLabel* userLabel = new QLabel("Username:", this);
    m_usernameEdit = new QLineEdit(this);
    QHBoxLayout* userLayout = new QHBoxLayout();
    userLayout->addWidget(userLabel);
    userLayout->addWidget(m_usernameEdit);
    credsLayout->addLayout(userLayout);
    
    QLabel* passLabel = new QLabel("Password:", this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    QHBoxLayout* passLayout = new QHBoxLayout();
    passLayout->addWidget(passLabel);
    passLayout->addWidget(m_passwordEdit);
    credsLayout->addLayout(passLayout);
    
    m_testButton = new QPushButton("Test Connection", this);
    connect(m_testButton, &QPushButton::clicked, this, &QrzcqSettingsDialog::onTestConnection);
    credsLayout->addWidget(m_testButton);
    
    mainLayout->addWidget(credsGroup);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    
    connect(m_okButton, &QPushButton::clicked, this, &QrzcqSettingsDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);
}

void QrzcqSettingsDialog::loadSettings()
{
    Settings& settings = Settings::instance();
    m_autoLookupCheckbox->setChecked(settings.getQrzcqAutoLookupEnabled());
    m_usernameEdit->setText(settings.getQrzcqUsername());
    m_passwordEdit->setText(settings.getQrzcqPassword());
}

void QrzcqSettingsDialog::onTestConnection()
{
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Missing Credentials",
                           "Please enter both username and password.");
        return;
    }
    
    m_testButton->setEnabled(false);
    m_testButton->setText("Testing...");
    
    m_api->setCredentials(username, password);
    m_api->setUserAgent("ContestLogX/1.0");
    m_api->getSession();
}

void QrzcqSettingsDialog::onSessionObtained(const QString& token)
{
    m_testButton->setEnabled(true);
    m_testButton->setText("Test Connection");
    QMessageBox::information(this, "Connection Successful",
                           "Successfully connected to QRZCQ.com and obtained session token.");
}

void QrzcqSettingsDialog::onSessionError(const QString& error)
{
    m_testButton->setEnabled(true);
    m_testButton->setText("Test Connection");
    QMessageBox::critical(this, "Connection Failed",
                        QString("Failed to connect to QRZCQ.com:\n%1").arg(error));
}

void QrzcqSettingsDialog::onAccepted()
{
    Settings& settings = Settings::instance();
    settings.setQrzcqAutoLookupEnabled(m_autoLookupCheckbox->isChecked());
    settings.setQrzcqCredentials(m_usernameEdit->text(), m_passwordEdit->text());
    settings.save();
    accept();
}
