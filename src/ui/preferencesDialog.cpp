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
#include "onlineScoreClient.h"
#include "dxccDatabase.h"
#include <QStandardPaths>
#include <QFile>
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
#include <QClipboard>
#include <QGuiApplication>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUuid>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , m_stationChanged(false)
    , m_themeChanged(false)
    , m_lookupChanged(false)
    , m_qrzcqApi(new QrzcqApi(this))
    , m_qrzApi(new QrzApi(this))
    , m_fontsChanged(false)
    , m_osTestClient(new OnlineScoreClient(this))
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

    m_cqZoneSpinBox = new QSpinBox(this);
    m_cqZoneSpinBox->setRange(0, 40);
    m_cqZoneSpinBox->setSpecialValueText("—");
    m_cqZoneSpinBox->setValue(settings.getCqZone());
    stationLayout->addRow("CQ Zone:", m_cqZoneSpinBox);

    m_ituZoneSpinBox = new QSpinBox(this);
    m_ituZoneSpinBox->setRange(0, 90);
    m_ituZoneSpinBox->setSpecialValueText("—");
    m_ituZoneSpinBox->setValue(settings.getItuZone());
    stationLayout->addRow("ITU Zone:", m_ituZoneSpinBox);

    m_arrlSectionEdit = new QLineEdit(settings.getArrlSection(), this);
    m_arrlSectionEdit->setMaxLength(10);
    stationLayout->addRow("ARRL Section:", m_arrlSectionEdit);

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

    m_forceX11Check = new QCheckBox("Use X11 backend (fixes window position on Wayland, requires restart)", this);
    m_forceX11Check->setChecked(Settings::instance().getForceX11());
#ifdef Q_OS_LINUX
    displayLayout->addRow("", m_forceX11Check);
#else
    m_forceX11Check->setVisible(false);
#endif

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
        { "cwDecoder",   "CW Decoder" },
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

    m_osEnabledCheck = new QCheckBox("Enable online score publishing to contestonlinescore.com");
    m_osEnabledCheck->setChecked(Settings::instance().getOnlineScoringEnabled());
    osLayout->addRow(m_osEnabledCheck);

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

    m_osTestButton = new QPushButton("Post Now (Test)");
    m_osTestButton->setToolTip("Send a test post to contestonlinescore.com to verify credentials");
    m_osTestStatusLabel = new QLabel("");
    QHBoxLayout *osTestLayout = new QHBoxLayout;
    osTestLayout->addWidget(m_osTestButton);
    osTestLayout->addWidget(m_osTestStatusLabel);
    osTestLayout->addStretch();
    osLayout->addRow("", osTestLayout);

    connect(m_osTestButton, &QPushButton::clicked, this, &PreferencesDialog::onTestOnlineScoring);

    QLabel *osNote = new QLabel(
        "Scores are posted to contestonlinescore.com.\n"
        "Use the Contest menu to start/stop posting during an active contest.");
    osNote->setWordWrap(true);
    osNote->setStyleSheet("color: gray;");
    osLayout->addRow(osNote);

    // Enable/disable fields based on checkbox state
    auto updateOsFields = [this](bool enabled) {
        m_osCallsignEdit->setEnabled(enabled);
        m_osPasswordEdit->setEnabled(enabled);
        m_osIntervalCombo->setEnabled(enabled);
        m_osPerQsoCheck->setEnabled(enabled);
        m_osTestButton->setEnabled(enabled);
    };
    connect(m_osEnabledCheck, &QCheckBox::toggled, updateOsFields);
    updateOsFields(m_osEnabledCheck->isChecked());

    tabWidget->addTab(osTab, "Online Scoring");

    // ── Remote Control tab ──────────────────────────────────────────────────
    QWidget *rcTab = new QWidget;
    QFormLayout *rcLayout = new QFormLayout(rcTab);

    m_rcEnabledCheck = new QCheckBox("Enable Remote Control HTTP server");
    m_rcEnabledCheck->setChecked(Settings::instance().getRemoteControlEnabled());
    m_rcEnabledCheck->setToolTip(
        "Exposes a small HTTP server on your LAN so your phone or tablet can "
        "show a read-only dashboard of your current session (score, rate, "
        "recent QSOs, rig state, propagation).");
    rcLayout->addRow(m_rcEnabledCheck);

    m_rcPortSpin = new QSpinBox;
    m_rcPortSpin->setRange(1, 65535);
    m_rcPortSpin->setValue(Settings::instance().getRemoteControlPort());
    rcLayout->addRow("Port:", m_rcPortSpin);

    m_rcBindModeCombo = new QComboBox;
    m_rcBindModeCombo->addItem("LAN (auto-detect)", "lan");
    m_rcBindModeCombo->addItem("Localhost only (127.0.0.1)", "localhost");
    m_rcBindModeCombo->addItem("Any interface (0.0.0.0)", "any");
    {
        int idx = m_rcBindModeCombo->findData(
            Settings::instance().getRemoteControlBindMode());
        m_rcBindModeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    rcLayout->addRow("Bind to:", m_rcBindModeCombo);

    // Token row — read-only display + Rotate button. Auto-generated on
    // first enable; rotating invalidates anything that had the old token
    // saved in a bookmark.
    m_rcTokenEdit = new QLineEdit;
    m_rcTokenEdit->setReadOnly(true);
    m_rcTokenEdit->setText(Settings::instance().getRemoteControlToken());
    m_rcRotateTokenButton = new QPushButton("Rotate");
    m_rcRotateTokenButton->setToolTip(
        "Generate a new token. Any bookmarked URLs using the old token will "
        "stop working.");
    QHBoxLayout *rcTokenRow = new QHBoxLayout;
    rcTokenRow->addWidget(m_rcTokenEdit, 1);
    rcTokenRow->addWidget(m_rcRotateTokenButton);
    rcLayout->addRow("Auth token:", rcTokenRow);

    // URL-for-phone helper — shows a bookmarkable URL and copies it to
    // the clipboard on click.
    m_rcUrlLabel = new QLabel;
    m_rcUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_rcUrlLabel->setWordWrap(true);
    m_rcCopyUrlButton = new QPushButton("Copy URL for Phone");
    QHBoxLayout *rcUrlRow = new QHBoxLayout;
    rcUrlRow->addWidget(m_rcUrlLabel, 1);
    rcUrlRow->addWidget(m_rcCopyUrlButton);
    rcLayout->addRow("Phone URL:", rcUrlRow);

    QLabel *rcNote = new QLabel(
        "Changes take effect after clicking OK; the server stops and restarts "
        "with the new settings. Keep the token private — anyone on your LAN "
        "with the URL and token can view your session state.");
    rcNote->setWordWrap(true);
    rcNote->setStyleSheet("color: gray;");
    rcLayout->addRow(rcNote);

    auto updateRcFields = [this](bool enabled) {
        m_rcPortSpin->setEnabled(enabled);
        m_rcBindModeCombo->setEnabled(enabled);
        m_rcRotateTokenButton->setEnabled(enabled);
        m_rcCopyUrlButton->setEnabled(enabled);
    };
    connect(m_rcEnabledCheck, &QCheckBox::toggled, this,
            [this, updateRcFields](bool on) {
        updateRcFields(on);
        // Generate a token on first enable so the URL is immediately usable.
        if (on && m_rcTokenEdit->text().isEmpty()) {
            m_rcTokenEdit->setText(
                QUuid::createUuid().toString(QUuid::Id128));
        }
        updateRemoteControlUrlLabel();
    });
    connect(m_rcPortSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int){ updateRemoteControlUrlLabel(); });
    connect(m_rcBindModeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int){ updateRemoteControlUrlLabel(); });
    connect(m_rcRotateTokenButton, &QPushButton::clicked, this, [this]() {
        m_rcTokenEdit->setText(QUuid::createUuid().toString(QUuid::Id128));
        updateRemoteControlUrlLabel();
    });
    connect(m_rcCopyUrlButton, &QPushButton::clicked, this, [this]() {
        const QString url = m_rcUrlLabel->text();
        if (!url.isEmpty() && !url.startsWith(QLatin1String("—"))) {
            QGuiApplication::clipboard()->setText(url);
        }
    });

    updateRcFields(m_rcEnabledCheck->isChecked());
    updateRemoteControlUrlLabel();

    tabWidget->addTab(rcTab, "Remote Control");

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

    int cqZone = m_cqZoneSpinBox->value();
    int ituZone = m_ituZoneSpinBox->value();
    QString arrlSection = m_arrlSectionEdit->text().trimmed().toUpper();

    if (call != settings.getCallsign() || name != settings.getOperatorName() ||
        grid != settings.getGridSquare() || state != settings.getState() ||
        cqZone != settings.getCqZone() || ituZone != settings.getItuZone() ||
        arrlSection != settings.getArrlSection()) {
        settings.setCallsign(call);
        settings.setOperatorName(name);
        settings.setGridSquare(grid);
        settings.setState(state);
        settings.setCqZone(cqZone);
        settings.setItuZone(ituZone);
        settings.setArrlSection(arrlSection);
        settings.save();
        m_stationChanged = true;
    }

    // Display
    QString selected = m_themeCombo->currentData().toString();
    if (selected != m_originalTheme) {
        settings.setTheme(selected);
        m_themeChanged = true;
    }

    settings.setForceX11(m_forceX11Check->isChecked());

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

    // Online scoring — validate required fields if enabling
    if (m_osEnabledCheck->isChecked()) {
        QStringList missing;
        if (m_osCallsignEdit->text().trimmed().isEmpty()) missing << "Online Scoring Callsign";
        if (m_osPasswordEdit->text().isEmpty()) missing << "Online Scoring Password";
        if (m_cqZoneSpinBox->value() <= 0) missing << "CQ Zone (Station tab)";
        if (m_ituZoneSpinBox->value() <= 0) missing << "ITU Zone (Station tab)";
        if (m_stateEdit->text().trimmed().isEmpty()) missing << "State/Province (Station tab)";
        if (m_gridEdit->text().trimmed().isEmpty()) missing << "Grid Square (Station tab)";

        if (!missing.isEmpty()) {
            QMessageBox::warning(this, "Online Scoring",
                "Online scoring requires the following fields:\n\n- " +
                missing.join("\n- ") +
                "\n\nPlease fill them in before enabling.");
            m_osEnabledCheck->setChecked(false);
        }
    }

    settings.setOnlineScoringEnabled(m_osEnabledCheck->isChecked());
    settings.setOnlineScoringCredentials(m_osCallsignEdit->text().trimmed().toUpper(),
                                         m_osPasswordEdit->text());
    settings.setOnlineScoringInterval(m_osIntervalCombo->currentData().toInt());
    settings.setOnlineScoringPerQso(m_osPerQsoCheck->isChecked());

    // Remote Control — save config and emit a signal that MainWindow
    // can hook to restart the HTTP server with new settings. The server's
    // listener port / bind mode might have changed; easiest is to stop
    // and start it.
    settings.setRemoteControlPort(m_rcPortSpin->value());
    settings.setRemoteControlBindMode(
        m_rcBindModeCombo->currentData().toString());
    settings.setRemoteControlToken(m_rcTokenEdit->text());
    settings.setRemoteControlEnabled(m_rcEnabledCheck->isChecked());

    settings.save();

    accept();
}

void PreferencesDialog::updateRemoteControlUrlLabel()
{
    if (!m_rcUrlLabel || !m_rcPortSpin || !m_rcBindModeCombo || !m_rcTokenEdit)
        return;

    const QString bindMode = m_rcBindModeCombo->currentData().toString();
    const int port = m_rcPortSpin->value();
    const QString token = m_rcTokenEdit->text();

    // Pick a sensible address to surface in the URL. "lan" auto-detects
    // the first non-loopback IPv4 — what a phone on the same Wi-Fi
    // will reach. "localhost" is for testing on the same machine. "any"
    // shows both so the operator can tell their phone where to point.
    QString hostPart;
    if (bindMode == QLatin1String("localhost")) {
        hostPart = QStringLiteral("127.0.0.1");
    } else {
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                const QHostAddress addr = entry.ip();
                if (addr.protocol() == QAbstractSocket::IPv4Protocol
                    && !addr.isLoopback()) {
                    hostPart = addr.toString();
                    break;
                }
            }
            if (!hostPart.isEmpty()) break;
        }
        if (hostPart.isEmpty()) hostPart = QStringLiteral("<your-LAN-IP>");
    }

    if (!m_rcEnabledCheck->isChecked()) {
        m_rcUrlLabel->setText(
            QStringLiteral("— (enable the server above to see the URL)"));
        return;
    }
    if (token.isEmpty()) {
        m_rcUrlLabel->setText(
            QStringLiteral("— (no token; check the checkbox to generate one)"));
        return;
    }
    const QString url = QStringLiteral("http://%1:%2/?token=%3")
                            .arg(hostPart).arg(port).arg(token);
    m_rcUrlLabel->setText(url);
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

void PreferencesDialog::onTestOnlineScoring()
{
    QString callsign = m_osCallsignEdit->text().trimmed().toUpper();
    QString password = m_osPasswordEdit->text();
    if (callsign.isEmpty() || password.isEmpty()) {
        m_osTestStatusLabel->setText("Enter callsign and password first");
        m_osTestStatusLabel->setStyleSheet("color: red;");
        return;
    }

    m_osTestButton->setEnabled(false);
    m_osTestStatusLabel->setText("Posting...");
    m_osTestStatusLabel->setStyleSheet("");

    m_osTestClient->setCredentials(callsign, password);

    // Build a minimal test post using "CW-Ops" as the contest ID
    // (always active on the server; zero scores are cleaned up before contests)
    ScorePostData data;
    data.contestId = "CW-Ops";
    data.callsign = callsign;
    data.ops = callsign;
    // Look up DXCC country from callsign using a temporary DxccDatabase
    {
        DxccDatabase db;
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                           + "/ContestLogX/cty.dat";
        if (QFile::exists(dataPath)) {
            db.loadFromFile(dataPath);
            auto entity = db.lookupCallsign(callsign);
            if (entity.dxcc > 0)
                data.dxccCountry = entity.primaryPrefix;
        }
    }
    data.stPrvOth = m_stateEdit->text().trimmed().toUpper();
    data.grid = m_gridEdit->text().trimmed().toUpper();
    data.cqZone = m_cqZoneSpinBox->value();
    data.ituZone = m_ituZoneSpinBox->value();
    data.arrlSection = m_arrlSectionEdit->text().trimmed().toUpper();
    data.totalScore = 0;

    // Totals-only breakdown
    ScoreBreakdownEntry totals;
    totals.band = "total";
    totals.mode = "ALL";
    totals.qsoCount = 0;
    totals.points = 0;
    data.breakdown.append(totals);

    connect(m_osTestClient, &OnlineScoreClient::postSuccess, this, [this](const QString&) {
        m_osTestStatusLabel->setText("Success!");
        m_osTestStatusLabel->setStyleSheet("color: green;");
        m_osTestButton->setEnabled(true);
    }, Qt::SingleShotConnection);

    connect(m_osTestClient, &OnlineScoreClient::postFailed, this, [this](const QString& error) {
        // A 404 "contest closed/not valid" from a test post means credentials worked
        if (error.contains("Contest is closed") || error.contains("not valid")) {
            m_osTestStatusLabel->setText("Connected OK (test contest not active)");
            m_osTestStatusLabel->setStyleSheet("color: green;");
        } else {
            m_osTestStatusLabel->setText("Failed: " + error);
            m_osTestStatusLabel->setStyleSheet("color: red;");
        }
        m_osTestButton->setEnabled(true);
    }, Qt::SingleShotConnection);

    connect(m_osTestClient, &OnlineScoreClient::authFailed, this, [this]() {
        m_osTestStatusLabel->setText("Authentication failed — check credentials");
        m_osTestStatusLabel->setStyleSheet("color: red;");
        m_osTestButton->setEnabled(true);
    }, Qt::SingleShotConnection);

    m_osTestClient->postScore(data);
}
