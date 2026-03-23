/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "preferencesDialog.h"
#include "shortcutsDialog.h"
#include "settings.h"
#include "qrzcqApi.h"
#include "qrzApi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QStyle>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QInputDialog>
#include <QListWidget>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , m_stationChanged(false)
    , m_themeChanged(false)
    , m_lookupChanged(false)
    , m_qrzcqApi(new QrzcqApi(this))
    , m_qrzApi(new QrzApi(this))
    , m_fontsChanged(false)
{
    m_originalTheme = Settings::instance().getTheme();
    setupUi();

    connect(m_qrzcqApi, &QrzcqApi::sessionObtained, this, &PreferencesDialog::onQrzcqSessionObtained);
    connect(m_qrzcqApi, &QrzcqApi::sessionError, this, &PreferencesDialog::onQrzcqSessionError);
    connect(m_qrzApi, &QrzApi::sessionObtained, this, &PreferencesDialog::onQrzSessionObtained);
    connect(m_qrzApi, &QrzApi::sessionError, this, &PreferencesDialog::onQrzSessionError);
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::setupUi()
{
    setWindowTitle("Preferences");
    setMinimumSize(700, 450);

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

    // Callsign Lookup tab
    QWidget *lookupTab = new QWidget(this);
    QVBoxLayout *lookupLayout = new QVBoxLayout(lookupTab);

    // Service selector
    QGroupBox *serviceGroup = new QGroupBox("Lookup Service", this);
    QVBoxLayout *serviceLayout = new QVBoxLayout(serviceGroup);
    m_lookupNoneRadio  = new QRadioButton("None (disabled)", this);
    m_lookupQrzcqRadio = new QRadioButton("QRZCQ.com", this);
    m_lookupQrzRadio   = new QRadioButton("QRZ.com (requires Logbook Data subscription)", this);
    serviceLayout->addWidget(m_lookupNoneRadio);
    serviceLayout->addWidget(m_lookupQrzcqRadio);
    serviceLayout->addWidget(m_lookupQrzRadio);
    lookupLayout->addWidget(serviceGroup);

    // Auto-lookup checkbox (applies to both services)
    m_qrzcqAutoLookupCheckbox = new QCheckBox("Enable automatic lookup when moving off the callsign field", this);
    lookupLayout->addWidget(m_qrzcqAutoLookupCheckbox);

    // QRZCQ credentials group
    m_qrzcqCredsGroup = new QGroupBox("QRZCQ.com Credentials", this);
    QVBoxLayout *qrzcqCredsLayout = new QVBoxLayout(m_qrzcqCredsGroup);
    QHBoxLayout *qrzcqUserLayout = new QHBoxLayout();
    qrzcqUserLayout->addWidget(new QLabel("Username:", this));
    m_qrzcqUsernameEdit = new QLineEdit(this);
    qrzcqUserLayout->addWidget(m_qrzcqUsernameEdit);
    qrzcqCredsLayout->addLayout(qrzcqUserLayout);
    QHBoxLayout *qrzcqPassLayout = new QHBoxLayout();
    qrzcqPassLayout->addWidget(new QLabel("Password:", this));
    m_qrzcqPasswordEdit = new QLineEdit(this);
    m_qrzcqPasswordEdit->setEchoMode(QLineEdit::Password);
    qrzcqPassLayout->addWidget(m_qrzcqPasswordEdit);
    qrzcqCredsLayout->addLayout(qrzcqPassLayout);
    m_qrzcqTestButton = new QPushButton("Test Connection", this);
    connect(m_qrzcqTestButton, &QPushButton::clicked, this, &PreferencesDialog::onTestQrzcqConnection);
    qrzcqCredsLayout->addWidget(m_qrzcqTestButton);
    lookupLayout->addWidget(m_qrzcqCredsGroup);

    // QRZ credentials group
    m_qrzCredsGroup = new QGroupBox("QRZ.com Credentials", this);
    QVBoxLayout *qrzCredsLayout = new QVBoxLayout(m_qrzCredsGroup);
    QHBoxLayout *qrzUserLayout = new QHBoxLayout();
    qrzUserLayout->addWidget(new QLabel("Username:", this));
    m_qrzUsernameEdit = new QLineEdit(this);
    qrzUserLayout->addWidget(m_qrzUsernameEdit);
    qrzCredsLayout->addLayout(qrzUserLayout);
    QHBoxLayout *qrzPassLayout = new QHBoxLayout();
    qrzPassLayout->addWidget(new QLabel("Password:", this));
    m_qrzPasswordEdit = new QLineEdit(this);
    m_qrzPasswordEdit->setEchoMode(QLineEdit::Password);
    qrzPassLayout->addWidget(m_qrzPasswordEdit);
    qrzCredsLayout->addLayout(qrzPassLayout);
    m_qrzTestButton = new QPushButton("Test Connection", this);
    connect(m_qrzTestButton, &QPushButton::clicked, this, &PreferencesDialog::onTestQrzConnection);
    qrzCredsLayout->addWidget(m_qrzTestButton);
    lookupLayout->addWidget(m_qrzCredsGroup);

    lookupLayout->addStretch();

    // Load current settings and set initial state
    {
        QString svc = settings.getCallsignLookupService();
        if (svc == "qrz")       m_lookupQrzRadio->setChecked(true);
        else if (svc == "qrzcq") m_lookupQrzcqRadio->setChecked(true);
        else                     m_lookupNoneRadio->setChecked(true);
    }
    m_qrzcqAutoLookupCheckbox->setChecked(settings.getQrzcqAutoLookupEnabled());
    m_qrzcqUsernameEdit->setText(settings.getQrzcqUsername());
    m_qrzcqPasswordEdit->setText(settings.getQrzcqPassword());
    m_qrzUsernameEdit->setText(settings.getQrzUsername());
    m_qrzPasswordEdit->setText(settings.getQrzPassword());

    connect(m_lookupNoneRadio,  &QRadioButton::toggled, this, &PreferencesDialog::onLookupServiceChanged);
    connect(m_lookupQrzcqRadio, &QRadioButton::toggled, this, &PreferencesDialog::onLookupServiceChanged);
    connect(m_lookupQrzRadio,   &QRadioButton::toggled, this, &PreferencesDialog::onLookupServiceChanged);
    onLookupServiceChanged();  // set initial visibility

    tabWidget->addTab(lookupTab, "Callsign Lookup");

    // DX Cluster tab
    QWidget *dxClusterTab = new QWidget(this);
    QVBoxLayout *dxClusterLayout = new QVBoxLayout(dxClusterTab);

    QLabel *dxClusterLabel = new QLabel("DX Cluster servers (host:port):", this);
    dxClusterLayout->addWidget(dxClusterLabel);

    m_dxClusterList = new QListWidget(this);
    for (const QString& srv : settings.getDxClusterServers())
        m_dxClusterList->addItem(srv);
    dxClusterLayout->addWidget(m_dxClusterList);

    QHBoxLayout *dxClusterButtonLayout = new QHBoxLayout();
    m_dxClusterAddButton = new QPushButton("Add", this);
    m_dxClusterEditButton = new QPushButton("Edit", this);
    m_dxClusterDeleteButton = new QPushButton("Delete", this);
    dxClusterButtonLayout->addWidget(m_dxClusterAddButton);
    dxClusterButtonLayout->addWidget(m_dxClusterEditButton);
    dxClusterButtonLayout->addWidget(m_dxClusterDeleteButton);
    dxClusterButtonLayout->addStretch();
    dxClusterLayout->addLayout(dxClusterButtonLayout);

    connect(m_dxClusterAddButton,    &QPushButton::clicked, this, &PreferencesDialog::onAddDxCluster);
    connect(m_dxClusterEditButton,   &QPushButton::clicked, this, &PreferencesDialog::onEditDxCluster);
    connect(m_dxClusterDeleteButton, &QPushButton::clicked, this, &PreferencesDialog::onDeleteDxCluster);

    tabWidget->addTab(dxClusterTab, "DX Cluster");

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

    // ── Online Scoring tab ──────────────────────────────────────────────────
    QWidget *osTab = new QWidget;
    QFormLayout *osLayout = new QFormLayout(osTab);

    m_osCallsignEdit = new QLineEdit;
    m_osCallsignEdit->setPlaceholderText("Callsign for contestonlinescore.com");
    m_osCallsignEdit->setText(Settings::instance().getOnlineScoringCallsign());
    osLayout->addRow("Callsign:", m_osCallsignEdit);

    m_osPasswordEdit = new QLineEdit;
    m_osPasswordEdit->setEchoMode(QLineEdit::Password);
    m_osPasswordEdit->setPlaceholderText("Password");
    m_osPasswordEdit->setText(Settings::instance().getOnlineScoringPassword());
    osLayout->addRow("Password:", m_osPasswordEdit);

    m_osIntervalCombo = new QComboBox;
    m_osIntervalCombo->addItem("1 minute", 1);
    m_osIntervalCombo->addItem("2 minutes", 2);
    m_osIntervalCombo->addItem("5 minutes", 5);
    m_osIntervalCombo->addItem("10 minutes", 10);
    m_osIntervalCombo->addItem("15 minutes", 15);
    int savedInterval = Settings::instance().getOnlineScoringInterval();
    int osIdx = m_osIntervalCombo->findData(savedInterval);
    if (osIdx >= 0) m_osIntervalCombo->setCurrentIndex(osIdx);
    osLayout->addRow("Post interval:", m_osIntervalCombo);

    m_osPerQsoCheck = new QCheckBox("Post after each QSO (instead of on timer)");
    m_osPerQsoCheck->setChecked(Settings::instance().getOnlineScoringPerQso());
    osLayout->addRow("", m_osPerQsoCheck);

    QLabel *osNote = new QLabel(
        "Enable posting from the Contest menu during an active contest.\n"
        "Scores are posted to contestonlinescore.com.");
    osNote->setWordWrap(true);
    osNote->setStyleSheet("color: gray;");
    osLayout->addRow(osNote);

    tabWidget->addTab(osTab, "Online Scoring");

    mainLayout->addWidget(tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *btn = buttonBox->button(QDialogButtonBox::Ok))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    if (auto *btn = buttonBox->button(QDialogButtonBox::Cancel))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
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

    // DX Cluster servers
    {
        QStringList servers;
        for (int i = 0; i < m_dxClusterList->count(); ++i)
            servers.append(m_dxClusterList->item(i)->text());
        settings.setDxClusterServers(servers);
    }

    // Callsign Lookup
    QString newService = m_lookupQrzRadio->isChecked()   ? "qrz"   :
                         m_lookupQrzcqRadio->isChecked() ? "qrzcq" : "none";
    bool svcChanged  = (newService != settings.getCallsignLookupService());
    bool autoChanged = (m_qrzcqAutoLookupCheckbox->isChecked() != settings.getQrzcqAutoLookupEnabled());
    bool qrzcqUserChanged = (m_qrzcqUsernameEdit->text() != settings.getQrzcqUsername());
    bool qrzcqPassChanged = (m_qrzcqPasswordEdit->text() != settings.getQrzcqPassword());
    bool qrzUserChanged   = (m_qrzUsernameEdit->text()   != settings.getQrzUsername());
    bool qrzPassChanged   = (m_qrzPasswordEdit->text()   != settings.getQrzPassword());

    if (svcChanged || autoChanged || qrzcqUserChanged || qrzcqPassChanged ||
        qrzUserChanged || qrzPassChanged) {
        settings.setCallsignLookupService(newService);
        settings.setQrzcqAutoLookupEnabled(m_qrzcqAutoLookupCheckbox->isChecked());
        settings.setQrzcqCredentials(m_qrzcqUsernameEdit->text(), m_qrzcqPasswordEdit->text());
        settings.setQrzCredentials(m_qrzUsernameEdit->text(), m_qrzPasswordEdit->text());
        settings.save();
        m_lookupChanged = true;
    }

    // Online scoring
    settings.setOnlineScoringCredentials(m_osCallsignEdit->text().trimmed().toUpper(),
                                         m_osPasswordEdit->text());
    settings.setOnlineScoringInterval(m_osIntervalCombo->currentData().toInt());
    settings.setOnlineScoringPerQso(m_osPerQsoCheck->isChecked());
    settings.save();

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

void PreferencesDialog::onLookupServiceChanged()
{
    bool qrzcqSelected = m_lookupQrzcqRadio->isChecked();
    bool qrzSelected   = m_lookupQrzRadio->isChecked();
    m_qrzcqCredsGroup->setVisible(qrzcqSelected);
    m_qrzCredsGroup->setVisible(qrzSelected);
    m_qrzcqAutoLookupCheckbox->setEnabled(qrzcqSelected || qrzSelected);
}

void PreferencesDialog::onTestQrzConnection()
{
    QString username = m_qrzUsernameEdit->text().trimmed();
    QString password = m_qrzPasswordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Missing Credentials",
                             "Please enter both username and password.");
        return;
    }

    m_qrzTestButton->setEnabled(false);
    m_qrzTestButton->setText("Testing...");

    m_qrzApi->setCredentials(username, password);
    m_qrzApi->setUserAgent("ContestLogX/1.0");
    m_qrzApi->getSession();
}

void PreferencesDialog::onQrzSessionObtained(const QString& token)
{
    Q_UNUSED(token);
    m_qrzTestButton->setEnabled(true);
    m_qrzTestButton->setText("Test Connection");
    QMessageBox::information(this, "Connection Successful",
                             "Successfully connected to QRZ.com and obtained session token.");
}

void PreferencesDialog::onQrzSessionError(const QString& error)
{
    m_qrzTestButton->setEnabled(true);
    m_qrzTestButton->setText("Test Connection");
    QMessageBox::critical(this, "Connection Failed",
                          QString("Failed to connect to QRZ.com:\n%1").arg(error));
}

void PreferencesDialog::onAddDxCluster()
{
    bool ok;
    QString server = QInputDialog::getText(this, "Add DX Cluster",
                                           "Server (host:port):", QLineEdit::Normal,
                                           QString(), &ok);
    server = server.trimmed();
    if (!ok || server.isEmpty())
        return;
    m_dxClusterList->addItem(server);
}

void PreferencesDialog::onEditDxCluster()
{
    QListWidgetItem *item = m_dxClusterList->currentItem();
    if (!item)
        return;
    bool ok;
    QString server = QInputDialog::getText(this, "Edit DX Cluster",
                                           "Server (host:port):", QLineEdit::Normal,
                                           item->text(), &ok);
    server = server.trimmed();
    if (!ok || server.isEmpty())
        return;
    item->setText(server);
}

void PreferencesDialog::onDeleteDxCluster()
{
    QListWidgetItem *item = m_dxClusterList->currentItem();
    if (!item)
        return;
    delete m_dxClusterList->takeItem(m_dxClusterList->row(item));
}
