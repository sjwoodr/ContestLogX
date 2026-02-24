/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "preferencesDialog.h"
#include "shortcutsDialog.h"
#include "settings.h"
#include "qrzcqApi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , m_stationChanged(false)
    , m_themeChanged(false)
    , m_qrzcqChanged(false)
    , m_qrzcqApi(new QrzcqApi(this))
    , m_fontsChanged(false)
{
    m_originalTheme = Settings::instance().getTheme();
    setupUi();

    connect(m_qrzcqApi, &QrzcqApi::sessionObtained, this, &PreferencesDialog::onQrzcqSessionObtained);
    connect(m_qrzcqApi, &QrzcqApi::sessionError, this, &PreferencesDialog::onQrzcqSessionError);
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::setupUi()
{
    setWindowTitle("Preferences");
    setMinimumSize(550, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QTabWidget *tabWidget = new QTabWidget(this);

    // Station tab
    Settings &settings = Settings::instance();

    QWidget *stationTab = new QWidget(this);
    QFormLayout *stationLayout = new QFormLayout(stationTab);

    m_callsignEdit = new QLineEdit(settings.getCallsign(), this);
    m_callsignEdit->setMaxLength(20);
    connect(m_callsignEdit, &QLineEdit::textChanged, this, &PreferencesDialog::onCallsignTextChanged);
    stationLayout->addRow("Callsign:", m_callsignEdit);

    m_nameEdit = new QLineEdit(settings.getOperatorName(), this);
    stationLayout->addRow("Operator Name:", m_nameEdit);

    m_gridEdit = new QLineEdit(settings.getGridSquare(), this);
    m_gridEdit->setMaxLength(10);
    connect(m_gridEdit, &QLineEdit::textChanged, this, &PreferencesDialog::onGridTextChanged);
    stationLayout->addRow("Grid Square:", m_gridEdit);

    m_stateEdit = new QLineEdit(settings.getState(), this);
    connect(m_stateEdit, &QLineEdit::textChanged, this, &PreferencesDialog::onStateTextChanged);
    stationLayout->addRow("State/Province:", m_stateEdit);

    tabWidget->addTab(stationTab, "Station");

    // Display tab
    QWidget *displayTab = new QWidget(this);
    QFormLayout *displayLayout = new QFormLayout(displayTab);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("Dark", "dark");
    m_themeCombo->addItem("Light", "light");

    int idx = m_themeCombo->findData(m_originalTheme);
    if (idx >= 0)
        m_themeCombo->setCurrentIndex(idx);

    displayLayout->addRow("Theme:", m_themeCombo);

    tabWidget->addTab(displayTab, "Display");

    // Shortcuts tab
    m_shortcutsWidget = new ShortcutsWidget(this);
    tabWidget->addTab(m_shortcutsWidget, "Shortcuts");

    // QRZCQ tab
    QWidget *qrzcqTab = new QWidget(this);
    QVBoxLayout *qrzcqLayout = new QVBoxLayout(qrzcqTab);

    QGroupBox *autoLookupGroup = new QGroupBox("Auto Callsign Lookup", this);
    QVBoxLayout *autoLayout = new QVBoxLayout(autoLookupGroup);
    m_qrzcqAutoLookupCheckbox = new QCheckBox("Enable automatic QRZCQ lookups when entering callsigns", this);
    autoLayout->addWidget(m_qrzcqAutoLookupCheckbox);
    qrzcqLayout->addWidget(autoLookupGroup);

    QGroupBox *credsGroup = new QGroupBox("QRZCQ.com Credentials", this);
    QVBoxLayout *credsLayout = new QVBoxLayout(credsGroup);

    QLabel *userLabel = new QLabel("Username:", this);
    m_qrzcqUsernameEdit = new QLineEdit(this);
    QHBoxLayout *userLayout = new QHBoxLayout();
    userLayout->addWidget(userLabel);
    userLayout->addWidget(m_qrzcqUsernameEdit);
    credsLayout->addLayout(userLayout);

    QLabel *passLabel = new QLabel("Password:", this);
    m_qrzcqPasswordEdit = new QLineEdit(this);
    m_qrzcqPasswordEdit->setEchoMode(QLineEdit::Password);
    QHBoxLayout *passLayout = new QHBoxLayout();
    passLayout->addWidget(passLabel);
    passLayout->addWidget(m_qrzcqPasswordEdit);
    credsLayout->addLayout(passLayout);

    m_qrzcqTestButton = new QPushButton("Test Connection", this);
    connect(m_qrzcqTestButton, &QPushButton::clicked, this, &PreferencesDialog::onTestQrzcqConnection);
    credsLayout->addWidget(m_qrzcqTestButton);

    qrzcqLayout->addWidget(credsGroup);
    qrzcqLayout->addStretch();

    // Load current QRZCQ settings
    m_qrzcqAutoLookupCheckbox->setChecked(settings.getQrzcqAutoLookupEnabled());
    m_qrzcqUsernameEdit->setText(settings.getQrzcqUsername());
    m_qrzcqPasswordEdit->setText(settings.getQrzcqPassword());

    tabWidget->addTab(qrzcqTab, "QRZCQ");

    // Fonts tab
    QWidget *fontsTab = new QWidget(this);
    QVBoxLayout *fontsVLayout = new QVBoxLayout(fontsTab);

    QFormLayout *fontsLayout = new QFormLayout();
    fontsLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    struct PanelDef { QString key; QString label; };
    const QList<PanelDef> panels = {
        { "qsoEntry",    "QSO Entry" },
        { "qsoLog",      "QSO Log" },
        { "dxCluster",   "DX Cluster" },
        { "scp",         "Super Check Partial" },
        { "cwKeyboard",  "CW Keyboard" },
        { "scoreWidget", "Score Widget" },
        { "cwMemories",  "CW Memories" },
        { "ssbMemories", "SSB Memories" },
    };

    for (const PanelDef& p : panels) {
        QFont saved = settings.getPanelFont(p.key);
        QFont current = saved.family().isEmpty() ? QApplication::font() : saved;

        QFontComboBox *familyCombo = new QFontComboBox(this);
        familyCombo->setCurrentFont(current);
        familyCombo->setMinimumWidth(200);

        QSpinBox *sizeSpinBox = new QSpinBox(this);
        sizeSpinBox->setRange(6, 48);
        sizeSpinBox->setValue(current.pointSize() > 0 ? current.pointSize() : QApplication::font().pointSize());
        sizeSpinBox->setSuffix(" pt");
        sizeSpinBox->setFixedWidth(70);

        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(familyCombo, 1);
        row->addWidget(sizeSpinBox);

        fontsLayout->addRow(p.label + ":", row);
        m_fontRows.append({ p.key, familyCombo, sizeSpinBox });
    }

    fontsVLayout->addLayout(fontsLayout);
    fontsVLayout->addStretch();

    QHBoxLayout *fontsButtonLayout = new QHBoxLayout();
    fontsButtonLayout->addStretch();
    QPushButton *resetFontsButton = new QPushButton("Reset to Defaults", this);
    connect(resetFontsButton, &QPushButton::clicked, this, [this]() {
        QFont appFont = QApplication::font();
        for (const FontRow& row : m_fontRows) {
            row.familyCombo->setCurrentFont(appFont);
            row.sizeSpinBox->setValue(appFont.pointSize() > 0 ? appFont.pointSize() : 10);
        }
    });
    fontsButtonLayout->addWidget(resetFontsButton);
    fontsVLayout->addLayout(fontsButtonLayout);

    tabWidget->addTab(fontsTab, "Fonts");

    mainLayout->addWidget(tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void PreferencesDialog::onAccept()
{
    Settings &settings = Settings::instance();

    // Station
    QString call = m_callsignEdit->text().trimmed().toUpper();
    QString name = m_nameEdit->text().trimmed();
    QString grid = m_gridEdit->text().trimmed().toUpper();
    QString state = m_stateEdit->text().trimmed().toUpper();

    if (call != settings.getCallsign() || name != settings.getOperatorName() ||
        grid != settings.getGridSquare() || state != settings.getState()) {
        settings.setCallsign(call);
        settings.setOperatorName(name);
        settings.setGridSquare(grid);
        settings.setState(state);
        settings.save();
        m_stationChanged = true;
    }

    // Display
    QString selected = m_themeCombo->currentData().toString();
    if (selected != m_originalTheme) {
        settings.setTheme(selected);
        m_themeChanged = true;
    }

    // Shortcuts
    m_shortcutsWidget->saveShortcuts();

    // Fonts
    for (const FontRow& row : m_fontRows) {
        QFont font = row.familyCombo->currentFont();
        font.setPointSize(row.sizeSpinBox->value());
        QFont saved = settings.getPanelFont(row.panelKey);
        if (font.family() != saved.family() || font.pointSize() != saved.pointSize()) {
            settings.setPanelFont(row.panelKey, font);
            m_fontsChanged = true;
        }
    }

    // QRZCQ
    bool autoChanged = m_qrzcqAutoLookupCheckbox->isChecked() != settings.getQrzcqAutoLookupEnabled();
    bool userChanged = m_qrzcqUsernameEdit->text() != settings.getQrzcqUsername();
    bool passChanged = m_qrzcqPasswordEdit->text() != settings.getQrzcqPassword();

    if (autoChanged || userChanged || passChanged) {
        settings.setQrzcqAutoLookupEnabled(m_qrzcqAutoLookupCheckbox->isChecked());
        settings.setQrzcqCredentials(m_qrzcqUsernameEdit->text(), m_qrzcqPasswordEdit->text());
        settings.save();
        m_qrzcqChanged = true;
    }

    accept();
}

void PreferencesDialog::onTestQrzcqConnection()
{
    QString username = m_qrzcqUsernameEdit->text().trimmed();
    QString password = m_qrzcqPasswordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Missing Credentials",
                           "Please enter both username and password.");
        return;
    }

    m_qrzcqTestButton->setEnabled(false);
    m_qrzcqTestButton->setText("Testing...");

    m_qrzcqApi->setCredentials(username, password);
    m_qrzcqApi->setUserAgent("ContestLogX/1.0");
    m_qrzcqApi->getSession();
}

void PreferencesDialog::onQrzcqSessionObtained(const QString& token)
{
    Q_UNUSED(token);
    m_qrzcqTestButton->setEnabled(true);
    m_qrzcqTestButton->setText("Test Connection");
    QMessageBox::information(this, "Connection Successful",
                           "Successfully connected to QRZCQ.com and obtained session token.");
}

void PreferencesDialog::onQrzcqSessionError(const QString& error)
{
    m_qrzcqTestButton->setEnabled(true);
    m_qrzcqTestButton->setText("Test Connection");
    QMessageBox::critical(this, "Connection Failed",
                        QString("Failed to connect to QRZCQ.com:\n%1").arg(error));
}

void PreferencesDialog::onCallsignTextChanged(const QString& text)
{
    QString upper = text.toUpper();
    if (text != upper) {
        int pos = m_callsignEdit->cursorPosition();
        m_callsignEdit->setText(upper);
        m_callsignEdit->setCursorPosition(pos);
    }
}

void PreferencesDialog::onGridTextChanged(const QString& text)
{
    QString upper = text.toUpper();
    if (text != upper) {
        int pos = m_gridEdit->cursorPosition();
        m_gridEdit->setText(upper);
        m_gridEdit->setCursorPosition(pos);
    }
}

void PreferencesDialog::onStateTextChanged(const QString& text)
{
    QString upper = text.toUpper();
    if (text != upper) {
        int pos = m_stateEdit->cursorPosition();
        m_stateEdit->setText(upper);
        m_stateEdit->setCursorPosition(pos);
    }
}
