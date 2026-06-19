/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "mainWindow.h"
#include "rigControlDialog.h"
#include "freqModeDialog.h"
#include "cwWindow.h"
#include "cwMemoriesDialog.h"
#include "ssbMemoriesDialog.h"
#include "ssbKeyingSetupDialog.h"
#include "dxClusterPanel.h"
#include "scoreWidget.h"
#include "scpWidget.h"
#include "ssbMemoriesWidget.h"
#include "multiplierWidget.h"
#include "rateWidget.h"
#include "scpLineEdit.h"
#include "stationClassDialog.h"
#include "contestSelectDialog.h"
#include "preferencesDialog.h"
#include "theme.h"
#include "cabrilloDialog.h"
#include "callHistoryDialog.h"
#include "scpDialog.h"
#include "cabrilloFile.h"
#include "contestEngine.h"
#include "fileHandler.h"
#include "clxFile.h"
#include "loadingWorker.h"
#include "scoringWorker.h"
#include "settings.h"
#include "callHistory.h"
#include "superCheckPartial.h"
#include "ttsManager.h"
#include "../utils/bandPlan.h"
#include "debugLogger.h"
#include "debugLogViewer.h"
#include "dxccDatabase.h"
#include "eadxDatabase.h"
#include "onlineScoreClient.h"
#include "cwDecoderWidget.h"
#include "net/clxSnapshot.h"
#include "net/httpServer.h"
#include <QFile>
#include <QUuid>
#include <QJsonArray>
#include <optional>
#include <cmath>
#include <QSpinBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QProgressDialog>
#include <QApplication>
#include <QWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QTabBar>
#include <QRegularExpressionValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QDateTime>
#include <QHeaderView>
#include <QSplitter>
#include <QThread>
#include <QDockWidget>
#include <QCloseEvent>
#include <QShowEvent>
#include <QtMath>
#include <QKeyEvent>
#include <QSettings>
#include <QColor>
#include <QPalette>
#include <QKeySequence>
#include <QShortcut>
#include <QListWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_callEdit(nullptr)
    , m_exchangeEdit(nullptr)
    , m_logButton(nullptr)
    , m_clearButton(nullptr)
    , m_returnToDockLabel(nullptr)
    , m_qrzButton(nullptr)
    , m_qsoTable(nullptr)
    , m_statusLabel(nullptr)
    , m_filterBar(nullptr)
    , m_filterEdit(nullptr)
    , m_filterShortcut(nullptr)
    , m_freqModeButton(nullptr)
    , m_contestNameLabel(nullptr)
    , m_qsoCountLabel(nullptr)
    , m_rigStatusLabel(nullptr)
    , m_wpmLabel(nullptr)
    , m_propagationLabel(nullptr)
    , m_noaaNetworkManager(nullptr)
    , m_noaaPropagationTimer(nullptr)
    , m_noaaPropagationReceived(false)
    , m_entryDock(nullptr)
    , m_dxClusterPanel(nullptr)
    , m_cwConsole(nullptr)
    , m_qsoModel(new QsoListModel(this))
    , m_currentFile("")
    , m_isModified(false)
    , m_showingLogFileNotFoundDialog(false)
    , m_testMode(false)
    , m_debugLogMode(false)
    , m_firstShow(true)
    , m_restoringState(true)
    , m_sessionStationInfo(new StationInfo())
    , m_contestEngine(new ContestEngine(this))
    , m_dxccDatabase(new DxccDatabase(this))
    , m_eadxDatabase(new EadxDatabase(this))
    , m_rigClient(nullptr)
    , m_onlineScoreClient(new OnlineScoreClient(this))
    , m_scorePostTimer(new QTimer(this))
    , m_onlineScoringAction(nullptr)
    , m_onlineScoringLabel(nullptr)
    , m_rigPollTimer(new QTimer(this))
    , m_dupeFlashTimer(new QTimer(this))
    , m_dockStateSaveTimer(new QTimer(this))
    , m_lastFrequency(14250.0)
    , m_lastMode("USB")
    , m_lastWpm(28)
    , m_qrzcqApi(new QrzcqApi(this))
    , m_qrzApi(new QrzApi(this))
    , m_ttsManager(new TtsManager(this))
    , m_backupEnabled(true)
    , m_contextMenuRow(-1)
{
    // Validate startup requirements
    // Check from current working directory first
    QString ctyPath = m_dxccDatabase->getDataPath() + "/cty.dat";
    bool hasCtyDat = QFile::exists(ctyPath);
    
    // Check for contests directory with at least one JSON file
    QDir contestsDir(Settings::getContestsPath());
    QStringList contestFiles = contestsDir.entryList(QStringList() << "*.json", QDir::Files);
    bool hasContestFiles = !contestFiles.isEmpty();
    
    // If no cty.dat AND no contest files, show error and exit
    if (!hasCtyDat && !hasContestFiles) {
        QMessageBox::critical(nullptr, "ContestLogX - Startup Error",
            "ContestLogX was not started from the correct directory.\n\n"
            "Required files not found:\n"
            "  - contests/ directory with contest definition files (*.json)\n"
            "  - data/cty.dat DXCC database file\n\n"
            "Please start ContestLogX from the application directory.");
        qApp->quit();
        return;
    }
    
    // Initialize DXCC database
    m_contestEngine->setDxccDatabase(m_dxccDatabase);
    bool ctyDatLoaded = false;
    if (hasCtyDat) {
        if (m_dxccDatabase->loadFromFile(ctyPath)) {
            DebugLogger::instance().log("MainWindow", "DXCC database loaded successfully");
            ctyDatLoaded = true;
        } else {
            DebugLogger::instance().log("MainWindow", "Failed to load DXCC database");
        }
    } else {
        DebugLogger::instance().log("MainWindow", "cty.dat not found, will prompt for download");
    }

    // Initialize EADX-100 database (URE-curated entity list used by King of Spain
    // and other URE contests). Optional — absence is logged but not fatal, since
    // most contests don't use eadx100 as a multiplier category. Lives in the
    // bundled data directory (shipped with the app, not downloaded), unlike
    // cty.dat which is fetched at runtime to user data.
    m_contestEngine->setEadxDatabase(m_eadxDatabase);
    QString eadxPath = Settings::getDataPath() + "/eadx100.json";
    if (QFile::exists(eadxPath)) {
        if (m_eadxDatabase->loadFromFile(eadxPath)) {
            DebugLogger::instance().log("MainWindow",
                QString("EADX-100 database loaded (%1 active entities)")
                    .arg(m_eadxDatabase->activeEntityCount()));
        } else {
            DebugLogger::instance().log("MainWindow", "Failed to load EADX-100 database");
        }
    } else {
        DebugLogger::instance().log("MainWindow", "eadx100.json not found (optional — only required for URE contests)");
    }
    
    setupUi();
    setupMenus();
    createConnections();
    updateWindowTitle();
    
    // Prompt to download cty.dat if missing (but contest files exist)
    if (!ctyDatLoaded && hasContestFiles) {
        DebugLogger::instance().log("MainWindow", "cty.dat not found, prompting user to download");
        QTimer::singleShot(200, this, &MainWindow::onDownloadCtyDat);
    }
    
    // Setup status bar
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel, 1);
    
    m_contestNameLabel = new QLabel("Contest: None");
    statusBar()->addPermanentWidget(m_contestNameLabel);

    statusBar()->addPermanentWidget(new QLabel(" | "));

    m_memoryTypeLabel = new QPushButton("Station Memories");
    m_memoryTypeLabel->setFlat(true);
    m_memoryTypeLabel->setCursor(Qt::PointingHandCursor);
    m_memoryTypeLabel->setToolTip("Click to toggle Contest / Station memories (Ctrl+T)");
    m_memoryTypeLabel->setFocusPolicy(Qt::NoFocus);
    connect(m_memoryTypeLabel, &QPushButton::clicked, this, &MainWindow::onToggleMemoryType);
    statusBar()->addPermanentWidget(m_memoryTypeLabel);

    statusBar()->addPermanentWidget(new QLabel(" | "));

    m_qsoCountLabel = new QLabel("QSOs: 0");
    statusBar()->addPermanentWidget(m_qsoCountLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_rigStatusLabel = new QLabel("Rig: Disconnected");
    m_rigStatusLabel->setCursor(Qt::PointingHandCursor);
    m_rigStatusLabel->installEventFilter(this);
    statusBar()->addPermanentWidget(m_rigStatusLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_wpmLabel = new QLabel("WPM: --");
    statusBar()->addPermanentWidget(m_wpmLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_propagationLabel = new QLabel("");
    m_propagationLabel->setCursor(Qt::PointingHandCursor);
    m_propagationLabel->installEventFilter(this);
    statusBar()->addPermanentWidget(m_propagationLabel);

    m_onlineScoringSeparator = new QLabel(" | ");
    m_onlineScoringSeparator->hide();
    m_onlineScoringLabel = new QLabel("");
    m_onlineScoringLabel->hide();
    statusBar()->addPermanentWidget(m_onlineScoringSeparator);
    statusBar()->addPermanentWidget(m_onlineScoringLabel);

    // Online score publishing signal connections
    connect(m_onlineScoreClient, &OnlineScoreClient::postSuccess, this, &MainWindow::onScorePostSuccess);
    connect(m_onlineScoreClient, &OnlineScoreClient::postFailed, this, &MainWindow::onScorePostFailed);
    connect(m_onlineScoreClient, &OnlineScoreClient::authFailed, this, &MainWindow::onScorePostAuthFailed);
    connect(m_scorePostTimer, &QTimer::timeout, this, &MainWindow::onPostScore);

    // Create rig client based on saved backend preference
    Settings& settings = Settings::instance();
    m_rigBackend = settings.getRigBackend();
    if (m_rigBackend == "hamlib") {
        m_rigClient = new HamlibClient(this);
    } else if (m_rigBackend == "mocked") {
        m_rigClient = new MockedRigClient(this);
    } else {
        m_rigClient = new FlrigClient(this);
        m_rigBackend = "flrig";
    }

    // Push the freshly-created rig client into widgets that were constructed
    // before m_rigClient existed (CWWindow is created in setupUi() above).
    if (m_cwConsole) {
        m_cwConsole->setRigClient(m_rigClient);
        DebugLogger::instance().log("MainWindow", "CWWindow rigClient pointer set post-construction");
    }
    if (m_ttsManager) {
        m_ttsManager->setRigClient(m_rigClient);
    }

    // Rig signal connections
    connect(m_rigClient, SIGNAL(connected()), this, SLOT(onRigConnected()));
    connect(m_rigClient, SIGNAL(disconnected()), this, SLOT(onRigDisconnected()));

    // Setup rig polling timer (500ms interval by default)
    int pollInterval = settings.getFlrigPollInterval();
    m_rigPollTimer->setInterval(pollInterval);
    connect(m_rigPollTimer, &QTimer::timeout, this, &MainWindow::onUpdateRigDisplay);

    // Initialize session station info from Settings (will be overridden if loading a log file)
    m_sessionStationInfo->setCallsign(settings.getCallsign());
    m_sessionStationInfo->setOperatorName(settings.getOperatorName());
    m_sessionStationInfo->setGrid(settings.getGridSquare());
    m_sessionStationInfo->setState(settings.getState());
    m_sessionStationInfo->setCqZone(settings.getCqZone());
    m_sessionStationInfo->setItuZone(settings.getItuZone());
    m_sessionStationInfo->setArrlSection(settings.getArrlSection());

    // Load CW memories
    loadCWMemories();

    // Check for --test-only mode early so we can skip rig/SO2R setup
    QStringList args = QApplication::arguments();
    int logIndex = args.indexOf("--log");
    int testIndex = args.indexOf("--test-only");

    if (testIndex != -1) {
        m_testMode = true;
        DebugLogger::instance().log("MainWindow", "Test mode enabled — skipping rig auto-connect and SO2R");
    }

    if (!m_testMode) {
        // Auto-reconnect to rig if previously connected
        bool autoConnect = false;
        QString host;
        int port = 0;

        if (m_rigBackend == "hamlib") {
            autoConnect = settings.getHamlibAutoConnect();
            host = settings.getHamlibHost();
            port = settings.getHamlibPort();
        } else if (m_rigBackend == "mocked") {
            autoConnect = settings.getMockedAutoConnect();
            host = "mocked";
        } else {
            autoConnect = settings.getFlrigAutoConnect();
            host = settings.getFlrigHost();
            port = settings.getFlrigPort();
        }

        DebugLogger::instance().log("MainWindow", QString("Rig backend: %1, AutoConnect: %2")
            .arg(m_rigBackend).arg(autoConnect ? "true" : "false"));
        if (autoConnect) {
            DebugLogger::instance().log("MainWindow", QString("Will auto-connect to %1 at %2:%3 in 500ms")
                .arg(m_rigBackend).arg(host).arg(port));
            QTimer::singleShot(500, this, [this, host, port]() {
                DebugLogger::instance().log("MainWindow", QString("Auto-connect timer fired, connecting to %1:%2").arg(host).arg(port));
                if (m_rigClient->connectToRig(host, port)) {
                    onRigConnected();
                }
            });
        } else {
            DebugLogger::instance().log("MainWindow", "Auto-connect disabled");
        }

        // Restore SO2R mode if previously enabled
        if (settings.getSo2rEnabled()) {
            QTimer::singleShot(600, this, [this]() {
                enableSo2r();
                if (m_so2rAction) m_so2rAction->setChecked(true);
            });
        }
    }

    // Start WSJT-X UDP listener (always on unless test mode)
    if (!m_testMode) {
        m_wsjtxListener = new WsjtxListener(this);
        connect(m_wsjtxListener, &WsjtxListener::qsoReceived, this, &MainWindow::onWsjtxQsoReceived);
        int wsjtxPort = settings.getWsjtxPort();
        if (m_wsjtxListener->startListening(wsjtxPort)) {
            DebugLogger::instance().log("MainWindow", QString("WSJT-X listener started on port %1").arg(wsjtxPort));
        }
    }

    // Initialize Remote Control — HTTP server + snapshot. No-op at startup
    // unless the user has enabled it in Preferences. Safe to construct
    // regardless; start() only binds the socket when enabled (TODO item 3).
    if (!m_testMode) {
        initRemoteControl();
    }

    // Fetch propagation data from NOAA on startup and every 15 minutes
    if (!m_testMode) {
        m_noaaNetworkManager = new QNetworkAccessManager(this);
        connect(m_noaaNetworkManager, &QNetworkAccessManager::finished,
                this, &MainWindow::onNoaaPropagationReply);
        m_noaaPropagationTimer = new QTimer(this);
        m_noaaPropagationTimer->setInterval(15 * 60 * 1000);  // 15 minutes
        connect(m_noaaPropagationTimer, &QTimer::timeout, this, &MainWindow::fetchNoaaPropagation);
        m_noaaPropagationTimer->start();
        QTimer::singleShot(2000, this, &MainWindow::fetchNoaaPropagation);
    }

    // Prompt for station setup if callsign is not configured (skip in test mode)
    if (!m_testMode && settings.getCallsign().isEmpty()) {
        DebugLogger::instance().log("MainWindow", "Station not configured, showing preferences dialog");
        QTimer::singleShot(100, this, &MainWindow::onPreferences);
    }

    if (logIndex != -1 && logIndex + 1 < args.count()) {
        QString logFilePath = args[logIndex + 1];
        m_debugLogMode = true;
        DebugLogger::instance().log("MainWindow", QString("Loading log file from command line: %1").arg(logFilePath));
        DebugLogger::instance().log("MainWindow", "Debug log mode enabled - summary sheet will be written to debug log");
        QTimer::singleShot(200, this, [this, logFilePath]() {
            loadLogFile(logFilePath);
        });
    } else if (!settings.getCallsign().isEmpty()) {
        DebugLogger::instance().log("MainWindow", "Station configured, showing contest selection dialog");
        QTimer::singleShot(100, this, &MainWindow::onNewLog);
    }

    // Check if data files (cty.dat, master.scp) are stale (skip in test mode)
    if (!m_testMode) {
        QTimer::singleShot(500, this, &MainWindow::checkDataFileStaleness);
    }

    // Restore window geometry BEFORE the window is shown so X11 window managers
    // treat the saved rect as the initial geometry hint. Restoring after show()
    // races with the WM's placement policy: on X11 the WM maps the window after
    // showEvent fires and overrides any move() we issued from a queued timer,
    // shoving us to its default spot (typically (0, top-of-workspace)) and that
    // wrong position is what gets saved on next close. Panel/dock state restore
    // still happens in showEvent — it requires the widgets to exist and the
    // window to be mapped.
    restoreWindowGeometry();

    // Load saved CW WPM
    int savedWpm = settings.getCwWpm();
    m_lastWpm = savedWpm;
    m_wpmLabel->setText(QString("WPM: %1").arg(savedWpm));

    // Apply saved font settings
    applyFontSettings();
}

MainWindow::~MainWindow()
{
    // Cleanup happens in closeEvent
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        m_firstShow = false;

        // Geometry was restored in the constructor before show(). Restore
        // panel/dock state here, after the window is mapped, so dock sizes
        // aren't clobbered by the initial resize.
        QTimer::singleShot(100, this, [this]() {
            restorePanelState();
            DebugLogger::instance().log("MainWindow", "Panel state restored after window mapped");
        });

        QTimer::singleShot(500, this, [this]() {
            DebugLogger::instance().log("MainWindow",
                QString("Window position 500ms after show: pos=(%1,%2)")
                .arg(pos().x()).arg(pos().y()));
        });
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Check if log has been modified
    if (m_isModified) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            "Unsaved Changes", 
            "The log has been modified. Do you want to save changes before exiting?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Save) {
            // Save the current log
            if (!m_currentFile.isEmpty()) {
                onSaveLog();
            }
        } else if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        } else {
            // Discard — remove crash backup so it doesn't reappear
            removeBackup();
        }
    }

    // Don't save window/panel state in test-only mode — parallel test runs
    // would corrupt the user's settings file
    if (!m_testMode) {
        saveWindowGeometry();
        savePanelState();
    }
    
    // Accept the close event
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(obj);

        // Handle F1-F8 for CW/SSB memories from any widget owned by this window,
        // including when the entry dock is floating (floating docks are separate top-level
        // windows so keyPressEvent() never fires for them).
        // Note: isAncestorOf() only works within the same window, so instead walk the
        // QObject parent chain to detect whether the target belongs to our window.
        // SO2R: switch active radio from floating docks (uses configured shortcut)
        if (m_so2rEnabled && !QApplication::activeModalWidget()) {
            QWidget *sw = qobject_cast<QWidget*>(obj);
            bool ours = false;
            for (QObject *p = obj; p; p = p->parent()) {
                if (p == this) { ours = true; break; }
            }
            if (sw && ours) {
                QString switchKey = Settings::instance().getShortcut("switchRadio");
                if (switchKey.isEmpty()) switchKey = "`";
                int k = keyEvent->key() | (keyEvent->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
                if (QKeySequence(k).toString() == QKeySequence(switchKey).toString()) {
                    switchActiveRadio();
                    return true;
                }
            }
        }

        if (keyEvent->modifiers() == Qt::NoModifier && !QApplication::activeModalWidget()) {
            QWidget *w = qobject_cast<QWidget*>(obj);
            bool belongsToUs = false;
            for (QObject *p = obj; p; p = p->parent()) {
                if (p == this) { belongsToUs = true; break; }
            }
            if (w && belongsToUs) {
                int fKeyIndex = -1;
                switch (keyEvent->key()) {
                    case Qt::Key_F1: fKeyIndex = 0; break;
                    case Qt::Key_F2: fKeyIndex = 1; break;
                    case Qt::Key_F3: fKeyIndex = 2; break;
                    case Qt::Key_F4: fKeyIndex = 3; break;
                    case Qt::Key_F5: fKeyIndex = 4; break;
                    case Qt::Key_F6: fKeyIndex = 5; break;
                    case Qt::Key_F7: fKeyIndex = 6; break;
                    case Qt::Key_F8: fKeyIndex = 7; break;
                    default: break;
                }
                if (fKeyIndex >= 0) {
                    QString mode = m_lastMode.toUpper();
                    DebugLogger::instance().log("MainWindow",
                        QString("Hardware Function key F%1 pressed (eventFilter), current mode: %2").arg(fKeyIndex + 1).arg(mode));
                    if ((mode == "CW" || mode == "CWR") && m_cwConsole) {
                        m_cwConsole->onMemoryButton(fKeyIndex);
                        return true;
                    } else if ((mode == "USB" || mode == "LSB") && m_ssbMemoriesWidget) {
                        m_ssbMemoriesWidget->triggerMemory(fKeyIndex);
                        return true;
                    }
                    return true; // Consume the F-key regardless to avoid system shortcuts
                }
            }
        }

        // Handle registered shortcuts (modifier-key combos) from floating docks.
        // keyPressEvent() never fires for floating dock children, so we dispatch here too.
        if (!QApplication::activeModalWidget() && keyEvent->modifiers() != Qt::NoModifier) {
            QWidget *w = qobject_cast<QWidget*>(obj);
            bool belongsToUs = false;
            for (QObject *p = obj; p; p = p->parent()) {
                if (p == this) { belongsToUs = true; break; }
            }
            if (w && belongsToUs) {
                int keyWithMods = keyEvent->key() | static_cast<int>(keyEvent->modifiers() &
                    (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
                QString eventSeqStr = QKeySequence(keyWithMods).toString();

                QMap<QString, QString> shortcuts = Settings::instance().getShortcuts();
                static const QMap<QString, QString> shortcutDefaults = {
                    {"clearQsoEntry",    "Ctrl+W"},
                    {"preSaveCall",      "Ctrl+S"},
                    {"qsoViewFilter",    "Ctrl+F"},
                    {"toggleRunSP",      "Ctrl+M"},
                    {"toggleMemoryType", "Ctrl+T"},
                    {"qsyBack",          "Alt+B"},
                };
                for (auto dit = shortcutDefaults.begin(); dit != shortcutDefaults.end(); ++dit) {
                    if (!shortcuts.contains(dit.key()))
                        shortcuts[dit.key()] = dit.value();
                }
                for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
                    if (QKeySequence(it.value()).toString() != eventSeqStr) continue;
                    if (it.key() == "clearQsoEntry")    {
                        if (m_so2rEnabled && m_activeRadio == ActiveRadio::Right)
                            clearEntryFormR();
                        else
                            clearEntryForm();
                        return true;
                    }
                    if (it.key() == "preSaveCall")      { preSaveCall();          return true; }
                    if (it.key() == "qsoViewFilter")    {
                        m_filterBar->setVisible(true);
                        m_filterEdit->setFocus();
                        m_filterEdit->selectAll();
                        return true;
                    }
                    if (it.key() == "toggleRunSP")      { onToggleRunSP();        return true; }
                    if (it.key() == "toggleMemoryType") { onToggleMemoryType();   return true; }
                    if (it.key() == "qsyBack")          { onQsyBack();           return true; }
                }
            }
        }

        // Escape halts CW and SSB sending from anywhere in the main window
        if (keyEvent->key() == Qt::Key_Escape) {
            if (m_cwConsole)
                m_cwConsole->onHalt();
            if (m_ttsManager)
                m_ttsManager->cancel();
            // fall through so other Escape handlers (filter bar) still run
        }

        // Escape in the filter bar hides it and clears the filter
        if (obj == m_filterEdit && keyEvent->key() == Qt::Key_Escape) {
            m_filterEdit->clear();
            m_filterBar->setVisible(false);
            m_qsoModel->setFilter("");
            m_statusLabel->setText("Ready");
            activeCallEdit()->setFocus();
            return true;
        }
        
        // Handle Space and/or Tab keys to advance to next text input field (wraps around)
        // Which keys are used depends on contest definition
        bool handleSpace = (m_fieldNavigationKeys == "space" || m_fieldNavigationKeys == "both");
        bool handleTab = (m_fieldNavigationKeys == "tab" || m_fieldNavigationKeys == "both");
        
        if (((keyEvent->key() == Qt::Key_Space && handleSpace) ||
             (keyEvent->key() == Qt::Key_Tab && handleTab)) && lineEdit) {
            // Check if this field is in our entry field list (Radio L)
            int currentIndex = m_entryFieldOrder.indexOf(lineEdit);
            if (currentIndex >= 0) {
                if (lineEdit == m_callEdit && !m_callEdit->text().isEmpty()) {
                    QString callsign = m_callEdit->text().trimmed().toUpper();
                    if (callsign.length() >= 2)
                        triggerAutoLookup(callsign);
                }
                int nextIndex = (currentIndex + 1) % m_entryFieldOrder.size();
                m_entryFieldOrder[nextIndex]->setFocus();
                return true;
            }
            // Check Radio R field list if SO2R active
            if (m_so2rEnabled && !m_entryWidgetsR.entryFieldOrder.isEmpty()) {
                int idxR = m_entryWidgetsR.entryFieldOrder.indexOf(lineEdit);
                if (idxR >= 0) {
                    if (lineEdit == m_entryWidgetsR.callEdit && !m_entryWidgetsR.callEdit->text().isEmpty()) {
                        QString callsign = m_entryWidgetsR.callEdit->text().trimmed().toUpper();
                        if (callsign.length() >= 2)
                            triggerAutoLookup(callsign);
                    }
                    int nextIdx = (idxR + 1) % m_entryWidgetsR.entryFieldOrder.size();
                    m_entryWidgetsR.entryFieldOrder[nextIdx]->setFocus();
                    return true;
                }
            }
        }

        // Handle Shift+Tab to go to previous field (only if tab navigation is enabled)
        if (keyEvent->key() == Qt::Key_Backtab && handleTab && lineEdit) {
            int currentIndex = m_entryFieldOrder.indexOf(lineEdit);
            if (currentIndex >= 0) {
                int prevIndex = (currentIndex - 1 + m_entryFieldOrder.size()) % m_entryFieldOrder.size();
                m_entryFieldOrder[prevIndex]->setFocus();
                return true;
            }
            if (m_so2rEnabled && !m_entryWidgetsR.entryFieldOrder.isEmpty()) {
                int idxR = m_entryWidgetsR.entryFieldOrder.indexOf(lineEdit);
                if (idxR >= 0) {
                    int prevIdx = (idxR - 1 + m_entryWidgetsR.entryFieldOrder.size()) % m_entryWidgetsR.entryFieldOrder.size();
                    m_entryWidgetsR.entryFieldOrder[prevIdx]->setFocus();
                    return true;
                }
            }
        }

        // Up/Down arrows in the SNr field increment/decrement the serial number
        if ((keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) && lineEdit) {
            if (m_exchangeFields.value("SNr") == lineEdit
                || (m_so2rEnabled && m_entryWidgetsR.exchangeFields.value("SNr") == lineEdit)) {
                bool ok = false;
                int sn = lineEdit->text().trimmed().toInt(&ok);
                if (!ok) sn = 0;
                sn += (keyEvent->key() == Qt::Key_Up) ? 1 : -1;
                if (sn < 0) sn = 0;
                lineEdit->setText(QString::number(sn));
                return true;
            }
        }

        // Check if Enter or Return was pressed in any QSO entry field
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            bool inEntryField = (lineEdit == m_callEdit)
                             || (lineEdit && m_exchangeFields.values().contains(lineEdit));
            // Also check Radio R entry fields if SO2R is active
            bool inEntryFieldR = false;
            if (m_so2rEnabled && m_entryWidgetsR.callEdit) {
                inEntryFieldR = (lineEdit == m_entryWidgetsR.callEdit)
                             || (lineEdit && m_entryWidgetsR.exchangeFields.values().contains(lineEdit));
            }
            if (inEntryField || inEntryFieldR) {
                // Activate the correct radio before processing Enter
                if (m_so2rEnabled && inEntryFieldR && m_activeRadio != ActiveRadio::Right) {
                    m_activeRadio = ActiveRadio::Right;
                    updateActiveRadioIndicator();
                } else if (m_so2rEnabled && inEntryField && m_activeRadio != ActiveRadio::Left) {
                    m_activeRadio = ActiveRadio::Left;
                    updateActiveRadioIndicator();
                }
                onQsoEntryReturn();
                return true;
            }
        }
    }
    
    // Click on rig status label opens rig connection settings
    if (event->type() == QEvent::MouseButtonRelease && obj == m_rigStatusLabel) {
        onRigControl();
        return true;
    }

    // Click on propagation label opens NOAA space weather page
    if (event->type() == QEvent::MouseButtonRelease && obj == m_propagationLabel
        && !m_propagationLabel->text().isEmpty()) {
        QDesktopServices::openUrl(QUrl("https://www.swpc.noaa.gov/communities/radio-communications"));
        return true;
    }

    // Pass event to base class
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);
    
    // Create main splitter (left=log/entry, right=cluster+cw)
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // LEFT SIDE: Log table and entry
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2);
    
    // Setup table view FIRST (at top, like ContestLogX)
    m_qsoTable = new QTableView(this);
    m_qsoTable->setModel(m_qsoModel);
    m_qsoTable->setAlternatingRowColors(true);
    m_qsoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_qsoTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_qsoTable->horizontalHeader()->setStretchLastSection(true);
    m_qsoTable->horizontalHeader()->setSortIndicatorShown(true);
    m_qsoTable->setSortingEnabled(true);
    m_qsoTable->verticalHeader()->setVisible(false);
    m_qsoTable->setMinimumHeight(400);
    
    // Freeze the first column (QSO number)
    m_qsoTable->setColumnWidth(0, 40);  // Set width for # column (fits 3-digit QSO numbers)
    
    // Add double-click handling for QSO editing
    connect(m_qsoTable, &QTableView::doubleClicked, this, &MainWindow::onQsoDoubleClicked);
    
    m_qsoTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_qsoTable, &QTableView::customContextMenuRequested, this, &MainWindow::onQsoContextMenuRequested);
    connect(m_qsoTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, &MainWindow::onColumnResized);
    connect(m_qsoTable->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, [this](int logicalIndex, Qt::SortOrder order) {
                QStringList headers = m_qsoModel->columnHeaders();
                if (logicalIndex == 0 && order == Qt::AscendingOrder) {
                    m_statusLabel->setText("Ready");
                } else if (logicalIndex >= 0 && logicalIndex < headers.size()) {
                    QString arrow = (order == Qt::AscendingOrder) ? u8"\u25b2" : u8"\u25bc";
                    m_statusLabel->setText(QString("Sorted by %1 %2")
                        .arg(headers.at(logicalIndex), arrow));
                }
            });
    restoreColumnWidths();

    leftLayout->addWidget(m_qsoTable, 1);

    // Filter bar (hidden by default, shown via Ctrl+F shortcut)
    m_filterBar = new QWidget(this);
    m_filterBar->setVisible(false);
    QHBoxLayout *filterLayout = new QHBoxLayout(m_filterBar);
    filterLayout->setContentsMargins(4, 2, 4, 2);
    filterLayout->setSpacing(6);
    QLabel *filterLabel = new QLabel("Filter:");
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText("Type to filter QSOs...");
    m_filterEdit->installEventFilter(this);
    QPushButton *filterClearBtn = new QPushButton("x");
    filterClearBtn->setMaximumWidth(28);
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_filterEdit, 1);
    filterLayout->addWidget(filterClearBtn);
    leftLayout->addWidget(m_filterBar);
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_qsoModel->setFilter(text);
        if (text.isEmpty()) {
            m_statusLabel->setText("Ready");
        } else {
            int visible = m_qsoModel->rowCount();
            int total = m_qsoModel->count();
            m_statusLabel->setText(QString("Filter: %1 of %2 QSOs").arg(visible).arg(total));
        }
    });
    connect(filterClearBtn, &QPushButton::clicked, this, [this]() {
        m_filterEdit->clear();
        m_filterBar->setVisible(false);
        m_qsoModel->setFilter("");
        m_statusLabel->setText("Ready");
    });

    // Register window-level shortcut for the QSO filter so it fires regardless of focus
    {
        QMap<QString, QString> storedShortcuts = Settings::instance().getShortcuts();
        QString filterKey = storedShortcuts.value("qsoViewFilter", "Ctrl+F");
        m_filterShortcut = new QShortcut(QKeySequence(filterKey), this);
        m_filterShortcut->setContext(Qt::WindowShortcut);
        connect(m_filterShortcut, &QShortcut::activated, this, [this]() {
            m_filterBar->setVisible(true);
            m_filterEdit->setFocus();
            m_filterEdit->selectAll();
        });
    }

    // Entry form at BOTTOM — Radio L
    QWidget *entryPanel = createEntryPanel(m_entryWidgets, QString());

    // Alias Radio L widgets to existing member pointers for backward compatibility
    m_freqModeButton = m_entryWidgets.freqModeButton;
    m_callEdit = m_entryWidgets.callEdit;
    m_exchangeEdit = m_entryWidgets.exchangeEdit;
    m_logButton = m_entryWidgets.logButton;
    m_clearButton = m_entryWidgets.clearButton;
    m_qrzButton = m_entryWidgets.qrzButton;
    m_offButton = m_entryWidgets.offButton;
    m_runButton = m_entryWidgets.runButton;
    m_spButton = m_entryWidgets.spButton;
    m_qsoEntryGroup = m_entryWidgets.entryGroup;
    m_qsoEntryLayout = m_entryWidgets.entryLayout;
    m_returnToDockLabel = m_entryWidgets.returnToDockLabel;

    mainSplitter->addWidget(leftPanel);

    // QSO entry as a bottom dock — floatable but cannot be dragged to the right dock area
    m_entryDock = new QDockWidget("QSO Entry", this);
    m_entryDock->setObjectName("entryDock");  // Required for saveState/restoreState
    m_entryDock->setWidget(entryPanel);
    m_entryDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_entryDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_entryDock);

    // Wire returnToDockLabel and freqModeButton now that dock exists
    connect(m_returnToDockLabel, &QLabel::linkActivated, this, [this]() {
        m_entryDock->setFloating(false);
    });
    connect(m_freqModeButton, &QPushButton::clicked, this, &MainWindow::onFreqModeButtonClicked);

    setupDocks(mainSplitter);

    // Right dock area owns the bottom-right corner so the entry dock
    // only spans the central widget width, not under the right-side docks.
    // Must be set after all right-side docks exist.
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    // Top-right corner is owned by the right dock too, so the CW Decoder
    // (which docks in the Top area above the QSO log) doesn't extend over
    // DX Cluster / Band Map / SCP / etc.
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    mainLayout->addWidget(mainSplitter);
}

QWidget* MainWindow::createEntryPanel(EntryPanelWidgets& w, const QString& radioLabel)
{
    QWidget *panel = new QWidget(this);
    QHBoxLayout *panelLayout = new QHBoxLayout(panel);
    panelLayout->setContentsMargins(2, 2, 2, 2);
    panelLayout->setSpacing(5);

    w.freqModeButton = new QPushButton("14250.0 USB", this);
    w.freqModeButton->setFlat(false);
    w.freqModeButton->setMinimumWidth(100);
    w.freqModeButton->setMaximumWidth(130);
    w.freqModeButton->setMinimumHeight(50);
    w.freqModeButton->setStyleSheet("QPushButton { text-align: center; padding: 4px; font-weight: bold; font-size: 10pt; }");
    panelLayout->addWidget(w.freqModeButton);

    w.entryGroup = new QGroupBox(this);
    w.entryGroup->setObjectName(radioLabel.isEmpty() ? "qsoEntryGroup" : "qsoEntryGroup" + radioLabel);

    QVBoxLayout *groupVLayout = new QVBoxLayout(w.entryGroup);
    groupVLayout->setSpacing(2);
    groupVLayout->setContentsMargins(5, 5, 5, 2);

    w.returnToDockLabel = new QLabel("<a href='dock'>Return to dock</a>");
    w.returnToDockLabel->setTextFormat(Qt::RichText);
    w.returnToDockLabel->setAlignment(Qt::AlignRight);
    w.returnToDockLabel->setVisible(false);
    groupVLayout->addWidget(w.returnToDockLabel);

    w.entryLayout = new QHBoxLayout();
    w.entryLayout->setSpacing(5);
    groupVLayout->addLayout(w.entryLayout);

    w.entryLayout->addWidget(new QLabel("Call:"));
    ScpLineEdit *callEdit = new ScpLineEdit();
    w.callEdit = callEdit;
    callEdit->setMaxLength(14);
    callEdit->setMaximumWidth(120);

    // Force uppercase input
    connect(callEdit, &QLineEdit::textChanged, [callEdit](const QString& text) {
        if (text != text.toUpper()) {
            int cursorPos = callEdit->cursorPosition();
            callEdit->setText(text.toUpper());
            callEdit->setCursorPosition(cursorPos);
        }
    });
    w.entryLayout->addWidget(callEdit);

    w.entryLayout->addWidget(new QLabel("Exchange:"));
    w.exchangeEdit = new QLineEdit();
    w.exchangeEdit->setMinimumWidth(100);
    connect(w.exchangeEdit, &QLineEdit::textChanged, [edit = w.exchangeEdit](const QString& text) {
        if (text != text.toUpper()) {
            int cursorPos = edit->cursorPosition();
            edit->setText(text.toUpper());
            edit->setCursorPosition(cursorPos);
        }
    });
    w.entryLayout->addWidget(w.exchangeEdit);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;

    w.qrzButton = new QPushButton;
    w.qrzButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    w.qrzButton->setFixedSize(32, 32);
    w.qrzButton->setToolTip("Look up callsign on QRZCQ");
    w.qrzButton->setFocusPolicy(Qt::NoFocus);
    buttonLayout->addWidget(w.qrzButton);

    w.logButton = new QPushButton("Log QSO");
    w.logButton->setFocusPolicy(Qt::NoFocus);
    buttonLayout->addWidget(w.logButton);

    w.clearButton = new QPushButton("Clear");
    w.clearButton->setFocusPolicy(Qt::NoFocus);
    buttonLayout->addWidget(w.clearButton);
    buttonLayout->addStretch();

    // Run / S&P / Off toggle buttons
    w.offButton = new QPushButton("OFF");
    w.offButton->setCheckable(true);
    w.offButton->setChecked(true);
    w.offButton->setToolTip("Enter key sequences disabled — logs QSO directly");
    w.offButton->setFixedWidth(46);
    w.offButton->setFocusPolicy(Qt::NoFocus);
    buttonLayout->addWidget(w.offButton);

    w.runButton = new QPushButton("RUN");
    w.runButton->setCheckable(true);
    w.runButton->setChecked(false);
    w.runButton->setToolTip("Run mode: you are calling CQ (Enter sequences CQ → Exchange → TU+Log)");
    w.runButton->setFixedWidth(46);
    w.runButton->setFocusPolicy(Qt::NoFocus);
    buttonLayout->addWidget(w.runButton);

    w.spButton = new QPushButton("S&&P");
    w.spButton->setCheckable(true);
    w.spButton->setChecked(false);
    w.spButton->setToolTip("Search & Pounce: you are answering a CQ (Enter sequences My Call → Exchange → Log)");
    w.spButton->setFixedWidth(46);
    w.spButton->setFocusPolicy(Qt::NoFocus);
    buttonLayout->addWidget(w.spButton);

    w.entryLayout->addLayout(buttonLayout);

    // Tab order
    setTabOrder(w.callEdit, w.exchangeEdit);
    setTabOrder(w.exchangeEdit, w.qrzButton);
    setTabOrder(w.qrzButton, w.logButton);
    setTabOrder(w.logButton, w.clearButton);

    // Event filters for Enter key handling
    w.callEdit->installEventFilter(this);
    w.exchangeEdit->installEventFilter(this);

    panelLayout->addWidget(w.entryGroup, 1);
    return panel;
}

void MainWindow::setupDocks(QSplitter* mainSplitter)
{
    // Convert all right side panels to QDockWidgets for flexibility
    
    // DX Cluster Panel as QDockWidget
    m_dxClusterDock = new QDockWidget("DX Cluster", this);
    m_dxClusterDock->setObjectName("dxClusterDock");  // Required for saveState/restoreState
    m_dxClusterPanel = new DxClusterPanel(m_dxClusterDock);
    m_dxClusterPanel->setMinimumHeight(200);
    m_dxClusterDock->setWidget(m_dxClusterPanel);
    m_dxClusterDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dxClusterDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_dxClusterDock);
    connect(m_dxClusterDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_dxClusterAction) m_dxClusterAction->setChecked(visible);
        if (!m_restoringState && m_dockStateSaveTimer) m_dockStateSaveTimer->start();
    });

    // Connect propagation data signal
    connect(m_dxClusterPanel, &DxClusterPanel::propagationDataReceived, 
            this, &MainWindow::onPropagationDataReceived);
    
    // Connect spot clicked signal to change rig frequency/mode
    connect(m_dxClusterPanel, &DxClusterPanel::spotClicked,
            this, &MainWindow::onDxSpotClicked);

    // Connect spot last QSO signal
    connect(m_dxClusterPanel, &DxClusterPanel::spotLastQsoRequested,
            this, &MainWindow::onSpotLastQso);
    
    // CW Console as QDockWidget
    m_cwConsoleDock = new QDockWidget("CW Console", this);
    m_cwConsoleDock->setObjectName("cwConsoleDock");  // Required for saveState/restoreState
    m_cwConsole = new CWWindow(m_rigClient, m_cwConsoleDock);
    m_cwConsole->setMinimumHeight(160);
    // Set up any configured CW keyer(s) (e.g. WinKeyer) and route the console.
    // Deferred so a serial open/handshake doesn't block window construction.
    QTimer::singleShot(300, this, [this]() {
        setupCwKeyers();
        updateCwConsoleRouting();
    });
    m_cwConsoleDock->setWidget(m_cwConsole);
    m_cwConsoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_cwConsoleDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_cwConsoleDock);
    connect(m_cwConsoleDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_cwConsoleAction) m_cwConsoleAction->setChecked(visible);
        if (!m_restoringState && m_dockStateSaveTimer) m_dockStateSaveTimer->start();
    });

    // Score Widget as QDockWidget
    m_scoreDock = new QDockWidget("Score", this);
    m_scoreDock->setObjectName("scoreDock");  // Required for saveState/restoreState
    m_scoreWidget = new ScoreWidget(m_scoreDock);
    m_scoreWidget->setMinimumHeight(160);
    m_scoreDock->setWidget(m_scoreWidget);
    m_scoreDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_scoreDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_scoreDock);
    connect(m_scoreDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_scoreWidgetAction) m_scoreWidgetAction->setChecked(visible);
    });

    // SCP Widget as QDockWidget
    m_scpWidget = new ScpWidget(this);
    m_scpWidget->setMinimumHeight(80);
    // No maximum height - allow user to resize as needed
    m_scpWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_scpWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_scpWidget);
    
    // Position SCP widget below DX Cluster - use splitDockWidget to control placement
    // This ensures SCP stays under DX Cluster, and CW Console/Score widgets are below it
    splitDockWidget(m_dxClusterDock, m_scpWidget, Qt::Vertical);
    
    m_scpWidget->hide();  // Hidden by default, user can show via Window menu

    // Open SCP config dialog when the user clicks the "disabled" placeholder
    connect(m_scpWidget, &ScpWidget::configureRequested, this, &MainWindow::onScpDialog);

    // Store as m_scpDock for consistency with other docks
    m_scpDock = m_scpWidget;

    // SSB Memories Widget (also a QDockWidget)
    m_ssbMemoriesWidget = new SsbMemoriesWidget(this);
    m_ssbMemoriesWidget->setObjectName("ssbMemoriesWidget");  // Required for saveState/restoreState
    m_ssbMemoriesWidget->setMinimumHeight(100);
    // No maximum height - allow user to resize as needed
    m_ssbMemoriesWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_ssbMemoriesWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_ssbMemoriesWidget);
    m_ssbMemoriesWidget->hide();  // Hidden by default, user can show via Window menu
    connect(m_ssbMemoriesWidget, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_ssbMemoriesWidgetAction)
            m_ssbMemoriesWidgetAction->setChecked(visible);
    });

    // Multiplier Widget as QDockWidget
    m_multiplierDock = new QDockWidget("Multipliers", this);
    m_multiplierDock->setObjectName("multiplierDock");  // Required for saveState/restoreState
    m_multiplierWidget = new MultiplierWidget(m_multiplierDock);
    m_multiplierWidget->setMinimumHeight(150);
    m_multiplierDock->setWidget(m_multiplierWidget);
    m_multiplierDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_multiplierDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_multiplierDock);
    m_multiplierDock->hide();  // Hidden by default
    connect(m_multiplierDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_multiplierWidgetAction) {
            m_multiplierWidgetAction->setChecked(visible);
        }
    });

    // Rate Widget
    m_rateDock = new QDockWidget("Rate & Stats", this);
    m_rateDock->setObjectName("rateDock");
    m_rateWidget = new RateWidget(m_rateDock);
    m_rateDock->setWidget(m_rateWidget);
    m_rateDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_rateDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_rateDock);
    m_rateDock->hide();  // Hidden by default
    connect(m_rateDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_rateWidgetAction)
            m_rateWidgetAction->setChecked(visible);
    });

    createBandMapDock();

    // If the last session quit with a Practice audio source selected, reset
    // it to "(none)" so CLX doesn't start generating CW on the speakers the
    // moment it launches. The operator must explicitly re-select Practice
    // each session. Only practice-* sentinels are stripped; real device
    // names persist across restarts as before.
    {
        Settings& s = Settings::instance();
        const QString dl = s.getRadioLAudioInputDevice();
        const QString dr = s.getRadioRAudioInputDevice();
        if (dl.startsWith(QStringLiteral("practice-")))
            s.setRadioLAudioInputDevice(QString());
        if (dr.startsWith(QStringLiteral("practice-")))
            s.setRadioRAudioInputDevice(QString());
    }

    // Spawn CW decoder widgets if an audio input device has been configured
    // per radio (SPEC-005). Safe to call even when no device is configured —
    // the method is a no-op in that case.
    spawnOrRefreshCwDecoders();

    // Install redock-on-minimize for all right-side docks — consistent with the
    // entry dock behaviour (clicking the OS minimize button re-docks instead
    // of minimizing to the taskbar, which confuses users looking for the dock).
    installRedockOnMinimize(m_dxClusterDock);
    installRedockOnMinimize(m_cwConsoleDock);
    installRedockOnMinimize(m_scoreDock);
    installRedockOnMinimize(m_scpWidget);
    installRedockOnMinimize(m_ssbMemoriesWidget);
    installRedockOnMinimize(m_multiplierDock);
    installRedockOnMinimize(m_rateDock);
    installRedockOnMinimize(m_bandMapWidget);

    // Load SSB memories (from contest or settings)
    loadSsbMemories();

    // Connect memory triggered signal to TTS manager
    connect(m_ssbMemoriesWidget, &SsbMemoriesWidget::memoryTriggered,
            this, &MainWindow::onSsbMemoryTriggered);

    // Connect CW memory triggered signal for macro substitution
    connect(m_cwConsole, &CWWindow::memoryTriggered,
            this, &MainWindow::onCwMemoryTriggered);

    // Route internal CW-send events → decoder mute (SPEC-005 FR-019c).
    // Catches both F-key memories and manual CW-console sends at their single
    // choke point in CWWindow::sendCWText.
    connect(m_cwConsole, &CWWindow::aboutToSendCw, this,
            [this](RigInterface* rig, const QString& text, int wpm) {
        const bool isRight = (rig == m_rigClientR);
        notifyInternalCwSend(isRight, text.length(), wpm);
    });

    // Enable nested docking and animated docks
    setDockNestingEnabled(true);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    
    // Check if we have saved dock state
    Settings& settings = Settings::instance();
    QByteArray savedDockState = settings.getDockWidgetState();
    
    if (savedDockState.isEmpty()) {
        // No saved state - set up default layout
        splitDockWidget(m_dxClusterDock, m_cwConsoleDock, Qt::Vertical);
        splitDockWidget(m_cwConsoleDock, m_scoreDock, Qt::Vertical);
        DebugLogger::instance().log("MainWindow", "Using default dock layout");
    } else {
        // Will restore saved state later in restorePanelState()
        DebugLogger::instance().log("MainWindow", "Will restore saved dock layout");
    }
    
    // Store splitter reference
    m_mainSplitter = mainSplitter;
    
    // Set initial splitter sizes (70% left, 30% right)
    mainSplitter->setStretchFactor(0, 7);
    mainSplitter->setStretchFactor(1, 3);
    
    // Restore splitter sizes from settings (already have settings reference above)
    QList<int> mainSizes = settings.getMainSplitterSizes();
    if (!mainSizes.isEmpty() && mainSizes.size() == 2) {
        m_mainSplitter->setSizes(mainSizes);
    }
    
    // Connect splitter moved signal to save sizes
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, [this]() {
        Settings::instance().setMainSplitterSizes(m_mainSplitter->sizes());
    });
    
    // Save dock state when dock widgets are moved or resized
    // Use a debounce timer to avoid excessive saves during drag operations
    // Don't trigger during state restoration to avoid overwriting the restored state
    connect(m_entryDock, &QDockWidget::dockLocationChanged, this, [this]() {
        if (!m_restoringState) m_dockStateSaveTimer->start();
    });
    connect(m_entryDock, &QDockWidget::topLevelChanged, this, [this](bool floating) {
        if (m_floatEntryAction) m_floatEntryAction->setChecked(floating);
        if (m_returnToDockLabel) m_returnToDockLabel->setVisible(floating);
        if (!m_restoringState) m_dockStateSaveTimer->start();

        // Disconnect any previous QWindow visibility watcher
        disconnect(m_entryWindowVisConn);

        if (floating) {
            // Defer one tick so the native QWindow is fully created, then connect directly
            // to QWindow::visibilityChanged — this fires reliably for minimize on all platforms.
            QTimer::singleShot(0, this, [this]() {
                if (!m_entryDock->isFloating()) return;
                QWindow *win = m_entryDock->window()->windowHandle();
                if (!win) return;
                win->setFlag(Qt::WindowMaximizeButtonHint, false);
                m_entryWindowVisConn = connect(win, &QWindow::visibilityChanged,
                        this, [this](QWindow::Visibility v) {
                    if (v == QWindow::Minimized || v == QWindow::Maximized || v == QWindow::Hidden)
                        QTimer::singleShot(0, this, [this]() { m_entryDock->setFloating(false); });
                });
            });
        }
    });

    // Re-show the entry dock whenever it is hidden — this fires when the user closes
    // the floating window via the OS title-bar X button.  We use a queued singleShot so
    // the close event finishes processing before we re-show.
    connect(m_entryDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible && !m_restoringState)
            QTimer::singleShot(0, m_entryDock, &QWidget::show);
    });
    connect(m_dxClusterDock, &QDockWidget::dockLocationChanged, this, [this]() {
        if (!m_restoringState) m_dockStateSaveTimer->start();  // Restart timer (debounce)
    });
    connect(m_cwConsoleDock, &QDockWidget::dockLocationChanged, this, [this]() {
        if (!m_restoringState) m_dockStateSaveTimer->start();
    });
    connect(m_scoreDock, &QDockWidget::dockLocationChanged, this, [this]() {
        if (!m_restoringState) m_dockStateSaveTimer->start();
    });
    if (m_scpWidget) {
        connect(m_scpWidget, &QDockWidget::dockLocationChanged, this, [this]() {
            if (!m_restoringState) m_dockStateSaveTimer->start();
        });
    }
    if (m_ssbMemoriesWidget) {
        connect(m_ssbMemoriesWidget, &QDockWidget::dockLocationChanged, this, [this]() {
            if (!m_restoringState) m_dockStateSaveTimer->start();
        });
    }
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    QAction *newAction = fileMenu->addAction("&New Log");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewLog);
    
    QAction *openAction = fileMenu->addAction("&Open Log...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenLog);
    
    QAction *saveAction = fileMenu->addAction("&Save Log");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveLog);
    
    QAction *saveAsAction = fileMenu->addAction("Save Log &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveLogAs);

    QAction *exportAction = fileMenu->addAction("&Export...");
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportAdif);

    QAction *importAction = fileMenu->addAction("&Import...");
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportAdif);

    fileMenu->addSeparator();

    QAction *downloadCtyAction = fileMenu->addAction("&Download DXCC Database (cty.dat)...");
    connect(downloadCtyAction, &QAction::triggered, this, &MainWindow::onDownloadCtyDat);
    
    QAction *downloadScpAction = fileMenu->addAction("Download Super Check Partial (master.scp)...");
    connect(downloadScpAction, &QAction::triggered, this, &MainWindow::onDownloadScp);
    
    fileMenu->addSeparator();
    
    QAction *callHistoryAction = fileMenu->addAction("Manage &Call History...");
    connect(callHistoryAction, &QAction::triggered, this, &MainWindow::onManageCallHistory);

    QAction *importCallHistoryAction = fileMenu->addAction("&Import Call History...");
    connect(importCallHistoryAction, &QAction::triggered, this, &MainWindow::onImportCallHistory);

    fileMenu->addSeparator();

    QAction *preferencesAction = fileMenu->addAction("&Preferences...");
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::onPreferences);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    // Rig menu
    QMenu *rigMenu = menuBar()->addMenu("&Rig");
    
    QAction *rigControlAction = rigMenu->addAction("Rig &Connection...");
    connect(rigControlAction, &QAction::triggered, this, &MainWindow::onRigControl);
    
    rigMenu->addSeparator();
    
    QAction *editCWMemAction = rigMenu->addAction("Edit CW &Memories...");
    connect(editCWMemAction, &QAction::triggered, this, &MainWindow::onEditCWMemories);

    QAction *editSSBMemAction = rigMenu->addAction("Edit &SSB Memories...");
    connect(editSSBMemAction, &QAction::triggered, this, &MainWindow::onEditSsbMemories);

    QAction *ssbKeyingAction = rigMenu->addAction("SSB &Keying Setup...");
    connect(ssbKeyingAction, &QAction::triggered, this, &MainWindow::onSsbKeyingSetup);

    rigMenu->addSeparator();
    m_so2rAction = rigMenu->addAction("SO&2R Mode");
    m_so2rAction->setCheckable(true);
    m_so2rAction->setChecked(Settings::instance().getSo2rEnabled());
    connect(m_so2rAction, &QAction::toggled, this, &MainWindow::onToggleSo2r);

    // Contest menu
    QMenu *contestMenu = menuBar()->addMenu("&Contest");
    
    QAction *recalcScoreAction = contestMenu->addAction("&Recalculate score");
    connect(recalcScoreAction, &QAction::triggered, this, &MainWindow::onRecalculateScore);
    
    contestMenu->addSeparator();
    
    QAction *setupAction = contestMenu->addAction("Contest &Setup...");
    connect(setupAction, &QAction::triggered, this, &MainWindow::onContestSetup);

    QAction *operatorCallAction = contestMenu->addAction("&Station info...");
    connect(operatorCallAction, &QAction::triggered, this, &MainWindow::onOperatorCallDialog);

    contestMenu->addSeparator();
    
    QAction *scpAction = contestMenu->addAction("&Super Check Partial...");
    connect(scpAction, &QAction::triggered, this, &MainWindow::onScpDialog);

    QAction *showMultAction = contestMenu->addAction("Show &Multipliers...");
    connect(showMultAction, &QAction::triggered, this, &MainWindow::onShowMultipliers);

    contestMenu->addSeparator();

    QAction *cabrilloAction = contestMenu->addAction("&Generate Cabrillo log...");
    connect(cabrilloAction, &QAction::triggered, this, &MainWindow::onExportCabrillo);
    
    QAction *summaryAction = contestMenu->addAction("&Create summary sheet...");
    connect(summaryAction, &QAction::triggered, this, &MainWindow::onCreateSummarySheet);

    contestMenu->addSeparator();

    m_onlineScoringAction = contestMenu->addAction("&Online Score Publishing");
    m_onlineScoringAction->setCheckable(true);
    m_onlineScoringAction->setChecked(false);
    connect(m_onlineScoringAction, &QAction::toggled, this, &MainWindow::onToggleOnlineScoring);

    contestMenu->addSeparator();

    QAction *contestCalendarAction = contestMenu->addAction("Contest Ca&lendar");
    connect(contestCalendarAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://www.contestcalendar.com/weeklycont.php"));
    });

    // Window menu
    QMenu *windowMenu = menuBar()->addMenu("&Window");
    
    m_floatEntryAction = windowMenu->addAction("Float &QSO Entry");
    m_floatEntryAction->setCheckable(true);
    m_floatEntryAction->setChecked(false);
    m_floatEntryAction->setToolTip("Detach the QSO entry panel into a floating window");
    connect(m_floatEntryAction, &QAction::triggered, this, [this](bool checked) {
        if (!m_entryDock->isVisible())
            m_entryDock->show();
        m_entryDock->setFloating(checked);
    });

    windowMenu->addSeparator();
    m_dxClusterAction = windowMenu->addAction("DX &Cluster");
    m_dxClusterAction->setCheckable(true);
    m_dxClusterAction->setChecked(true);
    connect(m_dxClusterAction, &QAction::triggered, this, &MainWindow::onToggleDxCluster);
    
    m_cwConsoleAction = windowMenu->addAction("CW C&onsole");
    m_cwConsoleAction->setCheckable(true);
    m_cwConsoleAction->setChecked(true);
    connect(m_cwConsoleAction, &QAction::triggered, this, &MainWindow::onToggleCwConsole);
    
    m_scoreWidgetAction = windowMenu->addAction("&Score");
    m_scoreWidgetAction->setCheckable(true);
    m_scoreWidgetAction->setChecked(true);
    connect(m_scoreWidgetAction, &QAction::triggered, this, &MainWindow::onToggleScoreWidget);
    
    m_scpWidgetAction = windowMenu->addAction("&Super Check Partial");
    m_scpWidgetAction->setCheckable(true);
    m_scpWidgetAction->setChecked(false);  // Hidden by default
    connect(m_scpWidgetAction, &QAction::triggered, this, &MainWindow::onToggleScpWidget);

    m_ssbMemoriesWidgetAction = windowMenu->addAction("SS&B Memories");
    m_ssbMemoriesWidgetAction->setCheckable(true);
    m_ssbMemoriesWidgetAction->setChecked(false);  // Hidden by default
    connect(m_ssbMemoriesWidgetAction, &QAction::triggered, this, &MainWindow::onToggleSsbMemoriesWidget);

    m_multiplierWidgetAction = windowMenu->addAction("&Multipliers");
    m_multiplierWidgetAction->setCheckable(true);
    m_multiplierWidgetAction->setChecked(false);  // Hidden by default
    connect(m_multiplierWidgetAction, &QAction::triggered, this, &MainWindow::onToggleMultiplierWidget);

    m_rateWidgetAction = windowMenu->addAction("&Rate && Stats");
    m_rateWidgetAction->setCheckable(true);
    m_rateWidgetAction->setChecked(false);  // Hidden by default
    connect(m_rateWidgetAction, &QAction::triggered, this, &MainWindow::onToggleRateWidget);

    m_bandMapWidgetAction = windowMenu->addAction("&Band Map");
    m_bandMapWidgetAction->setCheckable(true);
    m_bandMapWidgetAction->setChecked(false);  // Hidden by default
    connect(m_bandMapWidgetAction, &QAction::triggered, this, &MainWindow::onToggleBandMap);

    // CW Decoder entries — one per radio. Always visible (so operators can
    // discover the feature); if no audio device is configured, clicking opens
    // the Rig Connection Settings dialog so they can set one.
    auto makeDecoderAction = [this, windowMenu](const QString& label, bool right) {
        QAction* a = windowMenu->addAction(label);
        a->setCheckable(true);
        a->setChecked(false);
        connect(a, &QAction::triggered, this, [this, right](bool checked) {
            CwDecoderWidget* w = right ? m_cwDecoderRight : m_cwDecoderLeft;
            if (w) {
                w->setVisible(checked);
                return;
            }
            // No widget exists — operator hasn't configured an audio input yet.
            // Uncheck immediately and prompt them to the settings dialog.
            QAction* self = right ? m_cwDecoderRightAction : m_cwDecoderLeftAction;
            if (self) self->setChecked(false);
            QMessageBox::information(this, tr("CW Decoder"),
                tr("To enable the CW Decoder for %1, set an Audio Input Device "
                   "in Rig Connection Settings.\n\nOpening that dialog now.")
                    .arg(right ? tr("Radio R") : tr("Radio L")));
            onRigControl();
        });
        return a;
    };
    m_cwDecoderLeftAction  = makeDecoderAction(tr("CW &Decoder (Radio L)"), false);
    m_cwDecoderRightAction = makeDecoderAction(tr("CW Decoder (Radio &R)"), true);
    // Hide Radio R entry when SO2R is off; refresh handled on SO2R toggle.
    m_cwDecoderRightAction->setVisible(m_so2rEnabled);

    windowMenu->addSeparator();
    QAction* dockAllAction = windowMenu->addAction("&Dock All Panels");
    connect(dockAllAction, &QAction::triggered, this, [this]() {
        const QList<QDockWidget*> docks = findChildren<QDockWidget*>();
        for (QDockWidget* dock : docks)
            if (dock->isFloating()) dock->setFloating(false);
    });
    QAction* resetLayoutAction = windowMenu->addAction("&Reset Widget Positions...");
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetWidgetPositions);

    // Debug menu
    QMenu *debugMenu = menuBar()->addMenu("&Debug");
    
    m_flrigDebugAction = debugMenu->addAction("Enable &Rig Debug Logging");
    m_flrigDebugAction->setCheckable(true);
    bool flrigDebugEnabled = Settings::instance().getFlrigDebugEnabled();
    m_flrigDebugAction->setChecked(flrigDebugEnabled);
    DebugLogger::instance().setFlrigDebugEnabled(flrigDebugEnabled);
    connect(m_flrigDebugAction, &QAction::triggered, this, &MainWindow::onToggleFlrigDebug);
    
    // MainWindow and CWWindow debug logging are always enabled — too critical for triage

    m_contestEngineDebugAction = debugMenu->addAction("Enable &ContestEngine Debug Logging");
    m_contestEngineDebugAction->setCheckable(true);
    bool contestEngineDebugEnabled = Settings::instance().getContestEngineDebugEnabled();
    m_contestEngineDebugAction->setChecked(contestEngineDebugEnabled);
    DebugLogger::instance().setContestEngineDebugEnabled(contestEngineDebugEnabled);
    connect(m_contestEngineDebugAction, &QAction::triggered, this, &MainWindow::onToggleContestEngineDebug);

    m_contestSelectDialogDebugAction = debugMenu->addAction("Enable Contest&SelectDialog Debug Logging");
    m_contestSelectDialogDebugAction->setCheckable(true);
    bool contestSelectDialogDebugEnabled = Settings::instance().getContestSelectDialogDebugEnabled();
    m_contestSelectDialogDebugAction->setChecked(contestSelectDialogDebugEnabled);
    DebugLogger::instance().setContestSelectDialogDebugEnabled(contestSelectDialogDebugEnabled);
    connect(m_contestSelectDialogDebugAction, &QAction::triggered, this, &MainWindow::onToggleContestSelectDialogDebug);
    
    m_dxccDatabaseDebugAction = debugMenu->addAction("Enable &DxccDatabase Debug Logging");
    m_dxccDatabaseDebugAction->setCheckable(true);
    bool dxccDatabaseDebugEnabled = Settings::instance().getDxccDatabaseDebugEnabled();
    m_dxccDatabaseDebugAction->setChecked(dxccDatabaseDebugEnabled);
    DebugLogger::instance().setDxccDatabaseDebugEnabled(dxccDatabaseDebugEnabled);
    connect(m_dxccDatabaseDebugAction, &QAction::triggered, this, &MainWindow::onToggleDxccDatabaseDebug);

    m_dxClusterDebugAction = debugMenu->addAction("Enable DX C&luster Debug Logging");
    m_dxClusterDebugAction->setCheckable(true);
    bool dxClusterDebugEnabled = Settings::instance().getDxClusterDebugEnabled();
    m_dxClusterDebugAction->setChecked(dxClusterDebugEnabled);
    DebugLogger::instance().setDxClusterDebugEnabled(dxClusterDebugEnabled);
    connect(m_dxClusterDebugAction, &QAction::triggered, this, &MainWindow::onToggleDxClusterDebug);

    m_scpDebugAction = debugMenu->addAction("Enable &Super Check Partial Debug Logging");
    m_scpDebugAction->setCheckable(true);
    bool scpDebugEnabled = Settings::instance().getScpDebugEnabled();
    m_scpDebugAction->setChecked(scpDebugEnabled);
    DebugLogger::instance().setScpDebugEnabled(scpDebugEnabled);
    connect(m_scpDebugAction, &QAction::triggered, this, &MainWindow::onToggleScpDebug);

    m_multiplierWidgetDebugAction = debugMenu->addAction("Enable &Multiplier Widget Debug Logging");
    m_multiplierWidgetDebugAction->setCheckable(true);
    bool multiplierWidgetDebugEnabled = Settings::instance().getMultiplierWidgetDebugEnabled();
    m_multiplierWidgetDebugAction->setChecked(multiplierWidgetDebugEnabled);
    DebugLogger::instance().setMultiplierWidgetDebugEnabled(multiplierWidgetDebugEnabled);
    connect(m_multiplierWidgetDebugAction, &QAction::triggered, this, &MainWindow::onToggleMultiplierWidgetDebug);

    m_callsignLookupDebugAction = debugMenu->addAction("Enable &Callsign Lookup Debug Logging");
    m_callsignLookupDebugAction->setCheckable(true);
    bool callsignLookupDebugEnabled = Settings::instance().getCallsignLookupDebugEnabled();
    m_callsignLookupDebugAction->setChecked(callsignLookupDebugEnabled);
    DebugLogger::instance().setCallsignLookupDebugEnabled(callsignLookupDebugEnabled);
    connect(m_callsignLookupDebugAction, &QAction::triggered, this, &MainWindow::onToggleCallsignLookupDebug);

    m_wsjtxDebugAction = debugMenu->addAction("Enable &WSJT-X Debug Logging");
    m_wsjtxDebugAction->setCheckable(true);
    bool wsjtxDebugEnabled = Settings::instance().getWsjtxDebugEnabled();
    m_wsjtxDebugAction->setChecked(wsjtxDebugEnabled);
    DebugLogger::instance().setWsjtxDebugEnabled(wsjtxDebugEnabled);
    connect(m_wsjtxDebugAction, &QAction::triggered, this, [this](bool checked) {
        DebugLogger::instance().setWsjtxDebugEnabled(checked);
        Settings::instance().setWsjtxDebugEnabled(checked);
    });

    m_cwDecoderDebugAction = debugMenu->addAction("Enable CW &Decoder Debug Logging");
    m_cwDecoderDebugAction->setCheckable(true);
    bool cwDecoderDebugEnabled = Settings::instance().getCwDecoderDebugEnabled();
    m_cwDecoderDebugAction->setChecked(cwDecoderDebugEnabled);
    DebugLogger::instance().setCwDecoderDebugEnabled(cwDecoderDebugEnabled);
    connect(m_cwDecoderDebugAction, &QAction::triggered, this, [this](bool checked) {
        DebugLogger::instance().setCwDecoderDebugEnabled(checked);
        Settings::instance().setCwDecoderDebugEnabled(checked);
    });

    m_winKeyerDebugAction = debugMenu->addAction("Enable Win&Keyer Debug Logging");
    m_winKeyerDebugAction->setCheckable(true);
    bool winKeyerDebugEnabled = Settings::instance().getWinKeyerDebugEnabled();
    m_winKeyerDebugAction->setChecked(winKeyerDebugEnabled);
    DebugLogger::instance().setWinKeyerDebugEnabled(winKeyerDebugEnabled);
    connect(m_winKeyerDebugAction, &QAction::triggered, this, [this](bool checked) {
        DebugLogger::instance().setWinKeyerDebugEnabled(checked);
        Settings::instance().setWinKeyerDebugEnabled(checked);
    });

    debugMenu->addSeparator();
    m_viewDebugLogAction = debugMenu->addAction(tr("&View Debug Log"));
    connect(m_viewDebugLogAction, &QAction::triggered, this, &MainWindow::onShowDebugLogViewer);

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::loadQsosIntoModel(const QList<QsoRecord>& qsos, QProgressDialog* progressDialog)
{
    progressDialog->setLabelText("Loading QSOs...");
    m_qsoModel->replaceAll(qsos);
    m_qsoCountLabel->setText(QString("QSOs: %1").arg(m_qsoModel->count()));
    progressDialog->setValue(qsos.size());
}

// Scans the loaded contest definition's userPrompts for entries with "restrictMode": true,
// then applies setRestrictedMode() using the matching value already stored in the engine.
void MainWindow::applyRestrictedModeFromUserPrompts()
{
    if (!m_contestEngine || m_contestDefinition.isEmpty()) return;
    QJsonArray prompts = m_contestDefinition["userPrompts"].toArray();
    for (const QJsonValue& pv : prompts) {
        QJsonObject p = pv.toObject();
        if (p["restrictMode"].toBool(false)) {
            QString id = p["id"].toString();
            QString modeValue = m_contestEngine->getUserPromptValue(id);
            if (!modeValue.isEmpty()) {
                m_contestEngine->setRestrictedMode(modeValue);
                DebugLogger::instance().log("MainWindow",
                    QString("Applied restricted mode '%1' from userPrompt '%2'").arg(modeValue, id));
            }
        }
    }
}

bool MainWindow::isFieldVisible(const QString& columnName) const
{
    if (!m_contestEngine || !m_contestDefinition.contains("qsoFields"))
        return true;
    QJsonArray qsoFields = m_contestDefinition["qsoFields"].toArray();
    for (const QJsonValue& fieldVal : qsoFields) {
        QJsonObject field = fieldVal.toObject();
        if (field["column"].toString() == columnName && field.contains("visibleWhen")) {
            QJsonObject vw = field["visibleWhen"].toObject();
            QString promptId = vw["promptId"].toString();
            QJsonArray values = vw["values"].toArray();
            QString actual = m_contestEngine->getUserPromptValue(promptId);
            for (const QJsonValue& v : values)
                if (v.toString() == actual) return true;
            return false;
        }
    }
    return true;
}

void MainWindow::promptForMissingUserPrompts()
{
    if (!m_contestEngine || m_contestDefinition.isEmpty()) return;
    if (!m_contestDefinition.contains("userPrompts")) return;

    QJsonArray prompts = m_contestDefinition["userPrompts"].toArray();
    bool anyPrompted = false;

    for (const QJsonValue& promptVal : prompts) {
        QJsonObject p = promptVal.toObject();
        QString promptId = p["id"].toString();
        bool required    = p["required"].toBool(false);

        // Skip if already answered
        if (!m_contestEngine->getUserPromptValue(promptId).isEmpty()) continue;
        // Skip non-required prompts that are missing — don't bother the user
        if (!required) continue;
        // Skip prompts with unmet visibleWhen conditions
        if (p.contains("visibleWhen")) {
            QJsonObject vw = p["visibleWhen"].toObject();
            QString depId = vw["promptId"].toString();
            QJsonArray vals = vw["values"].toArray();
            QString actual = m_contestEngine->getUserPromptValue(depId);
            bool met = false;
            for (const QJsonValue& v : vals)
                if (v.toString() == actual) { met = true; break; }
            if (!met) continue;
        }

        anyPrompted = true;
        QString question = p["question"].toString();
        QString type     = p["type"].toString();

        if (type == "select") {
            QStringList labels, values;
            for (const QJsonValue& optVal : p["options"].toArray()) {
                QJsonObject opt = optVal.toObject();
                labels.append(opt["label"].toString());
                values.append(opt["value"].toString());
            }
            bool ok = false;
            QString selectedLabel = QInputDialog::getItem(this,
                tr("Contest Information"), question, labels, 0, false, &ok);
            if (!ok) return;  // User cancelled — leave rest of prompts unset
            int idx = labels.indexOf(selectedLabel);
            if (idx >= 0 && idx < values.size())
                m_contestEngine->setUserPromptValue(promptId, values[idx]);

        } else if (type == "text") {
            bool forceUppercase = p.value("forceUppercase").toBool(true);
            QString value;
            while (value.isEmpty()) {
                QDialog dlg(this);
                dlg.setWindowTitle(tr("Contest Information"));
                QVBoxLayout layout(&dlg);
                QLabel label(question, &dlg);
                QLineEdit edit(&dlg);
                QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                if (auto *btn = buttons.button(QDialogButtonBox::Ok))
                    btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
                if (auto *btn = buttons.button(QDialogButtonBox::Cancel))
                    btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
                layout.addWidget(&label);
                layout.addWidget(&edit);
                layout.addWidget(&buttons);
                connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                if (forceUppercase) {
                    connect(&edit, &QLineEdit::textChanged, &edit, [&edit](const QString& text) {
                        QString upper = text.toUpper();
                        if (upper != text) {
                            int pos = edit.cursorPosition();
                            edit.setText(upper);
                            edit.setCursorPosition(pos);
                        }
                    });
                }
                if (dlg.exec() != QDialog::Accepted) return;
                value = edit.text().trimmed();
                if (value.isEmpty())
                    QMessageBox::warning(this, tr("Input Required"), question + tr(" cannot be empty."));
            }
            m_contestEngine->setUserPromptValue(promptId, forceUppercase ? value.toUpper() : value);
        }

        DebugLogger::instance().log("MainWindow",
            QString("promptForMissingUserPrompts: '%1' = '%2'")
                .arg(promptId, m_contestEngine->getUserPromptValue(promptId)));
    }

    if (anyPrompted) {
        applyRestrictedModeFromUserPrompts();
        // Refresh multiplier widget — effective list may depend on prompts (e.g. stationType)
        if (m_multiplierWidget && m_contestDefinition.contains("ui")) {
            if (m_contestDefinition["ui"].toObject()["showMultiplierPanel"].toBool(false))
                m_multiplierWidget->setMultiplierList(m_contestEngine->getEffectiveNamedMultiplierList());
        }
    }
}

void MainWindow::createConnections()
{
    // Install app-level event filter so F-keys work even when entry dock is floating
    qApp->installEventFilter(this);

    // Wire Radio L entry panel connections
    wireEntryPanelConnections(m_entryWidgets, false);
    
    m_rateWidget->setModel(m_qsoModel);

    connect(m_qsoModel, &QsoListModel::qsoAdded, this, [this]() {
        m_qsoCountLabel->setText(QString("QSOs: %1").arg(m_qsoModel->count()));
        m_isModified = true;
        updateWindowTitle();
        // Auto-scroll to the bottom when a new QSO is added
        if (m_qsoTable) {
            m_qsoTable->scrollToBottom();
        }
    });
    
    // Rig connections are set up after m_rigClient creation in the constructor

    // CW Console WPM changes
    connect(m_cwConsole, &CWWindow::wpmChanged, this, [this](int wpm) {
        m_lastWpm = wpm;
        m_wpmLabel->setText(QString("WPM: %1").arg(wpm));
    });
    
    // Wire SCP to call entry field (m_scpWidget now exists)
    ScpLineEdit *scpLineEdit = qobject_cast<ScpLineEdit*>(m_callEdit);
    if (scpLineEdit && m_scpWidget) {
        scpLineEdit->setScpWidget(m_scpWidget);
        bool scpEnabled = Settings::instance().getScpEnabled();
        scpLineEdit->setScpEnabled(scpEnabled);
        DebugLogger::instance().log("MainWindow", "SCP wired to call entry field");
        
        // Load SCP database if it exists and is enabled
        if (scpEnabled) {
            QString scpFilePath = SuperCheckPartial::instance().getDataFilePath();
            QFile scpFile(scpFilePath);
            if (scpFile.exists()) {
                if (SuperCheckPartial::instance().loadDatabase(scpFilePath)) {
                    DebugLogger::instance().log("MainWindow", 
                        QString("SCP database loaded from %1").arg(scpFilePath));
                } else {
                    DebugLogger::instance().log("MainWindow", 
                        QString("Failed to load SCP database from %1").arg(scpFilePath));
                }
            } else {
                DebugLogger::instance().log("MainWindow", 
                    QString("SCP database file not found at %1").arg(scpFilePath));
            }
        }
    }
    
    // SCP Widget state changes
    if (m_scpWidget) {
        connect(m_scpWidget, &QDockWidget::topLevelChanged, this, [this](bool floating) {
            Q_UNUSED(floating);
            updateScpWidgetMenuText();
        });
        connect(m_scpWidget, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (m_scpWidgetAction)
                m_scpWidgetAction->setChecked(visible);
            updateScpWidgetMenuText();
        });
    }
    
    // Dupe flash timer
    connect(m_dupeFlashTimer, &QTimer::timeout, this, &MainWindow::onDupeFlashTimeout);

    // Dock state save timer - debounces frequent resize/move events
    m_dockStateSaveTimer->setSingleShot(true);
    m_dockStateSaveTimer->setInterval(500);  // 500ms delay after last change
    connect(m_dockStateSaveTimer, &QTimer::timeout, this, &MainWindow::savePanelState);

    // QRZCQ API connections
    connect(m_qrzcqApi, &QrzcqApi::sessionObtained, this, &MainWindow::onQrzcqSessionObtained);
    connect(m_qrzcqApi, &QrzcqApi::sessionError, this, &MainWindow::onQrzcqSessionError);
    connect(m_qrzcqApi, &QrzcqApi::callsignFound, this, &MainWindow::onQrzcqCallsignFound);
    connect(m_qrzcqApi, &QrzcqApi::callsignNotFound, this, &MainWindow::onQrzcqCallsignNotFound);
    connect(m_qrzcqApi, &QrzcqApi::lookupError, this, &MainWindow::onQrzcqLookupError);

    // QRZ API connections
    connect(m_qrzApi, &QrzApi::sessionObtained, this, &MainWindow::onQrzSessionObtained);
    connect(m_qrzApi, &QrzApi::sessionError, this, &MainWindow::onQrzSessionError);
    connect(m_qrzApi, &QrzApi::callsignFound, this, &MainWindow::onQrzCallsignFound);
    connect(m_qrzApi, &QrzApi::callsignNotFound, this, &MainWindow::onQrzCallsignNotFound);
    connect(m_qrzApi, &QrzApi::lookupError, this, &MainWindow::onQrzLookupError);

    // TTS Manager connections
    connect(m_ttsManager, &TtsManager::finished, this, &MainWindow::onTtsFinished);
    connect(m_ttsManager, &TtsManager::error, this, &MainWindow::onTtsError);
    m_ttsManager->setRigClient(m_rigClient);

    // Initialize the configured callsign lookup API
    initCallsignLookup();

    // Sync Run/S&P button visual state with default mode
    updateRunSPButtons();
}

void MainWindow::onNewLog()
{
    if (!maybeSave())
        return;
    // Disable online scoring when switching logs
    if (m_onlineScoringAction && m_onlineScoringAction->isChecked())
        m_onlineScoringAction->setChecked(false);
    resetBackupState();
    
    // Show contest selection dialog
    ContestSelectDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString selectedFile = dialog.selectedContestFile();
        if (!selectedFile.isEmpty()) {
            if (dialog.isOpeningExisting()) {
                // User selected "Open Existing" - load the .clx file using threaded approach
                // Create progress dialog
                QProgressDialog *progressDialog = new QProgressDialog("Loading file...", QString(), 0, 0, this);
                progressDialog->setWindowTitle("Loading Log");
                progressDialog->setWindowModality(Qt::WindowModal);
                progressDialog->setAutoClose(false);
                progressDialog->setAutoReset(false);
                progressDialog->show();
                QApplication::processEvents();
                
                // Create worker and thread
                QThread *loadThread = new QThread(this);
                LoadingWorker *worker = new LoadingWorker(selectedFile, nullptr);
                worker->moveToThread(loadThread);
                
                // Connect signals
                connect(loadThread, &QThread::started, worker, &LoadingWorker::doLoad);
                
                connect(worker, &LoadingWorker::loadingComplete, this, [this, selectedFile, progressDialog, loadThread, worker](QList<QsoRecord> loadedQsos, bool success, QString errorMessage) {
                    loadThread->quit();
                    loadThread->wait();
                    worker->deleteLater();
                    loadThread->deleteLater();
                    
                    if (!success) {
                        progressDialog->close();
                        progressDialog->deleteLater();
                        QMessageBox::warning(this, "Load Failed", 
                            "Failed to load file:\n\n" + errorMessage);
                        return;
                    }
                    
                    // Extract contest file and station class from the loaded QSOs
                    QString contestFile;
                    QString stationClass;
                    QString loadedContestVersion;
                    QString stationClassExchangeName;
                    QString stationClassExchangeId;
                    FileHandler fileHandler;
                    
                    // We need to parse the file to get contest info - for now we'll load it separately
                    QList<QsoRecord> temp;
                    fileHandler.loadClxWithContest(selectedFile, temp, contestFile, stationClass, loadedContestVersion, stationClassExchangeName, stationClassExchangeId);

                    // Load contest-specific memories from the file
                    ClxFile clxFileForMem;
                    if (clxFileForMem.load(selectedFile)) {
                        if (clxFileForMem.useContestMemories()) {
                            m_useContestMemories = true;
                            m_contestCwMemories = clxFileForMem.cwMemories();
                            m_contestSsbMemories = clxFileForMem.ssbMemories();
                        } else {
                            m_useContestMemories = false;
                            m_contestCwMemories.clear();
                            m_contestSsbMemories.clear();
                        }
                        loadCWMemories();
                        loadSsbMemories();
                    }

                    // Load the contest definition if specified
                    if (!contestFile.isEmpty()) {
                        QString contestPath = Settings::getContestsPath() + "/" + contestFile;
                        if (QFile::exists(contestPath)) {
                            // Set station class and exchange data BEFORE loading contest definition
                            if (!stationClass.isEmpty()) {
                                m_contestEngine->setStationClass(stationClass);
                                DebugLogger::instance().log("MainWindow", QString("(onNewLog) Set station class: %1").arg(stationClass));
                            }
                            if (!stationClassExchangeName.isEmpty() || !stationClassExchangeId.isEmpty()) {
                                m_contestEngine->setStationClassExchangeName(stationClassExchangeName);
                                m_contestEngine->setStationClassExchangeId(stationClassExchangeId);
                                DebugLogger::instance().log("MainWindow", QString("(onNewLog) Set station class exchange - Name: %1, ID: %2").arg(stationClassExchangeName, stationClassExchangeId));
                            }
                            
                            loadContestDefinition(contestPath);
                            
                            // Check if contest version has changed
                            if (!loadedContestVersion.isEmpty() && !m_contestDefinition.isEmpty()) {
                                QString currentVersion = m_contestDefinition["contest"].toObject()["version"].toString();
                                DebugLogger::instance().log("MainWindow", 
                                    QString("Contest version check: Log file v%1, Current definition v%2").arg(loadedContestVersion, currentVersion));
                                if (!currentVersion.isEmpty() && !isSemanticVersionEqual(currentVersion, loadedContestVersion)) {
                                    DebugLogger::instance().log("MainWindow", "Version mismatch detected - showing user warning");
                                    progressDialog->close();
                                    QMessageBox::StandardButton reply = QMessageBox::warning(this, "Contest Version Mismatch",
                                        "The log file was created with contest version " + loadedContestVersion + 
                                        " but the current definition is version " + currentVersion + ".\n\n" +
                                        "Loading with a different version may cause scoring issues.\n\n" +
                                        "Do you want to continue?",
                                        QMessageBox::Yes | QMessageBox::No);
                                    if (reply == QMessageBox::No) {
                                        progressDialog->deleteLater();
                                        return;
                                    }
                                    progressDialog->show();
                                    QApplication::processEvents();
                                }
                            }
                        }
                    }
                    
                    // Batch-insert all QSOs in one model reset (avoids N individual signals/repaints)
                    loadQsosIntoModel(loadedQsos, progressDialog);
                    
                    m_currentFile = selectedFile;
                    m_isModified = false;
                    updateWindowTitle();
                    updateQsoEntryFields();
                    
                    progressDialog->close();
                    progressDialog->deleteLater();
                    
                    // Auto-recalculate score to validate and mark dupes/out-of-band
                    onRecalculateScore();
                    checkForCrashBackups();

                    m_statusLabel->setText(QString("Loaded %1 QSOs").arg(loadedQsos.size()));
                });
                
                loadThread->start();
            } else {
                // User selected a contest definition - create new log
                // Clear the QSO model first so old data isn't visible
                m_qsoModel->clear();
                m_currentFile.clear();

                if (m_scoreWidget)
                    m_scoreWidget->resetScore();

                // Pass false to NOT restore the previous station class
                loadContestDefinition(selectedFile, false);
                
                // Prompt for station class if the contest requires it
                if (m_contestEngine && m_contestEngine->needsStationClass()) {
                    QStringList classOptions = m_contestEngine->getStationClassOptions();
                    QString selectedClass;

                    // Auto-select if only one class available
                    if (classOptions.size() == 1) {
                        selectedClass = classOptions.first().split('|').first();
                        DebugLogger::instance().log("MainWindow",
                            QString("Auto-selected single station class: %1").arg(selectedClass));
                    } else {
                        StationClassDialog classDialog(
                            m_contestEngine->getStationClassPrompt(),
                            classOptions,
                            this);
                        if (classDialog.exec() == QDialog::Accepted) {
                            selectedClass = classDialog.getSelectedClass();
                        }
                    }

                    if (!selectedClass.isEmpty()) {
                        m_contestEngine->setStationClass(selectedClass);
                        DebugLogger::instance().log("MainWindow",
                            QString("Station class selected: %1").arg(selectedClass));

                        // Prompt for operator callsign if contest requests it
                        if (m_contestEngine->stationClassPromptsForCallsign()) {
                            QString defaultCall = Settings::instance().getCallsign();
                            QDialog callDialog(this);
                            callDialog.setWindowTitle("Operator Callsign");
                            QVBoxLayout callLayout(&callDialog);
                            QLabel callLabel("Enter operator callsign:");
                            QLineEdit callEdit;
                            callEdit.setText(defaultCall);
                            callEdit.selectAll();
                            QDialogButtonBox callButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                            if (auto *btn = callButtons.button(QDialogButtonBox::Ok))
                                btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
                            if (auto *btn = callButtons.button(QDialogButtonBox::Cancel))
                                btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
                            callLayout.addWidget(&callLabel);
                            callLayout.addWidget(&callEdit);
                            callLayout.addWidget(&callButtons);
                            connect(&callButtons, &QDialogButtonBox::accepted, &callDialog, &QDialog::accept);
                            connect(&callButtons, &QDialogButtonBox::rejected, &callDialog, &QDialog::reject);
                            connect(&callEdit, &QLineEdit::textChanged, [&callEdit](const QString& text) {
                                if (text != text.toUpper()) {
                                    int pos = callEdit.cursorPosition();
                                    callEdit.blockSignals(true);
                                    callEdit.setText(text.toUpper());
                                    callEdit.setCursorPosition(pos);
                                    callEdit.blockSignals(false);
                                }
                            });
                            if (callDialog.exec() == QDialog::Accepted && !callEdit.text().trimmed().isEmpty()) {
                                m_sessionStationInfo->setCallsign(callEdit.text().trimmed().toUpper());
                                DebugLogger::instance().log("MainWindow",
                                    QString("Operator callsign set to: %1").arg(m_sessionStationInfo->callsign()));
                            } else {
                                m_contestEngine->resetStationClassState();
                                return;
                            }
                        }

                        // Check if this class needs additional input
                        if (m_contestEngine->stationClassNeedsInput() && m_contestEngine->getStationClassExchangeData().isEmpty()) {
                            // Prompt for name and ID separately
                            QString namePrompt = m_contestEngine->getStationClassNamePrompt();
                            QString idPrompt = m_contestEngine->getStationClassIdPrompt();
                            QJsonObject inputValidation = m_contestEngine->getStationClassInputValidation();

                            // Prompt for name with real-time uppercase conversion
                            QString name;
                            while (true) {
                                QDialog nameDialog(this);
                                nameDialog.setWindowTitle("Station Information");
                                QVBoxLayout layout(&nameDialog);
                                
                                QLabel label(namePrompt.isEmpty() ? "Enter your first name:" : namePrompt);
                                QLineEdit nameEdit;
                                QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                                if (auto *btn = buttonBox.button(QDialogButtonBox::Ok))
                                    btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
                                if (auto *btn = buttonBox.button(QDialogButtonBox::Cancel))
                                    btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
                                layout.addWidget(&label);
                                layout.addWidget(&nameEdit);
                                layout.addWidget(&buttonBox);
                                
                                connect(&buttonBox, &QDialogButtonBox::accepted, &nameDialog, &QDialog::accept);
                                connect(&buttonBox, &QDialogButtonBox::rejected, &nameDialog, &QDialog::reject);
                                
                                // Apply real-time uppercase conversion
                                QJsonObject nameValidation = inputValidation.value("name").toObject();
                                bool forceUppercase = nameValidation.contains("forceUppercase") ? nameValidation["forceUppercase"].toBool() : true;
                                
                                connect(&nameEdit, &QLineEdit::textChanged, [&nameEdit, forceUppercase](const QString& text) {
                                    QString filtered;
                                    for (const QChar& c : text) {
                                        if (c.isLetter()) {
                                            filtered += forceUppercase ? c.toUpper() : c;
                                        }
                                    }
                                    if (filtered != text) {
                                        int cursorPos = nameEdit.cursorPosition();
                                        nameEdit.blockSignals(true);
                                        nameEdit.setText(filtered);
                                        nameEdit.setCursorPosition(cursorPos);
                                        nameEdit.blockSignals(false);
                                    }
                                });
                                
                                if (nameDialog.exec() == QDialog::Accepted) {
                                    name = nameEdit.text();
                                    if (name.isEmpty()) {
                                        QMessageBox::warning(this, "Input Required", "Name cannot be empty");
                                        continue;
                                    }
                                    break;
                                } else {
                                    DebugLogger::instance().log("MainWindow", "Station class name input cancelled");
                                    m_contestEngine->resetStationClassState();
                                    return;
                                }
                            }
                            
                            // Prompt for ID with real-time validation
                            QString id;
                            while (true) {
                                QDialog idDialog(this);
                                idDialog.setWindowTitle("Station Information");
                                QVBoxLayout layout(&idDialog);
                                
                                QLabel label(idPrompt.isEmpty() ? "Enter ID or location:" : idPrompt);
                                QLineEdit idEdit;
                                QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                                if (auto *btn = buttonBox.button(QDialogButtonBox::Ok))
                                    btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
                                if (auto *btn = buttonBox.button(QDialogButtonBox::Cancel))
                                    btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
                                layout.addWidget(&label);
                                layout.addWidget(&idEdit);
                                layout.addWidget(&buttonBox);
                                
                                connect(&buttonBox, &QDialogButtonBox::accepted, &idDialog, &QDialog::accept);
                                connect(&buttonBox, &QDialogButtonBox::rejected, &idDialog, &QDialog::reject);
                                
                                // Apply validation rules for ID with real-time filtering
                                QJsonObject idValidation = inputValidation.value("id").toObject();
                                QString idType = idValidation.value("type").toString("alphanumeric");
                                bool idForceUppercase = idValidation.contains("forceUppercase") ? idValidation["forceUppercase"].toBool() : false;
                                QString defaultValue = idValidation.value("defaultValue").toString();
                                
                                // Pre-fill with default value if it exists
                                if (!defaultValue.isEmpty()) {
                                    idEdit.setText(defaultValue.toUpper());
                                }
                                
                                connect(&idEdit, &QLineEdit::textChanged, [&idEdit, idType, idForceUppercase, defaultValue](const QString& text) {
                                    QString filtered;
                                    for (const QChar& c : text) {
                                        bool charValid = false;
                                        QChar charToAdd = c;
                                        
                                        if (idType == "numeric") {
                                            if (c.isDigit()) {
                                                charValid = true;
                                            }
                                        } else if (idType == "alphanumeric") {
                                            if (c.isLetterOrNumber()) {
                                                charValid = true;
                                                // Apply uppercase conversion if needed, or always uppercase for fixed exchanges like CWA
                                                if ((idForceUppercase || !defaultValue.isEmpty()) && c.isLetter()) {
                                                    charToAdd = c.toUpper();
                                                }
                                            }
                                        }
                                        
                                        if (charValid) {
                                            filtered += charToAdd;
                                        }
                                    }
                                    if (filtered != text) {
                                        int cursorPos = idEdit.cursorPosition();
                                        idEdit.blockSignals(true);
                                        idEdit.setText(filtered);
                                        idEdit.setCursorPosition(cursorPos);
                                        idEdit.blockSignals(false);
                                    }
                                });
                                
                                if (idDialog.exec() == QDialog::Accepted) {
                                    id = idEdit.text();
                                    if (id.isEmpty()) {
                                        QMessageBox::warning(this, "Input Required", "ID cannot be empty");
                                        continue;
                                    }
                                    break;
                                } else {
                                    DebugLogger::instance().log("MainWindow", "Station class ID input cancelled");
                                    m_contestEngine->resetStationClassState();
                                    return;
                                }
                            }
                            
                            m_contestEngine->setStationClassExchangeName(name);
                            m_contestEngine->setStationClassExchangeId(id);
                            DebugLogger::instance().log("MainWindow", 
                                QString("Station class exchange data set - Name: %1, ID: %2").arg(name, id));
                        }
                    } else {
                        // User cancelled station class selection
                        m_contestEngine->resetStationClassState();
                        return;
                    }
                }
                
                // Prompt for user prompts (like grid square, month, etc.)
                if (m_contestEngine) {
                    QJsonObject contestDef = m_contestEngine->getContestDefinition();
                    if (contestDef.contains("userPrompts")) {
                        QJsonArray prompts = contestDef["userPrompts"].toArray();
                        for (const QJsonValue& promptVal : prompts) {
                            QJsonObject promptObj = promptVal.toObject();
                            QString promptId = promptObj["id"].toString();
                            QString question = promptObj["question"].toString();
                            QString type = promptObj["type"].toString();
                            bool required = promptObj["required"].toBool(false);

                            // Skip prompts with unmet visibleWhen conditions
                            if (promptObj.contains("visibleWhen")) {
                                QJsonObject vw = promptObj["visibleWhen"].toObject();
                                QString depId = vw["promptId"].toString();
                                QJsonArray vals = vw["values"].toArray();
                                QString actual = m_contestEngine->getUserPromptValue(depId);
                                bool met = false;
                                for (const QJsonValue& v : vals)
                                    if (v.toString() == actual) { met = true; break; }
                                if (!met) continue;
                            }

                            if (type == "text") {
                                QString value;
                                bool forceUppercase = promptObj.value("forceUppercase").toBool(true);
                                while (value.isEmpty() && required) {
                                    QDialog inputDialog(this);
                                    inputDialog.setWindowTitle("Contest Information");
                                    QVBoxLayout layout(&inputDialog);
                                    
                                    QLabel label(question);
                                    QLineEdit edit;
                                    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                                    if (auto *btn = buttonBox.button(QDialogButtonBox::Ok))
                                        btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
                                    if (auto *btn = buttonBox.button(QDialogButtonBox::Cancel))
                                        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
                                    layout.addWidget(&label);
                                    layout.addWidget(&edit);
                                    layout.addWidget(&buttonBox);
                                    
                                    connect(&buttonBox, &QDialogButtonBox::accepted, &inputDialog, &QDialog::accept);
                                    connect(&buttonBox, &QDialogButtonBox::rejected, &inputDialog, &QDialog::reject);
                                    
                                    // Apply real-time uppercase conversion
                                    connect(&edit, &QLineEdit::textChanged, [&edit, forceUppercase](const QString& text) {
                                        QString filtered;
                                        for (const QChar& c : text) {
                                            if (c.isLetter() || c.isDigit() || c == '-' || c == '/') {
                                                filtered += forceUppercase && c.isLetter() ? c.toUpper() : c;
                                            }
                                        }
                                        if (filtered != text) {
                                            int cursorPos = edit.cursorPosition();
                                            edit.blockSignals(true);
                                            edit.setText(filtered);
                                            edit.setCursorPosition(cursorPos);
                                            edit.blockSignals(false);
                                        }
                                    });
                                    
                                    if (inputDialog.exec() == QDialog::Accepted) {
                                        value = edit.text();
                                        if (value.isEmpty() && required) {
                                            QMessageBox::warning(this, "Input Required", 
                                                question + " cannot be empty");
                                            continue;
                                        }
                                    } else {
                                        // User cancelled
                                        m_contestEngine->resetStationClassState();
                                        return;
                                    }
                                }
                                if (!value.isEmpty()) {
                                    m_contestEngine->setUserPromptValue(promptId, value);
                                    DebugLogger::instance().log("MainWindow", 
                                        QString("User prompt '%1' set to: '%2'").arg(promptId, value));
                                }
                            } else if (type == "select") {
                                QStringList labels;
                                QStringList values;
                                if (promptObj.contains("options")) {
                                    QJsonArray options = promptObj["options"].toArray();
                                    for (const QJsonValue& optVal : options) {
                                        QJsonObject optObj = optVal.toObject();
                                        labels.append(optObj["label"].toString());
                                        values.append(optObj["value"].toString());
                                    }
                                }
                                
                                bool ok;
                                QString selectedLabel = QInputDialog::getItem(this,
                                    "Contest Information", question,
                                    labels, 0, false, &ok);
                                if (ok) {
                                    int selectedIndex = labels.indexOf(selectedLabel);
                                    if (selectedIndex >= 0 && selectedIndex < values.size()) {
                                        QString selectedValue = values[selectedIndex];
                                        m_contestEngine->setUserPromptValue(promptId, selectedValue);
                                        DebugLogger::instance().log("MainWindow",
                                            QString("User prompt '%1' set to: '%2'").arg(promptId, selectedValue));
                                    }
                                } else if (!ok) {
                                    // User cancelled
                                    m_contestEngine->resetStationClassState();
                                    return;
                                }
                            } else if (type == "checkboxes") {
                                // For checkboxes, collect selected options
                                QStringList selectedOptions;
                                if (promptObj.contains("options")) {
                                    QJsonArray options = promptObj["options"].toArray();
                                    
                                    // Create a dialog with checkboxes
                                    QDialog checkboxDialog(this);
                                    checkboxDialog.setWindowTitle("Contest Information");
                                    checkboxDialog.setMinimumWidth(500);
                                    
                                    QVBoxLayout dialogLayout(&checkboxDialog);
                                    
                                    QLabel questionLabel(question);
                                    dialogLayout.addWidget(&questionLabel);
                                    dialogLayout.addSpacing(10);
                                    
                                    QMap<QString, QCheckBox*> checkboxes;
                                    
                                    for (const QJsonValue& optVal : options) {
                                        QJsonObject optObj = optVal.toObject();
                                        QString optValue = optObj["value"].toString();
                                        QString optLabel = optObj["label"].toString();
                                        
                                        QCheckBox* checkbox = new QCheckBox(optLabel, &checkboxDialog);
                                        checkboxes[optValue] = checkbox;
                                        dialogLayout.addWidget(checkbox);
                                    }
                                    
                                    dialogLayout.addSpacing(10);
                                    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                                    if (auto *btn = buttonBox.button(QDialogButtonBox::Ok))
                                        btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
                                    if (auto *btn = buttonBox.button(QDialogButtonBox::Cancel))
                                        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
                                    dialogLayout.addWidget(&buttonBox);

                                    connect(&buttonBox, &QDialogButtonBox::accepted, &checkboxDialog, &QDialog::accept);
                                    connect(&buttonBox, &QDialogButtonBox::rejected, &checkboxDialog, &QDialog::reject);
                                    
                                    if (checkboxDialog.exec() == QDialog::Accepted) {
                                        // Collect selected checkboxes
                                        for (auto it = checkboxes.begin(); it != checkboxes.end(); ++it) {
                                            if (it.value()->isChecked()) {
                                                selectedOptions.append(it.key());
                                            }
                                        }
                                        
                                        // Convert to JSON array for storage
                                        QJsonArray selectedArray;
                                        for (const QString& opt : selectedOptions) {
                                            selectedArray.append(opt);
                                        }
                                        
                                        // Store as JSON string
                                        QJsonDocument doc(selectedArray);
                                        QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
                                        m_contestEngine->setUserPromptValue(promptId, jsonString);
                                        
                                        DebugLogger::instance().log("MainWindow", 
                                            QString("User prompt '%1' (checkboxes) set to: %2 options").arg(promptId).arg(selectedOptions.size()));
                                    } else {
                                        // User cancelled
                                        m_contestEngine->resetStationClassState();
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Apply mode restriction if any userPrompt has "restrictMode": true
                applyRestrictedModeFromUserPrompts();

                // Refresh multiplier widget now that user prompts are set
                if (m_multiplierWidget && m_contestEngine) {
                    QJsonObject ui = m_contestDefinition["ui"].toObject();
                    if (ui["showMultiplierPanel"].toBool(false)) {
                        DebugLogger::instance().log("MultiplierWidget",
                            QString("onNewLog post-prompt refresh: stationType='%1', effective mults=%2")
                                .arg(m_contestEngine->getUserPromptValue("stationType"))
                                .arg(m_contestEngine->getEffectiveNamedMultiplierList().size()));
                        m_multiplierWidget->setMultiplierList(m_contestEngine->getEffectiveNamedMultiplierList());
                    }
                }

                m_isModified = false;
                updateWindowTitle();
                clearEntryForm();
                m_statusLabel->setText("New log created");
                checkForCrashBackups();
            }
        }
    }
}

void MainWindow::onOpenLog()
{
    if (!maybeSave())
        return;
    resetBackupState();
    
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open Log File", "", 
        "All Supported (*.clx *.csv *.adi *.adif);;"
        "ContestLogX 2.0 Format (*.clx);;"
        "ADIF Files (*.adi *.adif);;"
        "CSV Files (*.csv);;"
        "All Files (*)");
    
    if (fileName.isEmpty())
        return;
    
    // Create progress dialog
    QProgressDialog *progressDialog = new QProgressDialog("Loading file...", QString(), 0, 0, this);
    progressDialog->setWindowTitle("Loading Log");
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);
    progressDialog->show();
    QApplication::processEvents();
    
    // Create worker and thread
    QThread *loadThread = new QThread(this);
    LoadingWorker *worker = new LoadingWorker(fileName, nullptr);
    worker->moveToThread(loadThread);
    
    // Connect signals
    connect(loadThread, &QThread::started, worker, &LoadingWorker::doLoad);
    
    connect(worker, &LoadingWorker::loadingComplete, this, [this, fileName, progressDialog, loadThread, worker](QList<QsoRecord> loadedQsos, bool success, QString errorMessage) {
        loadThread->quit();
        loadThread->wait();
        worker->deleteLater();
        loadThread->deleteLater();
        
        if (!success) {
            progressDialog->close();
            progressDialog->deleteLater();
            QMessageBox::warning(this, "Load Failed", 
                "Failed to load file:\n\n" + errorMessage);
            return;
        }
        
        // For .clx files, check and load contest info
        if (fileName.endsWith(".clx", Qt::CaseInsensitive)) {
            QString contestFile;
            QString stationClass;
            QString loadedContestVersion;
            QString stationClassExchangeName;
            QString stationClassExchangeId;
            QMap<QString, QString> userPromptValues;
            FileHandler fileHandler;

            QList<QsoRecord> temp;
            fileHandler.loadClxWithContest(fileName, temp, contestFile, stationClass, loadedContestVersion, stationClassExchangeName, stationClassExchangeId, userPromptValues);
            
            // Also load the station info from the CLX file (for session use only - don't persist to Settings)
            ClxFile clxFile;
            QString loadedMode;
            if (clxFile.load(fileName)) {
                // Update session station info from loaded file (NOT Settings)
                *m_sessionStationInfo = clxFile.station();

                QString loadedCallsign = clxFile.station().callsign();
                if (!loadedCallsign.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded callsign from CLX (session only): %1").arg(loadedCallsign));
                }
                // Also log operator name and state if available
                QString operatorName = clxFile.station().operatorName();
                QString operatorState = clxFile.station().state();
                if (!operatorName.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator name from CLX (session only): %1").arg(operatorName));
                }
                if (!operatorState.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator state from CLX (session only): %1").arg(operatorState));
                }
                // Also load the contest mode
                loadedMode = clxFile.contest().mode();
                if (!loadedMode.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded contest mode from CLX: %1").arg(loadedMode));
                }

                // Load contest-specific memories
                if (clxFile.useContestMemories()) {
                    m_useContestMemories = true;
                    m_contestCwMemories = clxFile.cwMemories();
                    m_contestSsbMemories = clxFile.ssbMemories();
                    DebugLogger::instance().log("MainWindow", QString("Loaded contest-specific memories from CLX: %1 CW, %2 SSB")
                        .arg(m_contestCwMemories.size()).arg(m_contestSsbMemories.size()));
                } else {
                    m_useContestMemories = false;
                    m_contestCwMemories.clear();
                    m_contestSsbMemories.clear();
                }
                loadCWMemories();
                loadSsbMemories();
            }

            // Load the contest definition if specified
            if (!contestFile.isEmpty()) {
                QString contestPath = Settings::getContestsPath() + "/" + contestFile;
                if (QFile::exists(contestPath)) {
                    // Set station class BEFORE loading contest definition to prevent dialog
                    if (!stationClass.isEmpty()) {
                        if (!m_contestEngine) {
                            m_contestEngine = new ContestEngine(this);
                            m_contestEngine->setDxccDatabase(m_dxccDatabase);
                        }
                        m_contestEngine->setStationClass(stationClass);
                        DebugLogger::instance().log("MainWindow", QString("Pre-set station class from CLX: %1").arg(stationClass));
                    }
                    
                    if (!stationClassExchangeName.isEmpty() || !stationClassExchangeId.isEmpty()) {
                        if (!m_contestEngine) {
                            m_contestEngine = new ContestEngine(this);
                            m_contestEngine->setDxccDatabase(m_dxccDatabase);
                        }
                        m_contestEngine->setStationClassExchangeName(stationClassExchangeName);
                        m_contestEngine->setStationClassExchangeId(stationClassExchangeId);
                        DebugLogger::instance().log("MainWindow", QString("Pre-set station class exchange from CLX - Name:'%1' Id:'%2'").arg(stationClassExchangeName, stationClassExchangeId));
                    }
                    
                    // Pass false to NOT restore/prompt for station class since we already have it
                    loadContestDefinition(contestPath, false);

                    // Restore userPromptValues after contest is loaded
                    if (!userPromptValues.isEmpty() && m_contestEngine) {
                        for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it) {
                            m_contestEngine->setUserPromptValue(it.key(), it.value());
                        }
                        DebugLogger::instance().log("MainWindow", QString("Restored %1 user prompt values from CLX").arg(userPromptValues.size()));
                        // Apply mode restriction if any userPrompt has "restrictMode": true
                        applyRestrictedModeFromUserPrompts();
                    }

                    // Prompt for any required userPrompts not present in the loaded file
                    promptForMissingUserPrompts();

                    // If we loaded a mode from the CLX file, restrict to that mode
                    if (!loadedMode.isEmpty()) {
                        m_contestEngine->setRestrictedMode(loadedMode);
                    }

                    // Check if contest version has changed
                    if (!loadedContestVersion.isEmpty() && !m_contestDefinition.isEmpty()) {
                        QString currentVersion = m_contestDefinition["contest"].toObject()["version"].toString();
                        DebugLogger::instance().log("MainWindow",
                            QString("Contest version check: Log file v%1, Current definition v%2").arg(loadedContestVersion, currentVersion));
                        if (!currentVersion.isEmpty() && !isSemanticVersionEqual(currentVersion, loadedContestVersion)) {
                            DebugLogger::instance().log("MainWindow", "Version mismatch detected - showing user warning");
                            progressDialog->close();
                            QMessageBox::StandardButton reply = QMessageBox::warning(this, "Contest Version Mismatch",
                                "The log file was created with contest version " + loadedContestVersion + 
                                " but the current definition is version " + currentVersion + ".\n\n" +
                                "Loading with a different version may cause scoring issues.\n\n" +
                                "Do you want to continue?",
                                QMessageBox::Yes | QMessageBox::No);
                            if (reply == QMessageBox::No) {
                                progressDialog->deleteLater();
                                return;
                            }
                            progressDialog->show();
                            QApplication::processEvents();
                        }
                    }
                }
            }
        }
        
        // If no contest is loaded and this is an ADIF file, prompt for contest selection
        if (m_contestDefinition.isEmpty() && (fileName.endsWith(".adi", Qt::CaseInsensitive) || fileName.endsWith(".adif", Qt::CaseInsensitive))) {
            DebugLogger::instance().log("MainWindow", "ADIF file loaded without contest, prompting for selection");
            progressDialog->close();
            
            ContestSelectDialog contestDialog(this);
            if (contestDialog.exec() == QDialog::Rejected) {
                DebugLogger::instance().log("MainWindow", "User cancelled contest selection");
                progressDialog->deleteLater();
                return;
            }
            
            QString selectedContestFile = contestDialog.selectedContestFile();
            DebugLogger::instance().log("MainWindow", QString("User selected contest: %1").arg(selectedContestFile));
            
            if (!selectedContestFile.isEmpty()) {
                DebugLogger::instance().log("MainWindow", QString("Contest path exists: %1").arg(QFile::exists(selectedContestFile) ? "true" : "false"));
                
                if (QFile::exists(selectedContestFile)) {
                    DebugLogger::instance().log("MainWindow", "Loading contest definition...");
                    bool contestLoaded = loadContestDefinition(selectedContestFile);
                    DebugLogger::instance().log("MainWindow", QString("Contest loaded: %1").arg(contestLoaded ? "true" : "false"));
                    
                    if (!contestLoaded) {
                        DebugLogger::instance().log("MainWindow", "Contest definition load failed, aborting");
                        progressDialog->deleteLater();
                        return;
                    }
                }
            }
            
            progressDialog->show();
            QApplication::processEvents();
        }
        
        // Batch-insert all QSOs in one model reset (avoids N individual signals/repaints)
        loadQsosIntoModel(loadedQsos, progressDialog);

        m_currentFile = fileName;
        m_isModified = false;
        updateWindowTitle();
        
        progressDialog->close();
        progressDialog->deleteLater();
        
        // For ADIF files, ask if user wants to reverse QSO order
        if (fileName.endsWith(".adi", Qt::CaseInsensitive) || fileName.endsWith(".adif", Qt::CaseInsensitive)) {
            // Check if QSOs are in reverse chronological order
            if (m_qsoModel->rowCount() > 1) {
                QsoRecord firstQso = m_qsoModel->getQso(0);
                QsoRecord lastQso = m_qsoModel->getQso(m_qsoModel->rowCount() - 1);
                
                if (firstQso.getDateTime() > lastQso.getDateTime()) {
                    // First QSO is newer than last QSO - they're in reverse order
                    QMessageBox::StandardButton reply = QMessageBox::question(
                        this,
                        "Reverse QSO Order",
                        "The QSOs appear to be in newest-first order.\n\n"
                        "Do you want to reverse them to oldest-first order?",
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes
                    );
                    
                    if (reply == QMessageBox::Yes) {
                        m_qsoModel->reverseQsos();
                    }
                }
            }
        }
        
        // Auto-recalculate score on background thread (only if contest loaded)
        DebugLogger::instance().log("MainWindow", QString("After ADIF load, m_contestDefinition.isEmpty(): %1").arg(m_contestDefinition.isEmpty() ? "true" : "false"));
        if (!m_contestDefinition.isEmpty()) {
            DebugLogger::instance().log("MainWindow", "Creating ScoringWorker for ADIF load");
            
            // Get current QSOs from model
            QList<QsoRecord> currentQsos = m_qsoModel->getAllQsos();
            QString myCallsign = getSessionCallsign();
            
            // Create NEW progress dialog for scoring phase
            QProgressDialog *scoringProgressDialog = new QProgressDialog("Scoring QSOs...", QString(), 0, currentQsos.count(), this);
            scoringProgressDialog->setWindowTitle("Scoring Log");
            scoringProgressDialog->setWindowModality(Qt::WindowModal);
            scoringProgressDialog->setAutoClose(false);
            scoringProgressDialog->setAutoReset(false);
            scoringProgressDialog->show();
            QApplication::processEvents();
            
            // Create scoring worker and thread
            QThread *scoringThread = new QThread(this);
            ScoringWorker *scoringWorker = new ScoringWorker(currentQsos, m_contestEngine, myCallsign, nullptr);
            scoringWorker->moveToThread(scoringThread);
            
            connect(scoringThread, &QThread::started, scoringWorker, &ScoringWorker::doScore);
            
            connect(scoringWorker, &ScoringWorker::progressUpdated, this, [this, scoringProgressDialog](int current, int total) {
                scoringProgressDialog->setMaximum(total);
                scoringProgressDialog->setValue(current);
                QApplication::processEvents();
            });
            
            connect(scoringWorker, &ScoringWorker::scoringComplete, this, [this, fileName, scoringProgressDialog, scoringThread, scoringWorker](QList<QsoRecord> scoredQsos, bool success) {
                if (success) {
                    // Clear the old model and add scored QSOs
                    m_qsoModel->clear();
                    for (const auto& qso : scoredQsos) {
                        m_qsoModel->addQso(qso);
                    }
                    
                    // Update score display
                    if (m_scoreWidget) {
                        m_scoreWidget->resetScore();
                        auto score = m_contestEngine->getRunningScore();
                        m_scoreWidget->updateScore(score);
                    }
                    updateSnapshotScore();
                    updateSnapshotQsos();
                    // Update multiplier widget
                    if (m_multiplierWidget && m_multiplierDock && m_multiplierDock->isVisible()) {
                        QString multType = m_contestEngine->getMultiplierType();
                        if (multType == "multsOnce")
                            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMults());
                        else if (multType == "multsPerBand")
                            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBand());
                        else if (multType == "multsPerMode")
                            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerMode());
                        else if (multType == "multsPerBandAndMode")
                            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBandAndMode());
                    }
                } else {
                    QMessageBox::critical(this, "Error", "Failed to score QSOs");
                }
                
                scoringProgressDialog->close();
                scoringProgressDialog->deleteLater();
                scoringThread->quit();
                scoringThread->wait();
                scoringWorker->deleteLater();
                scoringThread->deleteLater();
                
                // Set focus to call field
                if (m_callEdit) {
                    m_callEdit->setFocus();
                }

                checkForCrashBackups();
                m_statusLabel->setText("File loaded: " + fileName + " (" +
                    QString::number(m_qsoModel->rowCount()) + " QSOs)");
            });

            scoringThread->start();
        } else {
            DebugLogger::instance().log("MainWindow", "Contest is still empty, not recalculating score");
            checkForCrashBackups();
            m_statusLabel->setText("File loaded: " + fileName + " (" +
                QString::number(loadedQsos.count()) + " QSOs)");
        }
    });

    loadThread->start();
}

void MainWindow::loadLogFile(const QString& filename)
{
    resetBackupState();
    if (filename.isEmpty() || !QFile::exists(filename)) {
        // Prevent duplicate dialogs
        if (m_showingLogFileNotFoundDialog) {
            return;
        }
        m_showingLogFileNotFoundDialog = true;
        
        DebugLogger::instance().log("MainWindow", QString("Log file not found: %1").arg(filename));
        QMessageBox::warning(this, "Log File Not Found", 
                           QString("The log file '%1' was not found.").arg(filename),
                           QMessageBox::Ok);
        
        m_showingLogFileNotFoundDialog = false;
        return;
    }
    
    // Skip the contest selection dialog and go straight to loading
    DebugLogger::instance().log("MainWindow", QString("Loading log file directly: %1").arg(filename));
    
    // Create progress dialog
    QProgressDialog *progressDialog = new QProgressDialog("Loading file...", QString(), 0, 0, this);
    progressDialog->setWindowTitle("Loading Log");
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);
    progressDialog->show();
    QApplication::processEvents();
    
    // Create worker and thread
    QThread *loadThread = new QThread(this);
    LoadingWorker *worker = new LoadingWorker(filename, nullptr);
    worker->moveToThread(loadThread);
    
    // Connect signals
    connect(loadThread, &QThread::started, worker, &LoadingWorker::doLoad);
    
    connect(worker, &LoadingWorker::loadingComplete, this, [this, filename, progressDialog, loadThread, worker](QList<QsoRecord> loadedQsos, bool success, QString errorMessage) {
        loadThread->quit();
        loadThread->wait();
        worker->deleteLater();
        loadThread->deleteLater();
        
        if (!success) {
            progressDialog->close();
            progressDialog->deleteLater();
            DebugLogger::instance().log("MainWindow", QString("Failed to load file: %1").arg(errorMessage));
            return;
        }
        
        // For .clx files, check and load contest info
        if (filename.endsWith(".clx", Qt::CaseInsensitive)) {
            QString contestFile;
            QString stationClass;
            QString loadedContestVersion;
            QString stationClassExchangeName;
            QString stationClassExchangeId;
            QMap<QString, QString> userPromptValues;
            FileHandler fileHandler;
            
            QList<QsoRecord> temp;
            fileHandler.loadClxWithContest(filename, temp, contestFile, stationClass, loadedContestVersion, stationClassExchangeName, stationClassExchangeId, userPromptValues);
            
            // Also load the station info from the CLX file (for session use only - don't persist to Settings)
            ClxFile clxFile;
            QString loadedMode;
            if (clxFile.load(filename)) {
                // Update session station info from loaded file (NOT Settings)
                *m_sessionStationInfo = clxFile.station();

                QString loadedCallsign = clxFile.station().callsign();
                if (!loadedCallsign.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded callsign from CLX (session only): %1").arg(loadedCallsign));
                }
                // Also log operator name and state if available
                QString operatorName = clxFile.station().operatorName();
                QString operatorState = clxFile.station().state();
                if (!operatorName.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator name from CLX (session only): %1").arg(operatorName));
                }
                if (!operatorState.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator state from CLX (session only): %1").arg(operatorState));
                }
                // Also load the contest mode
                loadedMode = clxFile.contest().mode();
                if (!loadedMode.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded contest mode from CLX: %1").arg(loadedMode));
                }

                // Load contest-specific memories
                if (clxFile.useContestMemories()) {
                    m_useContestMemories = true;
                    m_contestCwMemories = clxFile.cwMemories();
                    m_contestSsbMemories = clxFile.ssbMemories();
                    DebugLogger::instance().log("MainWindow", QString("Loaded contest-specific memories from CLX: %1 CW, %2 SSB")
                        .arg(m_contestCwMemories.size()).arg(m_contestSsbMemories.size()));
                } else {
                    m_useContestMemories = false;
                    m_contestCwMemories.clear();
                    m_contestSsbMemories.clear();
                }
                loadCWMemories();
                loadSsbMemories();
            }

            DebugLogger::instance().log("MainWindow",
                QString("Loaded from CLX: contestFile='%1' stationClass='%2' exchangeName='%3' exchangeId='%4'").arg(contestFile, stationClass, stationClassExchangeName, stationClassExchangeId));
            
            // Load the contest definition if specified
            if (!contestFile.isEmpty()) {
                QString contestPath = Settings::getContestsPath() + "/" + contestFile;
                if (QFile::exists(contestPath)) {
                    // Set station class and exchange data BEFORE loading contest definition to prevent duplicate dialog
                    if (!stationClass.isEmpty()) {
                        // Need to create engine first if needed
                        if (!m_contestEngine) {
                            m_contestEngine = new ContestEngine(this);
                            // Make sure the new engine has the DXCC database
                            m_contestEngine->setDxccDatabase(m_dxccDatabase);
                        }
                        m_contestEngine->setStationClass(stationClass);
                        DebugLogger::instance().log("MainWindow", QString("Set station class in loadLogFile: %1").arg(stationClass));
                    }
                    
                    if (!stationClassExchangeName.isEmpty() || !stationClassExchangeId.isEmpty()) {
                        if (!m_contestEngine) {
                            m_contestEngine = new ContestEngine(this);
                            m_contestEngine->setDxccDatabase(m_dxccDatabase);
                        }
                        m_contestEngine->setStationClassExchangeName(stationClassExchangeName);
                        m_contestEngine->setStationClassExchangeId(stationClassExchangeId);
                        DebugLogger::instance().log("MainWindow", QString("Set station class exchange in loadLogFile: Name='%1' Id='%2'").arg(stationClassExchangeName, stationClassExchangeId));
                    }
                    
                    // Pass false to NOT restore/prompt for station class since we already have it
                    loadContestDefinition(contestPath, false);
                    
                    // Restore userPromptValues after contest is loaded
                    if (!userPromptValues.isEmpty() && m_contestEngine) {
                        for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it) {
                            m_contestEngine->setUserPromptValue(it.key(), it.value());
                        }
                        // Log the restored values
                        DebugLogger::instance().log("MainWindow", QString("Restored %1 user prompt values from CLX").arg(userPromptValues.size()));
                        for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it) {
                            DebugLogger::instance().log("MainWindow", QString("  %1 = '%2'").arg(it.key(), it.value()));
                        }

                        // Apply mode restriction if any userPrompt has "restrictMode": true
                        applyRestrictedModeFromUserPrompts();

                        // Refresh multiplier widget with effective mults (filtered by station type)
                        if (m_multiplierWidget && m_contestDefinition.contains("ui")) {
                            QJsonObject ui = m_contestDefinition["ui"].toObject();
                            if (ui["showMultiplierPanel"].toBool(false)) {
                                DebugLogger::instance().log("MultiplierWidget",
                                    QString("loadLogFile post-restore refresh: stationType='%1', effective mults=%2")
                                        .arg(m_contestEngine->getUserPromptValue("stationType"))
                                        .arg(m_contestEngine->getEffectiveNamedMultiplierList().size()));
                                m_multiplierWidget->setMultiplierList(m_contestEngine->getEffectiveNamedMultiplierList());
                            }
                        }
                    }

                    // Prompt for any required userPrompts not present in the loaded file
                    promptForMissingUserPrompts();

                    // If we loaded a mode from the CLX file, restrict to that mode
                    if (!loadedMode.isEmpty()) {
                        m_contestEngine->setRestrictedMode(loadedMode);
                    }
                    
                    // Check if contest version has changed
                    if (!loadedContestVersion.isEmpty() && !m_contestDefinition.isEmpty()) {
                        QString currentVersion = m_contestDefinition["contest"].toObject()["version"].toString();
                        DebugLogger::instance().log("MainWindow", 
                            QString("Contest version check: Log file v%1, Current definition v%2").arg(loadedContestVersion, currentVersion));
                        if (!currentVersion.isEmpty() && !isSemanticVersionEqual(currentVersion, loadedContestVersion)) {
                            DebugLogger::instance().log("MainWindow", "Version mismatch detected");
                        }
                    }
                }
            }
        }
        
        // Batch-insert all QSOs in one model reset (avoids N individual signals/repaints)
        loadQsosIntoModel(loadedQsos, progressDialog);

        m_currentFile = filename;
        m_isModified = false;
        updateWindowTitle();
        
        progressDialog->close();
        progressDialog->deleteLater();
        
        // Auto-recalculate score on background thread to avoid blocking UI
        QString myCallsign = getSessionCallsign();
        
        // Create NEW progress dialog for scoring phase
        QProgressDialog *scoringProgressDialog = new QProgressDialog("Scoring QSOs...", QString(), 0, loadedQsos.count(), this);
        scoringProgressDialog->setWindowTitle("Scoring Log");
        scoringProgressDialog->setWindowModality(Qt::WindowModal);
        scoringProgressDialog->setAutoClose(false);
        scoringProgressDialog->setAutoReset(false);
        scoringProgressDialog->show();
        QApplication::processEvents();
        
        // Create scoring worker and thread
        QThread *scoringThread = new QThread(this);
        ScoringWorker *scoringWorker = new ScoringWorker(loadedQsos, m_contestEngine, myCallsign, nullptr);
        scoringWorker->moveToThread(scoringThread);
        
        connect(scoringThread, &QThread::started, scoringWorker, &ScoringWorker::doScore);
        
        connect(scoringWorker, &ScoringWorker::progressUpdated, this, [this, scoringProgressDialog](int current, int total) {
            scoringProgressDialog->setMaximum(total);
            scoringProgressDialog->setValue(current);
            QApplication::processEvents();
        });
        
        connect(scoringWorker, &ScoringWorker::scoringComplete, this, [this, loadedQsos, filename, scoringProgressDialog, scoringThread, scoringWorker](QList<QsoRecord> scoredQsos, bool success) {
            if (success) {
                // Clear the old model and add scored QSOs
                m_qsoModel->clear();
                for (const auto& qso : scoredQsos) {
                    m_qsoModel->addQso(qso);
                }
                
                // Scroll to the bottom to show the most recent QSO
                if (m_qsoTable) {
                    m_qsoTable->scrollToBottom();
                }
                
                // Reset modified flag since we just loaded the file
                m_isModified = false;
                updateWindowTitle();

                // Update score display
                if (m_scoreWidget) {
                    m_scoreWidget->resetScore();
                    auto score = m_contestEngine->getRunningScore();
                    m_scoreWidget->updateScore(score);
                }
                updateSnapshotScore();
                updateSnapshotQsos();
                // Update multiplier widget
                if (m_multiplierWidget && m_multiplierDock) {
                    QString multType = m_contestEngine->getMultiplierType();
                    if (multType == "multsOnce")
                        m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMults());
                    else if (multType == "multsPerBand")
                        m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBand());
                    else if (multType == "multsPerMode")
                        m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerMode());
                    else if (multType == "multsPerBandAndMode")
                        m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBandAndMode());
                }

                if (!m_testMode)
                    checkForCrashBackups();
                m_statusLabel->setText("File loaded: " + filename + " (" +
                    QString::number(scoredQsos.count()) + " QSOs)");

                // If in debug log mode, generate summary sheet to debug log
                if (m_debugLogMode) {
                    generateSummaryToDebugLog();
                }
                
                // If in test mode, log the score and exit
                if (m_testMode) {
                    auto score = m_contestEngine->getRunningScore();
                    // Use "INFO" component so this is always written regardless of MainWindow debug setting
                    DebugLogger::instance().log("INFO",
                        QString("TEST MODE: Log fully loaded. CLAIMED_SCORE=%1").arg(score.contestScore));
                    // Exit after a brief delay to ensure log is written
                    QTimer::singleShot(100, qApp, &QCoreApplication::quit);
                }
            } else {
                QMessageBox::critical(this, "Error", "Failed to score QSOs");
            }
            
            scoringProgressDialog->close();
            scoringProgressDialog->deleteLater();
            scoringThread->quit();
            scoringThread->wait();
            scoringWorker->deleteLater();
            scoringThread->deleteLater();
            
            // Set focus to call field
            if (m_callEdit) {
                m_callEdit->setFocus();
            }
        });
        
        scoringThread->start();
    });
    
    loadThread->start();
}

void MainWindow::onSaveLog()
{
    if (m_currentFile.isEmpty()) {
        onSaveLogAs();
        return;
    }
    
    // Save file
    FileHandler fileHandler;
    bool success = false;

    // Use contest-aware save for .clx files
    if (m_currentFile.endsWith(".clx", Qt::CaseInsensitive) && !m_contestDefinition.isEmpty()) {
        QString stationClass = m_contestEngine ? m_contestEngine->getStationClass() : QString();
        QString stationClassExchangeName = m_contestEngine ? m_contestEngine->getStationClassExchangeName() : QString();
        QString stationClassExchangeId = m_contestEngine ? m_contestEngine->getStationClassExchangeId() : QString();
        QMap<QString, QString> userPromptValues = m_contestEngine ? m_contestEngine->getUserPromptValues() : QMap<QString, QString>();

        // Pass contest-specific memories to FileHandler
        fileHandler.setUseContestMemories(m_useContestMemories);
        fileHandler.setContestCwMemories(m_contestCwMemories);
        fileHandler.setContestSsbMemories(m_contestSsbMemories);

        // Pass the computed score so the statistics block is accurate
        if (m_contestEngine) {
            const ContestEngine::ContestScore& cs = m_contestEngine->getRunningScore();
            fileHandler.setComputedScore(cs.contactScore, cs.contestScore);
        }

        success = fileHandler.saveClxWithContest(m_currentFile, m_qsoModel->getQsos(), m_contestFile, m_contestDefinition, stationClass, stationClassExchangeName, stationClassExchangeId, userPromptValues, *m_sessionStationInfo);
    } else {
        success = fileHandler.save(m_currentFile, m_qsoModel->getQsos());
    }
    
    if (success) {
        removeBackup();
        m_isModified = false;
        updateWindowTitle();

        // Update call history if auto-save is enabled
        if (CallHistory::instance().isAutoSaveEnabled()) {
            updateCallHistory();
        }

        m_statusLabel->setText("File saved: " + m_currentFile + " (" +
            QString::number(m_qsoModel->count()) + " QSOs)");
    } else {
        QMessageBox::warning(this, "Save Failed",
            "Failed to save file:\n\n" + fileHandler.lastError());
    }
}

void MainWindow::onSaveLogAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Log File", "",
        "ContestLogX 2.0 Format (*.clx)");

    if (fileName.isEmpty())
        return;

    // Ensure .clx extension
    if (!fileName.endsWith(".clx", Qt::CaseInsensitive)) {
        fileName += ".clx";
    }
    
    m_currentFile = fileName;
    onSaveLog();
}

void MainWindow::onExportAdif()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Log", "",
        "ADIF Files (*.adi);;"
        "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    // Ensure a recognized extension
    if (!fileName.endsWith(".adi", Qt::CaseInsensitive) &&
        !fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        fileName += ".adi";
    }

    FileHandler fileHandler;
    fileHandler.setStationCallsign(getSessionCallsign());
    if (fileHandler.save(fileName, m_qsoModel->getQsos())) {
        m_statusLabel->setText("Exported: " + fileName + " (" +
            QString::number(m_qsoModel->count()) + " QSOs)");
    } else {
        QMessageBox::warning(this, "Export Failed",
            "Failed to export file:\n\n" + fileHandler.lastError());
    }
}

void MainWindow::onImportAdif()
{
    if (m_contestDefinition.isEmpty()) {
        QMessageBox::warning(this, "Import",
            "No log is currently open.\n\nPlease create or open a log first.");
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this,
        "Import File", "",
        "All Supported Files (*.adi *.adif *.log *.cbr *.cab);;"
        "ADIF Files (*.adi *.adif);;"
        "Cabrillo Files (*.log *.cbr *.cab);;"
        "All Files (*)");

    if (fileName.isEmpty())
        return;

    FileHandler fileHandler;
    fileHandler.setContestDefinition(m_contestDefinition);
    QList<QsoRecord> importedQsos;
    if (!fileHandler.load(fileName, importedQsos)) {
        QMessageBox::warning(this, "Import Failed",
            "Failed to import file:\n\n" + fileHandler.lastError());
        return;
    }

    if (importedQsos.isEmpty()) {
        QMessageBox::information(this, "Import",
            "No QSOs found in the selected file.");
        return;
    }

    int startSerial = m_qsoModel->count() + 1;
    for (int i = 0; i < importedQsos.size(); ++i) {
        importedQsos[i].setSerial(startSerial + i);
        m_qsoModel->addQso(importedQsos[i]);
    }

    m_isModified = true;
    updateWindowTitle();

    onRecalculateScore();

    m_statusLabel->setText(QString("Imported %1 QSOs from %2")
        .arg(importedQsos.size())
        .arg(QFileInfo(fileName).fileName()));
}

void MainWindow::onExit()
{
    // Route through close() so closeEvent() runs and saves window geometry
    // and panel state. closeEvent() also handles the unsaved-log prompt.
    close();
}

void MainWindow::onPreferences()
{
    PreferencesDialog dialog(this);

    // Wire settingsApplied() — fires on BOTH Apply and OK — to all the
    // post-save side-effects that used to run only after OK closed the
    // dialog. This means clicking Apply instantly restarts the Remote
    // Dashboard server, re-applies fonts, etc. without closing Prefs,
    // so the operator can e.g. enable the dashboard and immediately
    // scan the QR code that's right there in the dialog.
    connect(&dialog, &PreferencesDialog::settingsApplied, this, [this, &dialog]() {
        // Update filter shortcut key in case it was changed
        if (m_filterShortcut) {
            QMap<QString, QString> storedShortcuts = Settings::instance().getShortcuts();
            QString filterKey = storedShortcuts.value("qsoViewFilter", "Ctrl+F");
            m_filterShortcut->setKey(QKeySequence(filterKey));
        }

        if (dialog.themeChanged()) {
            applyTheme();
            QMessageBox::information(this, "Theme Changed",
                "Some elements may require a restart to fully apply the new theme.");
        }
        if (dialog.fontsChanged()) {
            applyFontSettings();
        }
        if (dialog.lookupChanged()) {
            initCallsignLookup();
        }

        // Remote Dashboard: restart the HTTP server to pick up any changes
        // to enabled / port / bind mode / token. stop() is safe to call
        // when the server isn't running, and start() respects the enabled
        // flag via the Settings singleton.
        if (m_httpServer) {
            m_httpServer->stop();
            if (Settings::instance().getRemoteControlEnabled()) {
                ensureRemoteControlToken();
                m_httpServer->start();
            }
        }
        if (m_dxClusterPanel) {
            m_dxClusterPanel->loadSettings();
        }
        // Update DXCC country from callsign
        if (m_dxccDatabase) {
            QString call = Settings::instance().getCallsign();
            if (!call.isEmpty()) {
                auto entity = m_dxccDatabase->lookupCallsign(call);
                if (entity.dxcc > 0)
                    Settings::instance().setDxccCountry(entity.primaryPrefix);
            }
        }
        // Sync session station info with updated preferences — only fill empty fields
        // so we don't overwrite per-session overrides the operator has already set
        if (m_sessionStationInfo && dialog.stationChanged()) {
            Settings &s = Settings::instance();
            if (m_sessionStationInfo->callsign().isEmpty())
                m_sessionStationInfo->setCallsign(s.getCallsign());
            if (m_sessionStationInfo->operatorName().isEmpty())
                m_sessionStationInfo->setOperatorName(s.getOperatorName());
            if (m_sessionStationInfo->grid().isEmpty())
                m_sessionStationInfo->setGrid(s.getGridSquare());
            if (m_sessionStationInfo->state().isEmpty())
                m_sessionStationInfo->setState(s.getState());
            if (m_sessionStationInfo->cqZone() <= 0)
                m_sessionStationInfo->setCqZone(s.getCqZone());
            if (m_sessionStationInfo->ituZone() <= 0)
                m_sessionStationInfo->setItuZone(s.getItuZone());
            if (m_sessionStationInfo->arrlSection().isEmpty())
                m_sessionStationInfo->setArrlSection(s.getArrlSection());
        }
    });

    dialog.exec();
}

void MainWindow::applyFontSettings()
{
    Settings& settings = Settings::instance();

    auto applyFont = [](QWidget* widget, const QFont& font) {
        if (widget && !font.family().isEmpty())
            widget->setFont(font);
    };

    applyFont(m_qsoEntryGroup, settings.getPanelFont("qsoEntry"));
    applyFont(m_qsoTable,      settings.getPanelFont("qsoLog"));
    applyFont(m_scpWidget,     settings.getPanelFont("scp"));
    applyFont(m_cwConsole,     settings.getPanelFont("cwKeyboard"));

    QFont dxFont = settings.getPanelFont("dxCluster");
    if (m_dxClusterPanel && !dxFont.family().isEmpty())
        m_dxClusterPanel->setTableFont(dxFont);

    QFont scoreFont = settings.getPanelFont("scoreWidget");
    if (m_scoreWidget && !scoreFont.family().isEmpty())
        m_scoreWidget->setBaseFont(scoreFont);

    if (m_rateWidget && !scoreFont.family().isEmpty())
        m_rateWidget->setBaseFont(scoreFont);

    QFont cwMemFont = settings.getPanelFont("cwMemories");
    if (m_cwConsole && !cwMemFont.family().isEmpty())
        m_cwConsole->setMemoriesFont(cwMemFont);

    QFont ssbMemFont = settings.getPanelFont("ssbMemories");
    if (m_ssbMemoriesWidget && !ssbMemFont.family().isEmpty())
        m_ssbMemoriesWidget->widget()->setFont(ssbMemFont);

    QFont decoderFont = settings.getPanelFont("cwDecoder");
    if (!decoderFont.family().isEmpty()) {
        if (m_cwDecoderLeft)  m_cwDecoderLeft->setBaseFont(decoderFont);
        if (m_cwDecoderRight) m_cwDecoderRight->setBaseFont(decoderFont);
    }
}

void MainWindow::onManageCallHistory()
{
    CallHistoryDialog dialog(this);
    dialog.exec();
}

void MainWindow::onImportCallHistory()
{
    if (!m_contestEngine) {
        QMessageBox::warning(this, "Import Call History", "Please load a contest before importing call history.");
        return;
    }

    // Get the exchange fields defined for the current contest (received side)
    QStringList contestFields = m_contestEngine->getCallHistoryFieldsToSave();
    if (contestFields.isEmpty()) {
        QMessageBox::warning(this, "Import Call History", "Current contest has no exchange fields defined for call history.");
        return;
    }

    // Open file dialog
    QString fileName = QFileDialog::getOpenFileName(this, "Import Call History",
        QString(), "Call History Files (*.txt *.csv);;All Files (*)");
    if (fileName.isEmpty()) return;

    // Read and parse the file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Import Call History",
            QString("Could not open file: %1").arg(file.errorString()));
        return;
    }

    QStringList fileFieldNames;
    QList<QStringList> dataRows;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Skip comments
        if (line.startsWith('#')) continue;

        // Parse format specifier: !!Order!!,Call,Field1,Field2,...
        if (line.startsWith("!!")) {
            // Remove the !!Order!! tag and split
            int firstComma = line.indexOf(',');
            if (firstComma >= 0) {
                QString fieldsPart = line.mid(firstComma + 1);
                fileFieldNames = fieldsPart.split(',');
                for (QString& f : fileFieldNames)
                    f = f.trimmed();
            }
            continue;
        }

        // Data line — split by comma
        QStringList fields = line.split(',');
        for (QString& f : fields)
            f = f.trimmed();
        if (!fields.isEmpty())
            dataRows.append(fields);
    }
    file.close();

    if (fileFieldNames.isEmpty()) {
        QMessageBox::warning(this, "Import Call History",
            "No format specifier (!!Order!!) found in file.");
        return;
    }
    if (dataRows.isEmpty()) {
        QMessageBox::warning(this, "Import Call History", "No data records found in file.");
        return;
    }

    // Find the Call field index (skip it in mapping since it's always the callsign)
    int callIndex = -1;
    for (int i = 0; i < fileFieldNames.size(); ++i) {
        if (fileFieldNames[i].compare("Call", Qt::CaseInsensitive) == 0) {
            callIndex = i;
            break;
        }
    }
    if (callIndex < 0) {
        QMessageBox::warning(this, "Import Call History",
            "No 'Call' field found in format specifier.");
        return;
    }

    // Build list of non-Call file fields that need mapping
    QStringList fieldsToMap;
    QList<int> fieldsToMapIndices;
    for (int i = 0; i < fileFieldNames.size(); ++i) {
        if (i == callIndex) continue;
        fieldsToMap.append(fileFieldNames[i]);
        fieldsToMapIndices.append(i);
    }

    if (fieldsToMap.isEmpty()) {
        QMessageBox::warning(this, "Import Call History",
            "No exchange fields found to map (only 'Call' was in the format).");
        return;
    }

    // Show mapping dialog
    QDialog mappingDialog(this);
    mappingDialog.setWindowTitle("Map Call History Fields");
    mappingDialog.setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(&mappingDialog);

    QLabel *infoLabel = new QLabel(
        QString("File contains %1 records with fields: %2\n\n"
                "Map each file field to a contest exchange field:")
            .arg(dataRows.size())
            .arg(fileFieldNames.join(", ")));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    QFormLayout *formLayout = new QFormLayout();
    QList<QComboBox*> mappingCombos;

    for (const QString& fileField : fieldsToMap) {
        QComboBox *combo = new QComboBox();
        combo->addItem("(skip)", QString());
        for (const QString& contestField : contestFields)
            combo->addItem(contestField, contestField);

        // Auto-select if there's a plausible match
        for (int j = 0; j < contestFields.size(); ++j) {
            if (contestFields[j].compare(fileField, Qt::CaseInsensitive) == 0) {
                combo->setCurrentIndex(j + 1); // +1 for "(skip)"
                break;
            }
        }

        formLayout->addRow(QString("File field \"%1\" →").arg(fileField), combo);
        mappingCombos.append(combo);
    }
    layout->addLayout(formLayout);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &mappingDialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &mappingDialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (mappingDialog.exec() != QDialog::Accepted) return;

    // Build the field mapping: file column index → contest field name
    QMap<int, QString> columnMapping;
    for (int i = 0; i < fieldsToMapIndices.size(); ++i) {
        QString contestField = mappingCombos[i]->currentData().toString();
        if (!contestField.isEmpty())
            columnMapping[fieldsToMapIndices[i]] = contestField;
    }

    if (columnMapping.isEmpty()) {
        QMessageBox::warning(this, "Import Call History", "No fields were mapped. Import cancelled.");
        return;
    }

    // Import the records
    int imported = 0;
    for (const QStringList& row : dataRows) {
        if (callIndex >= row.size()) continue;

        QString callsign = row[callIndex].toUpper().trimmed();
        if (callsign.isEmpty()) continue;

        QMap<QString, QString> fields;
        for (auto it = columnMapping.begin(); it != columnMapping.end(); ++it) {
            if (it.key() < row.size()) {
                QString value = row[it.key()].trimmed().toUpper();
                if (!value.isEmpty())
                    fields[it.value()] = value;
            }
        }

        if (!fields.isEmpty()) {
            CallHistory::instance().addOrUpdateRecord(callsign, fields);
            imported++;
        }
    }

    CallHistory::instance().save();

    DebugLogger::instance().log("MainWindow",
        QString("Imported %1 call history records from %2").arg(imported).arg(fileName));

    QMessageBox::information(this, "Import Call History",
        QString("Successfully imported %1 call history records.").arg(imported));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Handle F1-F8 function keys for CW/SSB memories (without modifiers)
    if (event->modifiers() == Qt::NoModifier) {
        int fKeyIndex = -1;

        switch (event->key()) {
            case Qt::Key_F1: fKeyIndex = 0; break;
            case Qt::Key_F2: fKeyIndex = 1; break;
            case Qt::Key_F3: fKeyIndex = 2; break;
            case Qt::Key_F4: fKeyIndex = 3; break;
            case Qt::Key_F5: fKeyIndex = 4; break;
            case Qt::Key_F6: fKeyIndex = 5; break;
            case Qt::Key_F7: fKeyIndex = 6; break;
            case Qt::Key_F8: fKeyIndex = 7; break;
        }

        if (fKeyIndex >= 0) {
            QString mode = m_lastMode.toUpper();
            DebugLogger::instance().log("MainWindow",
                QString("Hardware Function key F%1 pressed, current mode: %2").arg(fKeyIndex + 1).arg(mode));

            if (mode == "CW" || mode == "CWR") {
                // Trigger CW memory
                if (m_cwConsole) {
                    DebugLogger::instance().log("MainWindow",
                        QString("Triggering CW memory F%1").arg(fKeyIndex + 1));
                    m_cwConsole->onMemoryButton(fKeyIndex);
                    return;
                } else {
                    DebugLogger::instance().log("MainWindow", "CW console not available");
                }
            } else if (mode == "USB" || mode == "LSB") {
                // Trigger SSB memory
                if (m_ssbMemoriesWidget) {
                    DebugLogger::instance().log("MainWindow",
                        QString("Triggering SSB memory F%1").arg(fKeyIndex + 1));
                    m_ssbMemoriesWidget->triggerMemory(fKeyIndex);
                    return;
                } else {
                    DebugLogger::instance().log("MainWindow", "SSB memories widget not available");
                }
            } else {
                DebugLogger::instance().log("MainWindow",
                    QString("Hardware Function key F%1 ignored - mode %2 not supported for memories")
                    .arg(fKeyIndex + 1).arg(mode));
            }
            return;
        }
    }

    // Build the key sequence from the event
    int keyWithModifiers = event->key() | (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    QKeySequence eventSeq(keyWithModifiers);
    QString eventSeqStr = eventSeq.toString();

    // Check against registered shortcuts (merge with hard-coded defaults for any not yet saved)
    QMap<QString, QString> shortcuts = Settings::instance().getShortcuts();
    static const QMap<QString, QString> defaultShortcuts = {
        {"clearQsoEntry",    "Ctrl+W"},
        {"preSaveCall",      "Ctrl+S"},
        {"qsoViewFilter",    "Ctrl+F"},
        {"toggleMemoryType", "Ctrl+T"},
        {"switchRadio",      "`"},
        {"qsyBack",          "Alt+B"},
    };
    for (auto dit = defaultShortcuts.begin(); dit != defaultShortcuts.end(); ++dit) {
        if (!shortcuts.contains(dit.key()))
            shortcuts[dit.key()] = dit.value();
    }

    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        QKeySequence storedSeq(it.value());
        QString storedSeqStr = storedSeq.toString();

        // Compare the key sequences as strings
        if (eventSeqStr == storedSeqStr) {
            if (it.key() == "clearQsoEntry") {
                if (m_so2rEnabled && m_activeRadio == ActiveRadio::Right)
                    clearEntryFormR();
                else
                    clearEntryForm();
                return;
            } else if (it.key() == "preSaveCall") {
                preSaveCall();
                return;
            } else if (it.key() == "qsoViewFilter") {
                m_filterBar->setVisible(true);
                m_filterEdit->setFocus();
                m_filterEdit->selectAll();
                return;
            } else if (it.key() == "toggleRunSP") {
                onToggleRunSP();
                return;
            } else if (it.key() == "toggleMemoryType") {
                onToggleMemoryType();
                return;
            } else if (it.key() == "switchRadio") {
                switchActiveRadio();
                return;
            } else if (it.key() == "qsyBack") {
                onQsyBack();
                return;
            }
        }
    }

    QMainWindow::keyPressEvent(event);
}


void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Trigger dock state save when window is resized (includes dock widget resizing)
    // Don't trigger during state restoration to avoid overwriting the restored state
    if (m_dockStateSaveTimer && !m_restoringState) {
        m_dockStateSaveTimer->start();
    }
}


void MainWindow::onCallChanged(const QString& text)
{
    // Determine which call field triggered this
    QLineEdit* callEdit = qobject_cast<QLineEdit*>(sender());
    if (!callEdit) callEdit = m_callEdit;

    // Force uppercase
    if (text != text.toUpper()) {
        int cursorPos = callEdit->cursorPosition();
        callEdit->setText(text.toUpper());
        callEdit->setCursorPosition(cursorPos);
        return;  // Return early to avoid processing twice
    }

    // If Radio R's call field changed but Radio R is not the active radio,
    // skip the rest (dupe check, exchange pre-fill) to avoid cross-contamination
    if (m_so2rEnabled && callEdit == m_entryWidgetsR.callEdit
        && m_activeRadio != ActiveRadio::Right)
        return;
    if (m_so2rEnabled && callEdit == m_callEdit
        && m_activeRadio != ActiveRadio::Left)
        return;
    
    // Look up previous QSO with this callsign and pre-fill exchange
    QString callsign = text.trimmed().toUpper();
    if (callsign.isEmpty()) {
        m_statusLabel->setText("Ready");
        return;
    }

    // Reset sort to default (#/serial) as soon as the operator starts typing,
    // so the new entry will appear at the bottom of the log when logged.
    if (m_qsoTable->horizontalHeader()->sortIndicatorSection() != 0)
        m_qsoTable->sortByColumn(0, Qt::AscendingOrder);

    // Clear any active filter when the user starts working on a new QSO entry
    if (m_filterBar && m_filterBar->isVisible()) {
        m_filterEdit->clear();
        m_filterBar->setVisible(false);
        m_qsoModel->setFilter("");
    }
    
    bool found = false;
    
    // First, try to get from call history if enabled
    if (CallHistory::instance().isEnabled()) {
        QMap<QString, QString> historyRecord = CallHistory::instance().getRecord(callsign);
        if (!historyRecord.isEmpty()) {
            // Pre-fill exchange fields with values from call history
            for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
                QString fieldName = it.key();
                QLineEdit* field = it.value();
                
                // Look for matching fields in history record (e.g., NAMEr, EXCHr, NAME, EXC, STATE, QTH, etc.)
                if (historyRecord.contains(fieldName)) {
                    field->setText(historyRecord[fieldName].toUpper());
                    found = true;
                } else if (fieldName == "NAMEr" && historyRecord.contains("NAMEs")) {
                    // Try sent name if received name not available
                    field->setText(historyRecord["NAMEs"].toUpper());
                    found = true;
                } else if (fieldName == "NAMEr" && historyRecord.contains("NAME")) {
                    field->setText(historyRecord["NAME"].toUpper());
                    found = true;
                }
            }
            
            // Log regardless of whether we found fields (the call might be in history even without exchange data)
            DebugLogger::instance().log("MainWindow", 
                QString("Pre-filled exchange from call history for %1").arg(callsign));
            // Check for dupe using active radio's freq/mode
            bool isRight = m_so2rEnabled && m_activeRadio == ActiveRadio::Right;
            double activeFreq = isRight ? m_lastFrequencyR : m_lastFrequency;
            const QString& activeMode = isRight ? m_lastModeR : m_lastMode;
            QsoRecord tempQso;
            tempQso.setCall(callsign);
            tempQso.setFrequency(QString::number(activeFreq, 'f', 1));
            tempQso.setMode(activeMode);
            // Set band from current frequency - use same method as logged QSO
            QString band = m_contestEngine->getBandFromFrequency(activeFreq);
            if (!band.isEmpty()) {
                tempQso.setBandName(band);
            }
            QList<QsoRecord> allQsos = m_qsoModel->getQsos();
            
            if (m_contestEngine && m_contestEngine->isDupe(tempQso, allQsos)) {
                QString dupeDetails = getDupeQsoDetails(callsign, allQsos);
                QString message = "<span style='color: red;'>⚠</span> DUPE: " + callsign;
                if (!dupeDetails.isEmpty()) {
                    message += " (" + dupeDetails + ")";
                }
                m_statusLabel->setText(message);
                flashDupeWarning();
            } else {
                m_statusLabel->setText("Ready");
            }
            return;
        }
    }
    
    // Fallback to searching most recent QSO with this callsign
    QList<QsoRecord> allQsos = m_qsoModel->getQsos();
    QsoRecord lastQso;
    
    // Search backwards to find the most recent QSO with this call
    for (int i = allQsos.count() - 1; i >= 0; --i) {
        if (allQsos[i].getCall().toUpper() == callsign) {
            lastQso = allQsos[i];
            found = true;
            break;
        }
    }
    
    DebugLogger::instance().log("MainWindow", 
        QString("After fallback search: found=%1, allQsos.count()=%2").arg(found).arg(allQsos.count()));
    
    if (found) {
        // Pre-fill exchange fields with values from last QSO
        for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
            QString fieldName = it.key();
            QLineEdit* field = it.value();
            
            // For NAMEr and EXCHr fields, use the received values from last QSO
            if (fieldName == "NAMEr") {
                QString lastName = lastQso.getExchangeField("NAMEr");
                if (!lastName.isEmpty()) {
                    field->setText(lastName.toUpper());
                }
            } else if (fieldName == "EXCHr") {
                QString lastExch = lastQso.getExchangeField("EXCHr");
                if (!lastExch.isEmpty()) {
                    field->setText(lastExch.toUpper());
                }
            }
        }
        
        DebugLogger::instance().log("MainWindow", 
            QString("Pre-filled exchange from last QSO with %1").arg(callsign));
    }
    
    // Check for dupe regardless of pre-fill success — use active radio's freq/mode
    {
    bool isRight = m_so2rEnabled && m_activeRadio == ActiveRadio::Right;
    double activeFreq = isRight ? m_lastFrequencyR : m_lastFrequency;
    const QString& activeMode = isRight ? m_lastModeR : m_lastMode;
    QsoRecord tempQso;
    tempQso.setCall(callsign);
    tempQso.setFrequency(QString::number(activeFreq, 'f', 1));
    tempQso.setMode(activeMode);
    // Set band from current frequency
    QString band = m_contestEngine->getBandFromFrequency(activeFreq);
    if (!band.isEmpty()) {
        tempQso.setBandName(band);
    }
    
    
    if (m_contestEngine && m_contestEngine->isDupe(tempQso, allQsos)) {
        QString dupeDetails = getDupeQsoDetails(callsign, allQsos);
        QString message = "<span style='color: red;'>⚠</span> DUPE: " + callsign;
        if (!dupeDetails.isEmpty()) {
            message += " (" + dupeDetails + ")";
        }
        m_statusLabel->setText(message);
        flashDupeWarning();
    } else {
        m_statusLabel->setText("Ready");
    }
    } // dupe check scope
}

void MainWindow::onModeChanged(int index)
{
    // Mode change handler (not used now - mode is changed via freq/mode dialog)
}

void MainWindow::onExchangeChanged(const QString& text)
{
    // Exchange changed
}

void MainWindow::onLogQso()
{
    QsoRecord qso;
    
    // Get callsign from either the dynamic CALL field or the call edit for the active radio
    auto& exchFields = activeExchangeFields();
    QLineEdit* callEdit = activeCallEdit();
    QString callsign;
    if (exchFields.contains("CALL")) {
        callsign = exchFields["CALL"]->text().trimmed().toUpper();
    } else {
        callsign = callEdit->text().trimmed().toUpper();
    }

    if (callsign.isEmpty()) {
        QMessageBox::warning(this, "Invalid QSO", "Callsign cannot be empty");
        callEdit->setFocus();
        return;
    }
    
    // Check for non-standard call suffix format (home-call/suffix instead of location/home-call)
    if (callsign.contains('/')) {
        int slashPos = callsign.indexOf('/');
        QString beforeSlash = callsign.left(slashPos);
        QString afterSlash = callsign.mid(slashPos + 1);
        
        // Non-standard pattern: home-call/location (e.g., N9OH/PJ2, VK2ABC/PJ2)
        // Standard pattern: location/home-call (e.g., PJ2/N9OH, YB1AR/2)
        
        bool looksLikeNonStandard = false;
        
        // If afterSlash is 2-3 characters and looks like a DXCC prefix
        if (afterSlash.length() >= 2 && afterSlash.length() <= 3) {
            // Check if it starts with a letter (typical for DXCC prefixes like PJ, VK, YB, etc.)
            if (afterSlash[0].isLetter()) {
                // Check if beforeSlash looks like a full home callsign (typically contains both letters and digits)
                bool hasLetters = beforeSlash.contains(QRegularExpression("[A-Z]"));
                bool hasDigits = beforeSlash.contains(QRegularExpression("\\d"));
                
                if (hasLetters && hasDigits && beforeSlash.length() >= 4) {
                    looksLikeNonStandard = true;
                }
            }
        }
        
        if (looksLikeNonStandard) {
            QString warning = QString("Note: Call format '%1' may be non-standard. Standard format is location/home-call (e.g., PJ2/N9OH)").arg(callsign);
            m_statusLabel->setText(warning);
            DebugLogger::instance().log("MainWindow", warning);
        }
    }
    
    qso.setCall(callsign);
    // Use frequency/mode from active radio
    double freq = activeFrequency();
    QString mode = activeMode();
    qso.setFrequency(QString::number(freq, 'f', 1));

    // Get band from frequency
    QString band = m_contestEngine->getBandFromFrequency(freq);
    if (!band.isEmpty()) {
        qso.setBandName(band);
    }

    qso.setMode(mode);
    qso.setDateTime(QDateTime::currentDateTimeUtc());
    qso.setSerial(m_qsoModel->count() + 1);
    
    // Validate that the mode is allowed for this contest
    QStringList allowedModes = m_contestEngine->getAllowedModes();
    if (!allowedModes.isEmpty()) {
        bool modeValid = allowedModes.contains(mode.toUpper());

        // If SSB is allowed, also accept LSB and USB
        if (!modeValid && allowedModes.contains("SSB")) {
            modeValid = (mode == "LSB" || mode == "USB");
        }

        // If any digital mode is allowed, accept USB-D and LSB-D (rig data modes)
        // Prefer DIGITAL over RTTY since rigs have a separate RTTY mode
        if (!modeValid && (mode == "USB-D" || mode == "LSB-D")) {
            QStringList digiPreference = {"DIGI", "DIGITAL", "FT8", "FT4", "RTTY", "PSK", "JT65"};
            for (const QString& preferred : digiPreference) {
                if (allowedModes.contains(preferred)) {
                    modeValid = true;
                    mode = preferred;
                    qso.setMode(mode);
                    DebugLogger::instance().log("MainWindow",
                        QString("Mapped rig data mode to contest mode: %1").arg(mode));
                    break;
                }
            }
        }

        if (!modeValid) {
            QString errorMsg = QString("Invalid mode '%1'. This contest only allows: %2")
                .arg(mode)
                .arg(allowedModes.join(", "));
            m_statusLabel->setText(errorMsg);
            DebugLogger::instance().log("MainWindow", errorMsg);
            return;
        }
    }
    
    // If this log was loaded from a file, check if the mode is restricted to the original mode
    QString restrictedMode = m_contestEngine->getRestrictedMode();
    if (!restrictedMode.isEmpty()) {
        bool modeMatches = mode.toUpper() == restrictedMode.toUpper();

        // If the restricted mode is SSB, also allow LSB and USB
        if (!modeMatches && restrictedMode.toUpper() == "SSB") {
            modeMatches = (mode == "LSB" || mode == "USB");
        }

        if (!modeMatches) {
            QString errorMsg = QString("This log file is restricted to %1 mode only. Cannot log %2 contacts.")
                .arg(restrictedMode)
                .arg(mode);
            m_statusLabel->setText(errorMsg);
            DebugLogger::instance().log("MainWindow", errorMsg);
            return;
        }
    }
    
    DebugLogger::instance().log("MainWindow", 
        QString("QSO frequency set to: %1 kHz (m_lastFrequency=%2), band=%3").arg(qso.getFrequency()).arg(m_lastFrequency, 0, 'f', 1).arg(band));
    
    // Set RST sent based on mode (always auto-calculated)
    QString rstSent = (m_lastMode == "CW" || m_lastMode == "RTTY") ? "599" : 
                      (m_lastMode.contains("DIGI")) ? "+0" : "59";
    qso.setRstSent(rstSent);
    
    // Always set serial number — used for QSO ordering even if not part of exchange
    int nextSerial = m_qsoModel->count() + 1;
    QString serialSent = QString::number(nextSerial);
    qso.setExchangeField("SNs", serialSent);
    DebugLogger::instance().log("MainWindow",
        QString("Set SNs exchange field to: '%1'").arg(serialSent));
    
    // Set our grid square if the contest has a myGridSquare user prompt
    QString ourGrid = m_contestEngine->getUserPromptValue("myGridSquare");
    if (!ourGrid.isEmpty()) {
        qso.setExchangeField("GRIDs", ourGrid);
        DebugLogger::instance().log("MainWindow", 
            QString("Set GRIDs exchange field to: '%1'").arg(ourGrid));
    }
    
    // Get exchange sent from contest class and station settings
    QString stationQth = m_sessionStationInfo->state();

    // Try to get split NAME and EXCH from contest engine
    QString sentName = m_contestEngine->getSentExchangeName();
    QString sentExch = m_contestEngine->getSentExchangeId();

    // If name is not set from contest engine, fall back to session station info
    if (sentName.isEmpty()) {
        sentName = m_sessionStationInfo->operatorName();
    }

    // If exchange is not set from contest engine, fall back appropriately
    if (sentExch.isEmpty()) {
        if (m_contestEngine->getStationClassExchangeType() == "serial")
            sentExch = serialSent;
        else
            sentExch = m_sessionStationInfo->state();
    }
    
    // If we have split fields, set them individually
    if (!sentName.isEmpty() || !sentExch.isEmpty()) {
        // Store the split exchange in the QSO record using NAMEs and EXCHs fields
        qso.setExchangeField("NAMEs", sentName);
        qso.setExchangeField("EXCHs", sentExch);
        
        DebugLogger::instance().log("MainWindow", 
            QString("Exchange sent - NAMEs: '%1', EXCHs: '%2'")
            .arg(sentName).arg(sentExch));
    } else {
        // Fallback to the old method
        QString exchSent = m_contestEngine->getDefaultSentExchange(stationQth, nextSerial);
        qso.setExchangeSent(exchSent);
        DebugLogger::instance().log("MainWindow", 
            QString("Exchange sent (legacy): '%1'").arg(exchSent));
    }
    
    // Apply exchangeFieldMapping from userPrompts
    if (m_contestDefinition.contains("userPrompts")) {
        QJsonArray prompts = m_contestDefinition["userPrompts"].toArray();
        for (const QJsonValue& promptVal : prompts) {
            QJsonObject promptObj = promptVal.toObject();
            if (promptObj.contains("exchangeFieldMapping")) {
                QJsonObject mapping = promptObj["exchangeFieldMapping"].toObject();
                for (auto it = mapping.begin(); it != mapping.end(); ++it) {
                    QString promptId = it.key();
                    QString exchangeFieldName = it.value().toString();
                    QString promptValue = m_contestEngine->getUserPromptValue(promptId);
                    if (!promptValue.isEmpty()) {
                        qso.setExchangeField(exchangeFieldName, promptValue);
                        DebugLogger::instance().log("MainWindow", 
                            QString("Exchange sent - %1: '%2' (from userPrompt %3)")
                            .arg(exchangeFieldName, promptValue, promptId));
                    }
                }
            }
        }
    }
    
    // Set exchange fields from dynamic inputs (received exchange)
    if (!exchFields.isEmpty()) {
        DebugLogger::instance().log("MainWindow",
            QString("Processing %1 exchange fields").arg(exchFields.size()));

        QString receivedName;
        QString receivedExch;

        for (auto it = exchFields.begin(); it != exchFields.end(); ++it) {
            QString fieldName = it.key();
            QString value = it.value()->text().trimmed().toUpper();
            
            DebugLogger::instance().log("MainWindow", 
                QString("Field: %1 = '%2'").arg(fieldName).arg(value));
            
            if (fieldName == "CALL") {
                // CALL field should also be stored as an exchange field for validation
                qso.setExchangeField("CALL", value);
                DebugLogger::instance().log("MainWindow", 
                    QString("CALL exchange field set to: '%1'").arg(value));
            } else if (fieldName == "NAMEr") {
                // Received name field
                receivedName = value;
            } else if (fieldName == "EXCHr") {
                // Received exchange field (member ID / CWA / state)
                receivedExch = value;
            } else if (fieldName == "RSTr") {
                // Optional RST received
                DebugLogger::instance().log("MainWindow", 
                    QString("Setting RST Received: '%1'").arg(value));
                if (!value.isEmpty()) {
                    qso.setRstReceived(value);
                }
            } else if (fieldName == "SNr") {
                // Serial number received
                DebugLogger::instance().log("MainWindow", 
                    QString("Setting Serial Received: '%1'").arg(value));
                if (!value.isEmpty()) {
                    qso.setExchangeField("SNr", value);
                    DebugLogger::instance().log("MainWindow", 
                        QString("SNr exchange field set to: '%1'").arg(value));
                }
            } else if (fieldName.startsWith("EXCH")) {
                // Legacy exchange field name
                DebugLogger::instance().log("MainWindow", 
                    QString("Setting Exchange Received: '%1'").arg(value));
                qso.setExchangeReceived(value);
            } else {
                // Store as generic exchange field
                qso.setExchangeField(fieldName, value);
            }
        }
        
        // Store split NAME and EXCH fields separately in the QSO record
        if (!receivedName.isEmpty()) {
            qso.setExchangeField("NAMEr", receivedName);
        }
        if (!receivedExch.isEmpty()) {
            qso.setExchangeField("EXCHr", receivedExch);
        }
        
        DebugLogger::instance().log("MainWindow", 
            QString("Exchange received - NAMEr: '%1', EXCHr: '%2'")
            .arg(receivedName).arg(receivedExch));
        
        // Set default RST received if not provided
        if (qso.getRstReceived().isEmpty()) {
            QString defaultRst = (mode == "CW" || mode == "RTTY") ? "599" :
                                (mode.contains("DIGI")) ? "+0" : "59";
            qso.setRstReceived(defaultRst);
            DebugLogger::instance().log("MainWindow",
                QString("Set default RST received: %1").arg(defaultRst));
        }
    }

    // Validate with basic checks
    DebugLogger::instance().log("MainWindow", "Calling qso.isValid()...");
    if (!qso.isValid()) {
        DebugLogger::instance().log("MainWindow", QString("QSO validation failed: %1").arg(qso.validationError()));
        QMessageBox::warning(this, "Invalid QSO", qso.validationError());
        return;
    }
    DebugLogger::instance().log("MainWindow", "QSO is valid, proceeding...");
    
    // Contest-specific validation if we have a contest loaded
    bool isOutOfBand = false;
    if (!m_contestDefinition.isEmpty()) {
        // Check if out-of-band first
        double freqKhz = qso.getFrequency().toDouble();  // Already in kHz
        if (!m_contestEngine->isValidBand(freqKhz)) {
            isOutOfBand = true;
            qso.setOutOfBand(true);
            qso.setComment("Out of band for contest");
            qso.setPoints(0);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            m_statusLabel->setText("Warning: QSO is out of band for this contest");
            DebugLogger::instance().log("MainWindow", "QSO marked as out of band");
        } else {
            // Validate exchange fields FIRST (before checking dupes)
            DebugLogger::instance().log("MainWindow", QString("About to validate QSO - RST: '%1'").arg(qso.getRstReceived()));
            QString errorMsg;
            if (!m_contestEngine->validateQso(qso, errorMsg)) {
                DebugLogger::instance().log("MainWindow", QString("Contest validation failed: %1").arg(errorMsg));
                m_statusLabel->setText(errorMsg);
                // Don't clear the form - leave fields populated so user can correct
                return;
            }
            
            // Check for dupes
            QList<QsoRecord> existingQsos = m_qsoModel->getAllQsos();
            bool isDupe = m_contestEngine->isDupe(qso, existingQsos);
            
             if (isDupe) {
                 QMessageBox::StandardButton reply = QMessageBox::question(
                     this,
                     "Duplicate QSO",
                     QString("Duplicate: %1. Log anyway?").arg(qso.getCall()),
                     QMessageBox::Yes | QMessageBox::No);
                 
                 if (reply == QMessageBox::No) {
                     return;
                 }
                 
                 // Mark as dupe and set points to 0
                 qso.setDupe(true);
                 qso.setPoints(0);
                 qso.setMultiplierCount(0);
                 qso.setDxccCount(0);
                 
                 // Get dupe reason and set comment
                 QString dupeReason = m_contestEngine->getDupeReason(qso, existingQsos);
                 QString comment = QString("Duplicate contact for %1").arg(dupeReason);
                 qso.setComment(comment);
                 
                 m_statusLabel->setText("Duplicate QSO logged with 0 points");
                 DebugLogger::instance().log("MainWindow", 
                     QString("Dupe QSO logged: %1 with 0 points").arg(qso.getCall()));
             } else {
                 // Calculate points (pass station callsign)
                 QString myCallsign = getSessionCallsign();
                 int points = m_contestEngine->calculatePoints(qso, myCallsign);
                 qso.setPoints(points);
                 m_statusLabel->setText("QSO logged");
                 DebugLogger::instance().log("MainWindow", 
                     QString("QSO worth %1 points").arg(points));
             }
        }
    }
    
    // Attach station info from the most recent callsign lookup if it matches
    applyPendingStationInfo(qso, callsign);

    // Add the QSO first so it's included in score calculations
    m_qsoModel->addQso(qso);

    // Reset QSY Back index — new QSO becomes the most recent
    m_qsyBackIndex = -1;

    // Initialize backup on first logged QSO of the session; write on every QSO
    if (m_backupPath.isEmpty() && m_backupEnabled)
        initializeBackup();
    writeBackup();

    // Remove the spot from DX cluster if it's there
    if (m_dxClusterPanel) {
        m_dxClusterPanel->removeSpot(qso.getCall());
    }
    
    // Update running score and get total multiplier count
    if (m_contestEngine && m_scoreWidget) {
        QString myCallsign = getSessionCallsign();
        QList<QsoRecord> allQsos = m_qsoModel->getQsos();
        
        // Get the multiplier credit BEFORE updating running score
        // This uses the previous QSOs to determine if this is a new mult
        QList<QsoRecord> previousQsos = allQsos.mid(0, allQsos.count() - 1);  // All except the one we just added
        ContestEngine::QsoMultiplierCredit credit = m_contestEngine->getQsoMultiplierCredit(qso, previousQsos);
        
        // Extract grid square multiplier if present
        QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);
        QString gridSquareMult;
        for (const ContestEngine::MultiplierInfo& mult : mults) {
            if (mult.category == "gridSquares") {
                gridSquareMult = mult.value;
                break;
            }
        }
        int lastQsoIndex = m_qsoModel->count() - 1;
        if (!gridSquareMult.isEmpty()) {
            m_qsoModel->updateGridSquareMultiplier(lastQsoIndex, gridSquareMult);
        }
        
        // Update the QSO with mult credit
        m_qsoModel->updateMultiplierCount(lastQsoIndex, credit.namedMultCount);
        m_qsoModel->updateDxccCount(lastQsoIndex, credit.dxccMultCount);
        m_qsoModel->updateItuRegionCount(lastQsoIndex, credit.ituRegionMultCount);
        m_qsoModel->updateGridSquareMultiplierCount(lastQsoIndex, credit.gridSquareMultCount);
        
        // Now update running score with the updated QSO
        allQsos = m_qsoModel->getQsos();
        m_contestEngine->updateRunningScore(allQsos, myCallsign, false);  // Suppress verbose logging
        
        // Get the running score which includes calculated multipliers
        ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
        
        // Update score widget
        m_scoreWidget->updateScore(score);
        updateSnapshotScore();
        updateSnapshotQsos();

        // Update multiplier widget
        if (m_multiplierWidget && m_multiplierDock && m_multiplierDock->isVisible()) {
            QString multType = m_contestEngine->getMultiplierType();
            if (multType == "multsOnce")
                m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMults());
            else if (multType == "multsPerBand")
                m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBand());
            else if (multType == "multsPerMode")
                m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerMode());
            else if (multType == "multsPerBandAndMode")
                m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBandAndMode());
        }

        DebugLogger::instance().log("MainWindow",
            QString("QSO logged: %1 points, %2 total mults, %3 DXCCs, %4 total score")
                .arg(qso.getPoints())
                .arg(score.multipliers)
                .arg(score.dxccCount)
                .arg(score.contestScore));
    }
    
    // Clear the active radio's entry form
    if (m_so2rEnabled && m_activeRadio == ActiveRadio::Right)
        clearEntryFormR();
    else
        clearEntryForm();
    activeCallEdit()->setFocus();

    // Update QSO count in status bar
    m_qsoCountLabel->setText(QString("QSOs: %1").arg(m_qsoModel->count()));

    // Refresh band map spot statuses (newly worked station is now Worked)
    if (m_bandMapWidget)
        m_bandMapWidget->refreshAllStatuses([this](const QString &call) {
            return resolveSpotStatus(call);
        });

    // Trigger per-QSO online score posting (debounced)
    if (m_onlineScoringAction && m_onlineScoringAction->isChecked() &&
        Settings::instance().getOnlineScoringPerQso()) {
        QTimer::singleShot(2000, this, &MainWindow::onPostScore);
    }
}

void MainWindow::onQrzLookup()
{
    // Get callsign from the active radio's entry field
    QString callsign = activeCallEdit()->text().trimmed().toUpper();
    
    // If empty, use the last QSO's callsign
    if (callsign.isEmpty()) {
        QList<QsoRecord> qsos = m_qsoModel->getQsos();
        if (!qsos.isEmpty()) {
            callsign = qsos.last().getCall();
        } else {
            m_statusLabel->setText("No callsign to look up");
            return;
        }
    }
    
    // Open the appropriate callsign lookup website in the default browser
    QString service = Settings::instance().getCallsignLookupService();
    QString url;
    if (service == "qrz")
        url = QString("https://www.qrz.com/db/%1").arg(callsign);
    else
        url = QString("https://www.qrzcq.com/call/%1").arg(callsign);
    QDesktopServices::openUrl(QUrl(url));

    DebugLogger::instance().log("CallsignLookup",
        QString("Opening %1 lookup for %2").arg(service == "qrz" ? "QRZ.com" : "QRZCQ.com", callsign));
}

void MainWindow::onRigControl()
{
    RigControlDialog dialog(m_rigClient, m_rigClientR, m_so2rEnabled, this);

    connect(&dialog, &RigControlDialog::pollIntervalChanged, this, [this](int ms) {
        m_rigPollTimer->setInterval(ms);
        if (m_rigPollTimerR) m_rigPollTimerR->setInterval(ms);
        DebugLogger::instance().log("Rig", QString("Rig poll interval changed to %1 ms").arg(ms));
    });
    connect(&dialog, &RigControlDialog::backendChanged, this, &MainWindow::onRigBackendChanged);
    connect(&dialog, &RigControlDialog::backendChangedR, this, &MainWindow::onRigBackendChangedR);
    connect(&dialog, &RigControlDialog::so2rChanged, this, &MainWindow::onToggleSo2r);
    connect(&dialog, &RigControlDialog::audioConfigChanged,
            this, &MainWindow::onAudioConfigChanged);
    // Release any live keyer serial port(s) while the dialog is open so its
    // "Detect keyer" probe can access them (serial ports are exclusive). We
    // reconnect from current settings after the dialog closes.
    if (m_winKeyerL) m_winKeyerL->closePort();
    if (m_winKeyerR) m_winKeyerR->closePort();

    dialog.exec();

    // Sync poll timer state with connection after dialog closes
    if (m_rigClient->isConnected() && !m_rigPollTimer->isActive()) {
        onRigConnected();
    } else if (!m_rigClient->isConnected() && m_rigPollTimer->isActive()) {
        onRigDisconnected();
    }
    if (m_so2rEnabled && m_rigClientR) {
        if (m_rigClientR->isConnected() && m_rigPollTimerR && !m_rigPollTimerR->isActive()) {
            onRigConnectedR();
        } else if (!m_rigClientR->isConnected() && m_rigPollTimerR && m_rigPollTimerR->isActive()) {
            onRigDisconnectedR();
        }
    }

    // Apply CW keyer configuration and reconnect (covers OK and Cancel alike,
    // and re-opens the port we released above).
    setupCwKeyers();
    updateCwConsoleRouting();
}

void MainWindow::onRigBackendChanged(const QString& backend)
{
    if (backend == m_rigBackend) return;

    DebugLogger::instance().log("MainWindow", QString("Rig backend changing from %1 to %2").arg(m_rigBackend).arg(backend));

    // Stop polling and disconnect existing client
    m_rigPollTimer->stop();
    if (m_rigClient->isConnected()) {
        m_rigClient->disconnectFromRig();
    }

    // Disconnect signals from old client
    disconnect(m_rigClient, nullptr, this, nullptr);

    // Delete old client and create new one
    m_rigClient->deleteLater();
    m_rigBackend = backend;

    if (backend == "hamlib") {
        m_rigClient = new HamlibClient(this);
    } else if (backend == "mocked") {
        m_rigClient = new MockedRigClient(this);
    } else {
        m_rigClient = new FlrigClient(this);
    }

    // Reconnect signals — use SIGNAL/SLOT macros for cross-class signal inheritance
    connect(m_rigClient, SIGNAL(connected()), this, SLOT(onRigConnected()));
    connect(m_rigClient, SIGNAL(disconnected()), this, SLOT(onRigDisconnected()));

    // Update CW window and TTS manager with new client (keyer routing follows
    // the active rig unless a WinKeyer is configured for this radio).
    updateCwConsoleRouting();
    m_ttsManager->setRigClient(m_rigClient);

    // Update rig status label (Radio R may still be connected)
    updateRigStatusLabel();

    // Auto-connect the new backend if it has auto-connect enabled
    Settings& settings = Settings::instance();
    bool autoConnect = false;
    QString host;
    int port = 0;
    if (backend == "hamlib") {
        autoConnect = settings.getHamlibAutoConnect();
        host = settings.getHamlibHost();
        port = settings.getHamlibPort();
    } else if (backend == "mocked") {
        autoConnect = settings.getMockedAutoConnect();
        host = "mocked";
    } else {
        autoConnect = settings.getFlrigAutoConnect();
        host = settings.getFlrigHost();
        port = settings.getFlrigPort();
    }
    if (autoConnect) {
        DebugLogger::instance().log("MainWindow", QString("Auto-connecting new %1 backend to %2:%3")
            .arg(backend).arg(host).arg(port));
        QTimer::singleShot(200, this, [this, host, port]() {
            if (m_rigClient->connectToRig(host, port)) {
                onRigConnected();
            }
        });
    }
}

void MainWindow::onRigConnected()
{
    DebugLogger::instance().log("MainWindow", "onRigConnected - starting rig poll timer");
    m_rigPollTimer->start();
    updateRigStatusLabel();
}

void MainWindow::onRigDisconnected()
{
    DebugLogger::instance().log("MainWindow", "onRigDisconnected - stopping rig poll timer");
    m_rigPollTimer->stop();
    updateRigStatusLabel();
}

void MainWindow::updateRstDefaults(const QString& oldMode, const QString& newMode,
                                    QMap<QString, QLineEdit*>& exchangeFields)
{
    auto rstFor = [](const QString& m) {
        return (m == "CW" || m == "RTTY" || m == "DIG") ? "599" : "59";
    };
    QString oldRst = rstFor(oldMode);
    QString newRst = rstFor(newMode);
    if (oldRst == newRst) return;

    for (auto it = exchangeFields.begin(); it != exchangeFields.end(); ++it) {
        if (it.key().contains("RST", Qt::CaseSensitive)) {
            // Only update if the field still has the old default (user hasn't edited it)
            if (it.value()->text().isEmpty() || it.value()->text() == oldRst)
                it.value()->setText(newRst);
        }
    }
}

void MainWindow::updateRigStatusLabel()
{
    bool lConnected = m_rigClient && m_rigClient->isConnected();
    bool rConnected = m_so2rEnabled && m_rigClientR && m_rigClientR->isConnected();

    if (m_so2rEnabled) {
        int count = (lConnected ? 1 : 0) + (rConnected ? 1 : 0);
        if (count == 2) {
            m_rigStatusLabel->setText("Rig: Connected (2 of 2)");
            m_rigStatusLabel->setStyleSheet("QLabel { color: green; }");
        } else if (count == 1) {
            m_rigStatusLabel->setText("Rig: Connected (1 of 2)");
            m_rigStatusLabel->setStyleSheet("QLabel { color: orange; }");
        } else {
            m_rigStatusLabel->setText("Rig: Disconnected");
            m_rigStatusLabel->setStyleSheet("QLabel { color: red; }");
        }
    } else {
        if (lConnected) {
            m_rigStatusLabel->setText("Rig: Connected");
            m_rigStatusLabel->setStyleSheet("QLabel { color: green; }");
        } else {
            m_rigStatusLabel->setText("Rig: Disconnected");
            m_rigStatusLabel->setStyleSheet("QLabel { color: red; }");
        }
    }
}

void MainWindow::onUpdateRigDisplay()
{
    if (!m_rigClient->isConnected()) {
        m_rigPollTimer->stop();
        return;
    }

    // Skip poll while TTS is active to avoid reentrant flrig socket use
    if (m_ttsManager && m_ttsManager->isActive())
        return;

    // Request frequency, mode, and WPM from rig
    // Catch errors to prevent UI lag when radio is off or unresponsive
    double freq = 0;
    QString mode;
    int wpm = 0;
    
    try {
        freq = m_rigClient->getFrequency();
        if (freq > 0) {
            mode = m_rigClient->getMode();
            wpm = m_rigClient->getCWSpeed();
        }
    } catch (...) {
        // Ignore errors - radio might be off or unresponsive
        return;
    }
    
    // even though this is mainwindow, this belongs in the Flrig filter
    if (DebugLogger::instance().isFlrigDebugEnabled()) {
        DebugLogger::instance().log("Flrig", QString("Rig poll: freq=%1 mode=%2 wpm=%3").arg(freq).arg(mode).arg(wpm));
    }
    
    // Update frequency if changed (100 Hz tolerance)
    if (freq > 0) {
        double freqKHz = freq / 1000.0;
        if (qAbs(freqKHz - m_lastFrequency) > 0.1) { // 100 Hz in kHz
            m_lastFrequency = freqKHz;
            DebugLogger::instance().log("MainWindow", QString("Updated frequency to %1 kHz").arg(freqKHz));
        }

        // Update band map VFO line
        if (m_bandMapWidget)
            m_bandMapWidget->setRigFrequency(freqKHz / 1000.0); // MHz

        // Detect band change — update band map range and clear spots
        if (m_contestEngine && m_bandMapWidget) {
            QString currentBand = m_contestEngine->getBandFromFrequency(freqKHz);
            if (!currentBand.isEmpty() && currentBand != m_lastBand) {
                m_lastBand = currentBand;
                // Retrieve band limits from contest definition
                double minMhz = 0.0, maxMhz = 0.0;
                if (m_contestDefinition.contains("frequencies")) {
                    QJsonObject freqs = m_contestDefinition["frequencies"].toObject();
                    if (freqs.contains(currentBand)) {
                        QJsonObject bandObj = freqs[currentBand].toObject();
                        minMhz = bandObj["start"].toDouble() / 1000.0; // kHz → MHz
                        maxMhz = bandObj["end"].toDouble() / 1000.0;
                    }
                }
                if (minMhz > 0.0 && maxMhz > minMhz)
                    m_bandMapWidget->setBandRange(minMhz, maxMhz, currentBand);
            }
        }
    }
    
    // Update mode if changed
    if (!mode.isEmpty() && mode != m_lastMode) {
        // Map common mode names
        QString mappedMode = mode;
        if (mode == "SSB" || mode == "PKTUSB") mappedMode = "USB";
        else if (mode == "PKTLSB") mappedMode = "LSB";
        else if (mode == "RTTY" || mode == "RTTYR") mappedMode = "DIG";

        QString oldMode = m_lastMode;
        m_lastMode = mappedMode;
        DebugLogger::instance().log("MainWindow", QString("Updated mode to %1").arg(mappedMode));

        // Refresh RST pre-fills if the RST default changed (CW/RTTY use 599, others 59)
        updateRstDefaults(oldMode, mappedMode, m_exchangeFields);
    }
    
    // Update WPM if changed and valid
    if (wpm > 0 && wpm != m_lastWpm) {
        m_lastWpm = wpm;
        m_wpmLabel->setText(QString("WPM: %1").arg(wpm));
    }
    
    // Update freq/mode button
    if (freq > 0) {
        m_freqModeButton->setText(QString("%1 %2")
            .arg(freq / 1000.0, 0, 'f', 1)
            .arg(!mode.isEmpty() ? mode : m_lastMode));
    }
}

void MainWindow::onFreqModeButtonClicked()
{
    // Determine which radio's button was clicked
    bool isRadioR = m_so2rEnabled && sender() == m_entryWidgetsR.freqModeButton;

    double& lastFreq = isRadioR ? m_lastFrequencyR : m_lastFrequency;
    QString& lastMode = isRadioR ? m_lastModeR : m_lastMode;
    RigInterface* rigClient = isRadioR ? m_rigClientR : m_rigClient;
    QPushButton* freqButton = isRadioR ? m_entryWidgetsR.freqModeButton : m_freqModeButton;
    QLineEdit* callField = isRadioR ? m_entryWidgetsR.callEdit : m_callEdit;

    // Show frequency/mode entry dialog
    FreqModeDialog dialog(this);
    dialog.setFrequency(lastFreq);
    dialog.setMode(lastMode);

    if (dialog.exec() == QDialog::Accepted) {
        double newFreq = dialog.frequency();
        QString newMode = dialog.mode();

        // Update local display and refresh RST defaults if mode changed
        QString oldMode = lastMode;
        lastFreq = newFreq;
        lastMode = newMode;
        auto& exchFields = isRadioR ? m_entryWidgetsR.exchangeFields : m_exchangeFields;
        updateRstDefaults(oldMode, newMode, exchFields);
        freqButton->setText(QString("%1 %2")
            .arg(newFreq, 0, 'f', 1)
            .arg(newMode));

        // Send to rig if connected
        if (rigClient && rigClient->isConnected()) {
            rigClient->setFrequency(newFreq * 1000.0);
            rigClient->setMode(newMode);
            m_statusLabel->setText(QString("Rig set to %1 kHz %2")
                .arg(newFreq, 0, 'f', 1)
                .arg(newMode));
        }

        // Return focus to call field for the correct radio
        if (callField) {
            callField->setFocus();
            callField->selectAll();
        }
    }
}

void MainWindow::updateMemoryTypeLabel()
{
    if (m_memoryTypeLabel)
        m_memoryTypeLabel->setText(m_useContestMemories ? "Contest Memories" : "Station Memories");
}

void MainWindow::loadCWMemories()
{
    QList<CwMemory> memories;
    if (m_useContestMemories) {
        memories = m_contestCwMemories;  // Empty list → all slots disabled; no station fallback
    } else {
        memories = Settings::instance().getCwMemories();
    }
    if (m_cwConsole) {
        m_cwConsole->setMemories(memories);
    }
    updateMemoryTypeLabel();
}

void MainWindow::loadSsbMemories()
{
    QList<SsbMemory> memories;
    if (m_useContestMemories) {
        memories = m_contestSsbMemories;  // Empty list → all slots disabled; no station fallback
    } else {
        memories = Settings::instance().getSsbMemories();
    }
    if (m_ssbMemoriesWidget) {
        m_ssbMemoriesWidget->setMemories(memories);
    }
}

void MainWindow::onEditCWMemories()
{
    Settings& settings = Settings::instance();

    CwMemoriesDialog dialog(this);
    dialog.setMemories(settings.getCwMemories());
    dialog.setContestMemories(m_contestCwMemories);
    dialog.setContestMode(m_useContestMemories);
    dialog.setSnPadding(settings.getCwSnPadding());
    dialog.setSnCutNumbers(settings.getCwSnCutNumbers());

    if (dialog.exec() == QDialog::Accepted) {
        QList<CwMemory> memories = dialog.getMemories();
        bool contestMode = dialog.isContestMode();

        if (contestMode) {
            m_contestCwMemories = memories;
            m_useContestMemories = true;
            m_isModified = true;
            updateWindowTitle();
        } else {
            settings.setCwMemories(memories);
            m_useContestMemories = false;
        }

        settings.setCwSnPadding(dialog.getSnPadding());
        settings.setCwSnCutNumbers(dialog.getSnCutNumbers());

        loadCWMemories();
        loadSsbMemories();
        m_statusLabel->setText("CW memories updated");
    }
}

void MainWindow::onEditSsbMemories()
{
    Settings& settings = Settings::instance();

    SsbMemoriesDialog dialog(this);
    dialog.setMemories(settings.getSsbMemories());
    dialog.setContestMemories(m_contestSsbMemories);
    dialog.setContestMode(m_useContestMemories);

    if (dialog.exec() == QDialog::Accepted) {
        QList<SsbMemory> memories = dialog.getMemories();
        bool contestMode = dialog.isContestMode();

        if (contestMode) {
            m_contestSsbMemories = memories;
            m_useContestMemories = true;
            m_isModified = true;
            updateWindowTitle();
        } else {
            settings.setSsbMemories(memories);
            m_useContestMemories = false;
        }

        loadCWMemories();
        loadSsbMemories();
        m_statusLabel->setText("SSB memories updated");
    }
}

void MainWindow::onWsjtxQsoReceived(const WsjtxQsoData& data)
{
    DebugLogger::instance().log("MainWindow",
        QString("WSJT-X QSO received: %1 on %2 Hz %3, RST='%4', exch='%5'")
            .arg(data.callsign).arg(data.frequencyHz).arg(data.mode, data.reportReceived, data.exchangeReceived));

    QLineEdit* callEdit = activeCallEdit();
    auto& exchFields = activeExchangeFields();

    // Set callsign
    callEdit->setText(data.callsign.toUpper());

    // Set frequency and mode from WSJT-X
    if (data.frequencyHz > 0) {
        double freqKhz = data.frequencyHz / 1000.0;
        m_lastFrequency = freqKhz;

        // Map WSJT-X mode names to contest digital mode
        QString mode = data.mode.toUpper();
        if (mode == "FT8" || mode == "FT4" || mode == "JT65" || mode == "JT9" ||
            mode == "WSPR" || mode == "MSK144" || mode == "Q65" || mode == "FST4") {
            mode = "DIGI";
        }
        QString oldMode = m_lastMode;
        m_lastMode = mode;

        // Update the frequency/mode button directly
        m_freqModeButton->setText(QString("%1 %2").arg(freqKhz, 0, 'f', 1).arg(mode));

        // Update RST defaults if mode changed (e.g., from SSB 59 to digital 599)
        if (oldMode != mode) {
            updateRstDefaults(oldMode, mode, activeExchangeFields());
        }
    }

    // Pre-fill exchange fields
    if (exchFields.contains("RSTs") && !data.reportSent.isEmpty()) {
        exchFields["RSTs"]->setText(data.reportSent);
    }
    if (exchFields.contains("RSTr") && !data.reportReceived.isEmpty()) {
        exchFields["RSTr"]->setText(data.reportReceived);
    }
    if (exchFields.contains("EXCHr") && !data.exchangeReceived.isEmpty()) {
        exchFields["EXCHr"]->setText(data.exchangeReceived.toUpper());
    }
    if (exchFields.contains("NAMEr") && !data.operatorName.isEmpty()) {
        exchFields["NAMEr"]->setText(data.operatorName.toUpper());
    }
    if (exchFields.contains("GRIDr") && !data.gridSquare.isEmpty()) {
        exchFields["GRIDr"]->setText(data.gridSquare.toUpper());
    }

    // Focus the call field so the operator can review and hit Enter to log
    callEdit->setFocus();
    callEdit->selectAll();

    m_statusLabel->setText(QString("WSJT-X: %1 %2 %3")
        .arg(data.callsign, data.mode,
             data.exchangeReceived.isEmpty() ? data.gridSquare : data.exchangeReceived));
}

void MainWindow::onSsbKeyingSetup()
{
    SsbKeyingSetupDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        DebugLogger::instance().log("MainWindow", "SSB keying setup updated");
        m_statusLabel->setText("SSB keying setup updated");
    }
}

static QString callsignToPhonetic(const QString& callsign)
{
    static const QMap<QChar, QString> phonetics = {
        {'A', "alpha"}, {'B', "bravo"}, {'C', "charlie"}, {'D', "delta"},
        {'E', "echo"}, {'F', "foxtrot"}, {'G', "golf"}, {'H', "hotel"},
        {'I', "india"}, {'J', "juliet"}, {'K', "kilo"}, {'L', "lima"},
        {'M', "mike"}, {'N', "november"}, {'O', "oscar"}, {'P', "papa"},
        {'Q', "quebec"}, {'R', "romeo"}, {'S', "sierra"}, {'T', "tango"},
        {'U', "uniform"}, {'V', "victor"}, {'W', "whiskey"}, {'X', "x-ray"},
        {'Y', "yankee"}, {'Z', "zulu"},
        {'0', "zero"}, {'1', "one"}, {'2', "two"}, {'3', "three"},
        {'4', "four"}, {'5', "five"}, {'6', "six"}, {'7', "seven"},
        {'8', "eight"}, {'9', "nine"},
    };

    QStringList words;
    for (const QChar& ch : callsign.toUpper()) {
        if (ch == '/') {
            words << "stroke";
        } else {
            auto it = phonetics.find(ch);
            if (it != phonetics.end())
                words << it.value();
        }
    }
    return words.join(" ");
}

void MainWindow::onSsbMemoryTriggered(int memoryNumber, const QString& text)
{
    DebugLogger::instance().log("MainWindow",
        QString("SSB Memory F%1 triggered: %2").arg(memoryNumber).arg(text));

    QLineEdit* callEdit = activeCallEdit();

    // Substitute macros in memory text
    QString expandedText = text;
    expandedText.replace("{CALL}", callsignToPhonetic(callEdit->text().trimmed()), Qt::CaseInsensitive);
    expandedText.replace("{MYCALL}", callsignToPhonetic(getSessionCallsign()), Qt::CaseInsensitive);

    // Serial number: {SNs}, {SN}, {serial} all resolve to next serial
    QString nextSerial = QString::number(m_qsoModel->count() + 1);
    expandedText.replace("{SNs}", nextSerial, Qt::CaseInsensitive);
    expandedText.replace("{SN}", nextSerial, Qt::CaseInsensitive);
    expandedText.replace("{serial}", nextSerial, Qt::CaseInsensitive);

    // Update status bar with expanded text
    m_statusLabel->setText(QString("SSB F%1: %2").arg(memoryNumber).arg(expandedText));

    // Trigger TTS if enabled
    Settings& settings = Settings::instance();
    if (settings.getSsbKeyingEnabled()) {
        DebugLogger::instance().log("MainWindow",
            QString("Starting TTS playback: %1").arg(expandedText));
        m_ttsManager->speak(expandedText);
    } else {
        DebugLogger::instance().log("MainWindow", "SSB keying disabled, skipping TTS");
    }

    // Return focus to call field of active radio
    callEdit->setFocus();
}

void MainWindow::onCwMemoryTriggered(int fKey, const QString& text)
{
    DebugLogger::instance().log("MainWindow",
        QString("CW Memory F%1 triggered: %2").arg(fKey + 1).arg(text));

    QLineEdit* callEdit = activeCallEdit();

    QString expandedText = text;
    expandedText.replace("{CALL}", callEdit->text().trimmed(), Qt::CaseInsensitive);
    expandedText.replace("{MYCALL}", getSessionCallsign(), Qt::CaseInsensitive);

    int serialNum = m_qsoModel->count() + 1;
    int padding = Settings::instance().getCwSnPadding();
    bool cutNumbers = Settings::instance().getCwSnCutNumbers();

    // Apply zero-padding (only pads when serial fits within the requested width)
    QString nextSerial = QString("%1").arg(serialNum, padding, 10, QChar('0'));

    // Apply cut numbers: 0→T, 9→N, 1→A (standard CW contest cut numbers)
    if (cutNumbers) {
        static const QMap<QChar, QChar> cuts = {
            {'0', 'T'}, {'9', 'N'}, {'1', 'A'}
        };
        QString cut;
        for (const QChar& c : nextSerial) {
            cut += cuts.value(c, c);
        }
        nextSerial = cut;
    }

    expandedText.replace("{SNs}", nextSerial, Qt::CaseInsensitive);
    expandedText.replace("{SN}", nextSerial, Qt::CaseInsensitive);
    expandedText.replace("{serial}", nextSerial, Qt::CaseInsensitive);

    DebugLogger::instance().log("MainWindow",
        QString("CW Memory expanded: %1").arg(expandedText));

    if (m_cwConsole) {
        m_cwConsole->sendCWText(expandedText);
    }

    // Return focus to call field of active radio
    callEdit->setFocus();
}

void MainWindow::onTtsFinished()
{
    DebugLogger::instance().log("MainWindow", "TTS playback finished");
    m_statusLabel->setText("TTS playback completed");
}

void MainWindow::onTtsError(const QString& error)
{
    DebugLogger::instance().log("MainWindow", QString("TTS error: %1").arg(error));
    m_statusLabel->setText(QString("TTS error: %1").arg(error));
    QMessageBox::warning(this, "TTS Error",
        QString("Voice keying failed:\n%1\n\nCheck your SSB Keying Setup (Rig menu).").arg(error));
}

void MainWindow::onRecalculateScore()
{
    DebugLogger::instance().log("MainWindow", 
        QString("onRecalculateScore: m_contestEngine=%1, m_contestDefinition.isEmpty()=%2")
        .arg(m_contestEngine ? "valid" : "null", m_contestDefinition.isEmpty() ? "true" : "false"));
    
    if (!m_contestEngine || m_contestDefinition.isEmpty()) {
        QMessageBox::warning(this, "No Contest", "No contest is currently loaded");
        return;
    }
    
    // Clear the score widget
    if (m_scoreWidget) {
        m_scoreWidget->resetScore();
    }
    
    // Get all QSOs, rescore them in one O(n) pass, then batch-update the model.
    QList<QsoRecord> allQsos = m_qsoModel->getAllQsos();
    QString myCallsign = getSessionCallsign();

    m_contestEngine->rescoreAll(allQsos, myCallsign);   // O(n) — no mid() copies
    m_qsoModel->replaceAll(allQsos);                    // single model reset, one repaint

    // Update score widget
    if (m_scoreWidget) {
        ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
        m_scoreWidget->updateScore(score);
    }
    updateSnapshotScore();
    updateSnapshotQsos();

    // Update multiplier widget
    if (m_multiplierWidget && m_multiplierDock) {
        QString multType = m_contestEngine->getMultiplierType();
        if (multType == "multsOnce")
            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMults());
        else if (multType == "multsPerBand")
            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBand());
        else if (multType == "multsPerMode")
            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerMode());
        else if (multType == "multsPerBandAndMode")
            m_multiplierWidget->updateWorkedMultipliers(m_contestEngine->getWorkedNamedMultsPerBandAndMode());
    }

    m_statusLabel->setText("Score recalculated");
    DebugLogger::instance().log("MainWindow", "Score recalculated for all QSOs");

    // Refresh band map spot statuses after rescore
    if (m_bandMapWidget)
        m_bandMapWidget->refreshAllStatuses([this](const QString &call) {
            return resolveSpotStatus(call);
        });
}

void MainWindow::onCWWindow()
{
    // CW console is now always visible in the right panel
    // This slot can just set focus to it
    if (m_cwConsole) {
        m_cwConsole->setFocus();
    }
}

void MainWindow::onToggleSsbMemoriesWidget(bool checked)
{
    if (m_ssbMemoriesWidget) {
        if (checked) {
            // Show the widget
            m_ssbMemoriesWidget->setVisible(true);

            // If it's floating, dock it back to the right panel
            if (m_ssbMemoriesWidget->isFloating()) {
                m_ssbMemoriesWidget->setFloating(false);
                addDockWidget(Qt::RightDockWidgetArea, m_ssbMemoriesWidget);
                DebugLogger::instance().log("MainWindow", "SSB Memories widget docked to right panel");
            } else {
                DebugLogger::instance().log("MainWindow", "SSB Memories widget shown (already docked)");
            }
        } else {
            // Hide the widget
            m_ssbMemoriesWidget->setVisible(false);
            DebugLogger::instance().log("MainWindow", "SSB Memories widget hidden");
        }
        savePanelState();
    }
}

void MainWindow::onToggleMultiplierWidget(bool checked)
{
    if (m_multiplierDock) {
        m_multiplierDock->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::onToggleRateWidget(bool checked)
{
    if (m_rateDock) {
        m_rateDock->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::installRedockOnMinimize(QDockWidget* dock)
{
    if (!dock) return;
    connect(dock, &QDockWidget::topLevelChanged, this, [this, dock](bool floating) {
        // Disconnect any previous QWindow visibility watcher for this dock
        auto it = m_floatingDockVisConn.find(dock);
        if (it != m_floatingDockVisConn.end()) {
            disconnect(*it);
            m_floatingDockVisConn.erase(it);
        }
        if (!floating) return;

        // Defer one tick so the native QWindow is fully created
        QTimer::singleShot(0, this, [this, dock]() {
            if (!dock->isFloating()) return;
            QWindow *win = dock->window()->windowHandle();
            if (!win) return;
            win->setFlag(Qt::WindowMaximizeButtonHint, false);
            m_floatingDockVisConn[dock] = connect(win, &QWindow::visibilityChanged,
                    this, [this, dock](QWindow::Visibility v) {
                if (v == QWindow::Minimized || v == QWindow::Maximized || v == QWindow::Hidden)
                    QTimer::singleShot(0, this, [dock]() { dock->setFloating(false); });
            });
        });
    });
}

void MainWindow::createBandMapDock()
{
    m_bandMapWidget = new BandMapWidget(this);
    m_bandMapWidget->setObjectName("bandMapDock");
    m_bandMapWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, m_bandMapWidget);
    m_bandMapWidget->hide();

    connect(m_bandMapWidget, &BandMapWidget::visibilityChanged, this, [this](bool visible) {
        if (m_bandMapWidgetAction)
            m_bandMapWidgetAction->setChecked(visible);
    });

    // Band map spot click → QSY (reuse existing onDxSpotClicked)
    connect(m_bandMapWidget, &BandMapWidget::spotClicked,
            this, &MainWindow::onDxSpotClicked);

    // DX cluster spots → band map
    connect(m_dxClusterPanel, &DxClusterPanel::spotReceived,
            this, &MainWindow::onSpotReceived);

    // Cluster connect/disconnect → band map indicator + clear on reconnect
    connect(m_dxClusterPanel, &DxClusterPanel::clusterConnectedChanged,
            this, [this](bool connected) {
        m_bandMapWidget->setClusterConnected(connected);
        if (connected)
            m_bandMapWidget->clearAllSpots();
    });
}

void MainWindow::onSpotReceived(const SpotData &spot)
{
    if (!m_bandMapWidget)
        return;
    SpotData resolved = spot;
    resolved.status = resolveSpotStatus(spot.callsign);
    m_bandMapWidget->addOrUpdateSpot(resolved);
}

ContactStatus MainWindow::resolveSpotStatus(const QString &callsign)
{
    if (!m_qsoModel)
        return ContactStatus::Unknown;

    const QList<QsoRecord> &qsos = m_qsoModel->getQsos();

    // Check if already worked (any band/mode)
    for (const QsoRecord &q : qsos) {
        if (q.getCall().compare(callsign, Qt::CaseInsensitive) == 0)
            return ContactStatus::Worked;
    }

    // Heuristic new-multiplier check: if DXCC entity of this spot has not been
    // worked by any QSO in the log, flag it as a potential new multiplier.
    if (m_dxccDatabase && m_dxccDatabase->isLoaded()) {
        DxccEntity spotEntity = m_dxccDatabase->lookupCallsign(callsign);
        if (spotEntity.dxcc > 0) {
            bool entityWorked = false;
            for (const QsoRecord &q : qsos) {
                DxccEntity qEntity = m_dxccDatabase->lookupCallsign(q.getCall());
                if (qEntity.dxcc == spotEntity.dxcc) {
                    entityWorked = true;
                    break;
                }
            }
            if (!entityWorked)
                return ContactStatus::NewMultiplier;
        }
    }

    return ContactStatus::UnworkedNonMult;
}

void MainWindow::onToggleBandMap(bool checked)
{
    if (m_bandMapWidget)
        m_bandMapWidget->setVisible(checked);
}

void MainWindow::onShowMultipliers()
{
    // Check if a contest is loaded
    if (m_contestDefinition.isEmpty()) {
        QMessageBox::information(this, "No Contest", "No contest is loaded. Please open a log or select a contest first.");
        return;
    }

    // Check if showMultiplierPanel is enabled in the contest definition
    bool panelEnabled = false;
    if (m_contestDefinition.contains("ui")) {
        QJsonObject ui = m_contestDefinition["ui"].toObject();
        panelEnabled = ui["showMultiplierPanel"].toBool(false);
    }

    if (!panelEnabled) {
        QMessageBox::information(this, "Multiplier Display",
            "Multiplier display is not enabled for this contest.");
        return;
    }

    // Show the dock and update the Window menu action
    if (m_multiplierDock) {
        m_multiplierDock->show();
    }
    if (m_multiplierWidgetAction) {
        m_multiplierWidgetAction->setChecked(true);
    }
    savePanelState();
}

void MainWindow::onToggleFlrigDebug(bool checked)
{
    DebugLogger::instance().setFlrigDebugEnabled(checked);
    Settings::instance().setFlrigDebugEnabled(checked);
    m_statusLabel->setText(checked ? "Flrig debug logging enabled" : "Flrig debug logging disabled");
}

void MainWindow::onToggleContestEngineDebug(bool checked)
{
    DebugLogger::instance().setContestEngineDebugEnabled(checked);
    Settings::instance().setContestEngineDebugEnabled(checked);
    m_statusLabel->setText(checked ? "ContestEngine debug logging enabled" : "ContestEngine debug logging disabled");
}

void MainWindow::onToggleContestSelectDialogDebug(bool checked)
{
    DebugLogger::instance().setContestSelectDialogDebugEnabled(checked);
    Settings::instance().setContestSelectDialogDebugEnabled(checked);
    m_statusLabel->setText(checked ? "ContestSelectDialog debug logging enabled" : "ContestSelectDialog debug logging disabled");
}

void MainWindow::onToggleDxccDatabaseDebug(bool checked)
{
    DebugLogger::instance().setDxccDatabaseDebugEnabled(checked);
    Settings::instance().setDxccDatabaseDebugEnabled(checked);
    m_statusLabel->setText(checked ? "DxccDatabase debug logging enabled" : "DxccDatabase debug logging disabled");
}

void MainWindow::onToggleDxClusterDebug(bool checked)
{
    DebugLogger::instance().setDxClusterDebugEnabled(checked);
    Settings::instance().setDxClusterDebugEnabled(checked);
    m_statusLabel->setText(checked ? "DX Cluster debug logging enabled" : "DX Cluster debug logging disabled");
}

void MainWindow::onToggleScpDebug(bool checked)
{
    DebugLogger::instance().setScpDebugEnabled(checked);
    Settings::instance().setScpDebugEnabled(checked);
    m_statusLabel->setText(checked ? "Super Check Partial debug logging enabled" : "Super Check Partial debug logging disabled");
}

void MainWindow::onToggleMultiplierWidgetDebug(bool checked)
{
    DebugLogger::instance().setMultiplierWidgetDebugEnabled(checked);
    Settings::instance().setMultiplierWidgetDebugEnabled(checked);
    m_statusLabel->setText(checked ? "Multiplier Widget debug logging enabled" : "Multiplier Widget debug logging disabled");
}

void MainWindow::onToggleCallsignLookupDebug(bool checked)
{
    DebugLogger::instance().setCallsignLookupDebugEnabled(checked);
    Settings::instance().setCallsignLookupDebugEnabled(checked);
    m_statusLabel->setText(checked ? "Callsign Lookup debug logging enabled" : "Callsign Lookup debug logging disabled");
}

void MainWindow::onShowDebugLogViewer()
{
    if (!m_debugLogViewer) {
        m_debugLogViewer = new DebugLogViewer(this);
        // Drop the pointer when the viewer is destroyed so we re-create
        // on the next invocation rather than dereferencing a stale one.
        m_debugLogViewer->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_debugLogViewer, &QObject::destroyed, this, [this]() {
            m_debugLogViewer = nullptr;
        });
    }
    m_debugLogViewer->show();
    m_debugLogViewer->raise();
    m_debugLogViewer->activateWindow();
}

void MainWindow::onOperatorCallDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Station Info"));
    dlg.setMinimumWidth(320);

    auto *form = new QFormLayout(&dlg);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto *note = new QLabel(
        "These settings override your default station preferences for this "
        "contest session only. Leave fields unchanged if your defaults are correct.", &dlg);
    note->setWordWrap(true);
    form->addRow(note);
    form->addRow(new QLabel("", &dlg));

    auto *callEdit    = new QLineEdit(m_sessionStationInfo->callsign(), &dlg);
    auto *nameEdit    = new QLineEdit(m_sessionStationInfo->operatorName(), &dlg);
    auto *gridEdit    = new QLineEdit(m_sessionStationInfo->grid(), &dlg);
    auto *stateEdit   = new QLineEdit(m_sessionStationInfo->state(), &dlg);
    auto *cqZoneEdit  = new QSpinBox(&dlg);
    auto *ituZoneEdit = new QSpinBox(&dlg);
    auto *arrlEdit    = new QLineEdit(m_sessionStationInfo->arrlSection(), &dlg);

    cqZoneEdit->setRange(0, 40);
    cqZoneEdit->setSpecialValueText("—");
    cqZoneEdit->setValue(m_sessionStationInfo->cqZone());
    ituZoneEdit->setRange(0, 90);
    ituZoneEdit->setSpecialValueText("—");
    ituZoneEdit->setValue(m_sessionStationInfo->ituZone());

    // Enforce uppercase on fields that need it
    auto forceUpper = [](QLineEdit *le) {
        QObject::connect(le, &QLineEdit::textChanged, le, [le](const QString& text) {
            QString upper = text.toUpper();
            if (upper != text) {
                int pos = le->cursorPosition();
                le->setText(upper);
                le->setCursorPosition(pos);
            }
        });
    };
    forceUpper(callEdit);
    forceUpper(gridEdit);
    forceUpper(stateEdit);
    forceUpper(arrlEdit);

    // Auto-populate CQ/ITU zone from DXCC database when callsign changes
    connect(callEdit, &QLineEdit::editingFinished, &dlg, [this, callEdit, cqZoneEdit, ituZoneEdit]() {
        QString call = callEdit->text().trimmed().toUpper();
        if (call.isEmpty() || !m_dxccDatabase) return;
        auto entity = m_dxccDatabase->lookupCallsign(call);
        if (entity.dxcc > 0) {
            if (cqZoneEdit->value() == 0) cqZoneEdit->setValue(entity.cqZone);
            if (ituZoneEdit->value() == 0) ituZoneEdit->setValue(entity.ituZone);
        }
    });

    form->addRow(tr("Callsign:"),       callEdit);
    form->addRow(tr("Operator name:"),  nameEdit);
    form->addRow(tr("Grid square:"),    gridEdit);
    form->addRow(tr("State/Province:"), stateEdit);
    form->addRow(tr("CQ Zone:"),        cqZoneEdit);
    form->addRow(tr("ITU Zone:"),       ituZoneEdit);
    form->addRow(tr("ARRL Section:"),   arrlEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    if (auto *btn = buttons->button(QDialogButtonBox::Ok))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    if (auto *btn = buttons->button(QDialogButtonBox::Cancel))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    m_sessionStationInfo->setCallsign(callEdit->text().trimmed().toUpper());
    m_sessionStationInfo->setOperatorName(nameEdit->text().trimmed());
    m_sessionStationInfo->setGrid(gridEdit->text().trimmed().toUpper());
    m_sessionStationInfo->setState(stateEdit->text().trimmed().toUpper());
    m_sessionStationInfo->setCqZone(cqZoneEdit->value());
    m_sessionStationInfo->setItuZone(ituZoneEdit->value());
    m_sessionStationInfo->setArrlSection(arrlEdit->text().trimmed().toUpper());

    DebugLogger::instance().log("MainWindow",
        QString("Station info updated: call=%1 name=%2 grid=%3 state=%4 cq=%5 itu=%6 arrl=%7")
            .arg(m_sessionStationInfo->callsign())
            .arg(m_sessionStationInfo->operatorName())
            .arg(m_sessionStationInfo->grid())
            .arg(m_sessionStationInfo->state())
            .arg(m_sessionStationInfo->cqZone())
            .arg(m_sessionStationInfo->ituZone())
            .arg(m_sessionStationInfo->arrlSection()));

    // Mark the log as modified so the updated info is persisted on next save
    m_isModified = true;
    updateWindowTitle();
}

void MainWindow::onContestSetup()
{
    DebugLogger::instance().log("MainWindow", 
        QString("onContestSetup: m_contestEngine=%1, m_contestDefinition.isEmpty()=%2")
        .arg(m_contestEngine ? "valid" : "null", m_contestDefinition.isEmpty() ? "true" : "false"));
    
    if (!m_contestEngine || m_contestDefinition.isEmpty()) {
        QMessageBox::warning(this, "No Contest", "No contest is currently loaded");
        return;
    }
    
    QJsonArray userPrompts = m_contestDefinition.value("userPrompts").toArray();
    if (userPrompts.isEmpty()) {
        QMessageBox::information(this, "Contest Setup", "This contest has no configurable settings.");
        return;
    }
    
    // Clear saved userPrompt responses from Settings
    QString contestFile = m_contestFile;
    if (contestFile.endsWith(".json")) {
        contestFile.remove(".json");
    }

    Settings::instance().clearContestUserPrompts(contestFile);
    Settings::instance().save();

    DebugLogger::instance().log("MainWindow",
        QString("Cleared userPrompt settings for contest: %1").arg(contestFile));
    
    // Now re-prompt for each userPrompt
    for (const QJsonValue& promptVal : userPrompts) {
        QJsonObject promptObj = promptVal.toObject();
        QString promptId = promptObj["id"].toString();
        QString question = promptObj["question"].toString();
        QString type = promptObj["type"].toString();
        bool required = promptObj["required"].toBool(false);
        
        if (type == "text") {
            QString value;
            bool forceUppercase = promptObj.value("forceUppercase").toBool(true);
            while (value.isEmpty() && required) {
                QDialog inputDialog(this);
                inputDialog.setWindowTitle("Contest Setup");
                QVBoxLayout layout(&inputDialog);
                
                QLabel label(question);
                QLineEdit edit;
                QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                
                layout.addWidget(&label);
                layout.addWidget(&edit);
                layout.addWidget(&buttonBox);
                
                connect(&buttonBox, &QDialogButtonBox::accepted, &inputDialog, &QDialog::accept);
                connect(&buttonBox, &QDialogButtonBox::rejected, &inputDialog, &QDialog::reject);
                
                // Apply real-time uppercase conversion
                connect(&edit, &QLineEdit::textChanged, [&edit, forceUppercase](const QString& text) {
                    QString filtered;
                    for (const QChar& c : text) {
                        if (c.isLetter() || c.isDigit() || c == '-' || c == '/') {
                            filtered += forceUppercase && c.isLetter() ? c.toUpper() : c;
                        }
                    }
                    if (filtered != text) {
                        int cursorPos = edit.cursorPosition();
                        edit.blockSignals(true);
                        edit.setText(filtered);
                        edit.setCursorPosition(cursorPos);
                        edit.blockSignals(false);
                    }
                });
                
                if (inputDialog.exec() == QDialog::Accepted) {
                    value = edit.text();
                    if (value.isEmpty() && required) {
                        QMessageBox::warning(this, "Input Required", 
                            question + " cannot be empty");
                        continue;
                    }
                } else {
                    return;
                }
            }
            
            // Store the value
            if (!value.isEmpty()) {
                Settings::instance().setContestUserPrompt(contestFile, promptId, value);
                Settings::instance().save();
                // Also update the contest engine
                if (m_contestEngine) {
                    m_contestEngine->setUserPromptValue(promptId, value);
                }
                DebugLogger::instance().log("MainWindow",
                    QString("Set userPrompt %1 = %2").arg(promptId, value));
            }
        } else if (type == "select") {
            QJsonArray options = promptObj["options"].toArray();
            QStringList optionLabels;
            QStringList optionValues;
            
            for (const QJsonValue& optVal : options) {
                QJsonObject opt = optVal.toObject();
                optionLabels.append(opt["label"].toString());
                optionValues.append(opt["value"].toString());
            }
            
            bool ok;
            QString selected = QInputDialog::getItem(this, "Contest Setup", question,
                optionLabels, 0, false, &ok);
            
            if (ok && !selected.isEmpty()) {
                int idx = optionLabels.indexOf(selected);
                if (idx >= 0) {
                    QString selectedValue = optionValues[idx];
                    Settings::instance().setContestUserPrompt(contestFile, promptId, selectedValue);
                    Settings::instance().save();
                    // Also update the contest engine
                    if (m_contestEngine) {
                        m_contestEngine->setUserPromptValue(promptId, selectedValue);
                        if (promptObj["restrictMode"].toBool(false)) {
                            applyRestrictedModeFromUserPrompts();
                        }
                    }
                    DebugLogger::instance().log("MainWindow",
                        QString("Set userPrompt %1 = %2").arg(promptId, selectedValue));
                }
            }
        }
    }
    
    // Refresh multiplier widget in case station type changed
    if (m_multiplierWidget && m_contestEngine && m_contestDefinition.contains("ui")) {
        QJsonObject ui = m_contestDefinition["ui"].toObject();
        if (ui["showMultiplierPanel"].toBool(false)) {
            m_multiplierWidget->setMultiplierList(m_contestEngine->getEffectiveNamedMultiplierList());
        }
    }

    // Recalculate score with new settings
    onRecalculateScore();

    QMessageBox::information(this, "Contest Setup", "Contest setup parameters have been updated.");
}

void MainWindow::onExportCabrillo()
{
    DebugLogger::instance().log("MainWindow", 
        QString("onExportCabrillo: m_contestEngine=%1, m_contestDefinition.isEmpty()=%2")
        .arg(m_contestEngine ? "valid" : "null", m_contestDefinition.isEmpty() ? "true" : "false"));
    
    if (!m_contestEngine || m_contestDefinition.isEmpty()) {
        QMessageBox::warning(this, "No Contest", "No contest is currently loaded");
        return;
    }
    
    if (m_qsoModel->rowCount() == 0) {
        QMessageBox::warning(this, "No QSOs", "No QSOs to export");
        return;
    }
    
    // Show Cabrillo header dialog
    CabrilloDialog dialog(this);
    dialog.setCallsign(getSessionCallsign());
    
    // Get the final contest score from the score widget (which is the only accurate representation)
    int totalScore = m_scoreWidget ? m_scoreWidget->getFinalScore() : 0;
    dialog.setClaimedScore(totalScore);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Ask where to save - use callsign as default filename
    QString defaultDir = QDir::homePath();
    QString callsign = getSessionCallsign();
    QString defaultFileName = callsign.isEmpty() ? "cabrillo.log" : callsign.toLower() + ".log";
    
    QString fileName = QFileDialog::getSaveFileName(this, "Export Cabrillo Log",
        defaultDir + "/" + defaultFileName, "Log Files (*.log);;Cabrillo Files (*.cbr *.cab);;Text Files (*.txt);;All Files (*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Export
    CabrilloFile exporter;
    QString myCallsign = getSessionCallsign();
    QString selectedMode = m_contestEngine->getStationClassMode();
    if (selectedMode.isEmpty())
        selectedMode = m_contestEngine->getUserPromptValue("contestMode");
    if (!exporter.exportToFile(fileName, m_qsoModel->getAllQsos(), m_contestDefinition, dialog.getHeaderData(), myCallsign, selectedMode)) {
        QMessageBox::critical(this, "Export Failed", "Failed to export Cabrillo log:\n" + exporter.lastError());
        return;
    }
    
    // Create a custom dialog with checkbox
    QDialog resultDialog(this);
    resultDialog.setWindowTitle("Export Successful");
    resultDialog.setMinimumWidth(400);
    
    QVBoxLayout layout(&resultDialog);
    
    QLabel messageLabel(QString("Cabrillo log exported to:\n%1").arg(fileName));
    layout.addWidget(&messageLabel);
    
    layout.addSpacing(10);
    
    QCheckBox viewCheckbox("View log file now");
    viewCheckbox.setChecked(true);  // Default to checked
    layout.addWidget(&viewCheckbox);
    
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok);
    layout.addWidget(&buttonBox);
    
    connect(&buttonBox, &QDialogButtonBox::accepted, &resultDialog, &QDialog::accept);
    
    resultDialog.exec();
    
    // If user checked the box, open the file with default text editor
    if (viewCheckbox.isChecked()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
    }
}

void MainWindow::onAbout()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About ContestLogX");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setTextInteractionFlags(Qt::TextBrowserInteraction);
    msgBox.setText(
        "<b>ContestLogX - Version 0.8.4 (Beta)</b><br><br>"
        "Cross-platform amateur radio contest logging software<br><br>"
        "Copyright &copy; 2025-2026, by Steve Woodruff, N9OH<br><br>"
        "<a href=\"https://contestlogx.com\">https://contestlogx.com</a><br><br>"
        "<a href=\"https://github.com/sjwoodr/ContestLogX\">"
        "https://github.com/sjwoodr/ContestLogX</a>"
        "<br><br>"
        "<b>Third-Party Libraries</b><br>"
        "This application uses Qt " + QString(qVersion()) + ", licensed under the "
        "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">GNU LGPL v3</a>.<br>"
        "Qt is a trademark of The Qt Company Ltd. "
        "Source: <a href=\"https://www.qt.io\">https://www.qt.io</a>"
    );
    msgBox.exec();
}

// ---------------------------------------------------------------------------
// Run / S&P helpers
// ---------------------------------------------------------------------------

void MainWindow::updateRunSPButtons()
{
    // Update Radio L buttons
    m_offButton->setChecked(m_runMode == RunMode::Off);
    m_runButton->setChecked(m_runMode == RunMode::Run);
    m_spButton->setChecked(m_runMode == RunMode::SP);

    // Visual emphasis: bold text on the active button
    QFont boldFont = m_offButton->font();
    boldFont.setBold(true);
    QFont normalFont = boldFont;
    normalFont.setBold(false);

    m_offButton->setFont(m_runMode == RunMode::Off ? boldFont : normalFont);
    m_runButton->setFont(m_runMode == RunMode::Run ? boldFont : normalFont);
    m_spButton->setFont(m_runMode == RunMode::SP  ? boldFont : normalFont);

    // Update Radio R buttons if SO2R is active
    if (m_so2rEnabled && m_entryWidgetsR.offButton) {
        m_entryWidgetsR.offButton->setChecked(m_runModeR == RunMode::Off);
        m_entryWidgetsR.runButton->setChecked(m_runModeR == RunMode::Run);
        m_entryWidgetsR.spButton->setChecked(m_runModeR == RunMode::SP);

        m_entryWidgetsR.offButton->setFont(m_runModeR == RunMode::Off ? boldFont : normalFont);
        m_entryWidgetsR.runButton->setFont(m_runModeR == RunMode::Run ? boldFont : normalFont);
        m_entryWidgetsR.spButton->setFont(m_runModeR == RunMode::SP  ? boldFont : normalFont);
    }

    if (m_statusLabel) {
        RunMode mode = activeRunMode();
        switch (mode) {
            case RunMode::Off: m_statusLabel->setText("Operating mode: OFF (Enter logs directly)"); break;
            case RunMode::Run: m_statusLabel->setText("Operating mode: RUN (Enter sequences CQ → Exchange → TU+Log)"); break;
            case RunMode::SP:  m_statusLabel->setText("Operating mode: S&P (Enter sequences My Call → Exchange+Log)"); break;
        }
    }
}

bool MainWindow::validateRunSPRoles(RunMode mode)
{
    if (mode == RunMode::Off) return true;

    QString currentMode = m_lastMode.toUpper();
    bool isCw = (currentMode == "CW" || currentMode == "CWR");
    QString memType = isCw ? "CW" : "SSB";

    QList<MemoryRole> required;
    QStringList requiredNames;
    if (mode == RunMode::Run) {
        required = { MemoryRole::MyCall, MemoryRole::CQ, MemoryRole::RunExchange, MemoryRole::TU };
        requiredNames = { "My Call", "CQ", "Run Exchange", "TU" };
    } else {
        required = { MemoryRole::MyCall, MemoryRole::SPExchange };
        requiredNames = { "My Call", "S&P Exchange" };
    }

    QStringList missing;
    for (int i = 0; i < required.size(); ++i) {
        if (findMemoryIndexByRole(required[i]) < 0)
            missing.append(requiredNames[i]);
    }

    if (missing.isEmpty()) return true;

    QString modeName = (mode == RunMode::Run) ? "Run" : "S&P";
    QString message = QString("%1 mode requires the following %2 memory role(s) to be assigned:\n\n  • %3\n\n"
                              "Opening the %2 Memories editor so you can assign them.")
                          .arg(modeName, memType, missing.join("\n  • "));

    DebugLogger::instance().log("MainWindow",
        QString("%1 mode activation blocked - missing %2 memory roles: %3")
        .arg(modeName, memType, missing.join(", ")));

    QMessageBox::warning(this, "Missing Memory Roles", message);

    if (isCw) onEditCWMemories();
    else      onEditSsbMemories();

    return false;
}

void MainWindow::onQsyBack()
{
    int totalQsos = m_qsoModel->count();
    if (totalQsos == 0) {
        m_statusLabel->setText("No QSOs to QSY back to");
        return;
    }

    // First press: start at last QSO; subsequent presses walk further back
    if (m_qsyBackIndex < 0)
        m_qsyBackIndex = 0;
    else
        m_qsyBackIndex++;

    if (m_qsyBackIndex >= totalQsos) {
        m_statusLabel->setText("No more QSOs to QSY back to");
        m_qsyBackIndex = totalQsos - 1;
        return;
    }

    // Walk from end of log
    QsoRecord qso = m_qsoModel->getQso(totalQsos - 1 - m_qsyBackIndex);
    double freqKhz = qso.getFrequency().toDouble();
    QString mode = qso.getMode();

    if (freqKhz <= 0) {
        m_statusLabel->setText("QSO has no frequency");
        return;
    }

    // Tune the active rig
    RigInterface* rig = (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_rigClientR : m_rigClient;
    if (rig && rig->isConnected()) {
        rig->setFrequency(freqKhz * 1000.0);
        if (!mode.isEmpty())
            rig->setMode(mode);
    }

    // Update local state and display
    m_lastFrequency = freqKhz;
    if (!mode.isEmpty())
        m_lastMode = mode;
    m_freqModeButton->setText(QString("%1 %2").arg(freqKhz, 0, 'f', 1).arg(mode));

    m_statusLabel->setText(QString("QSY Back: %1 kHz %2 (QSO #%3)")
        .arg(freqKhz, 0, 'f', 1).arg(mode).arg(totalQsos - m_qsyBackIndex));

    DebugLogger::instance().log("MainWindow",
        QString("QSY Back to %1 kHz %2 (index %3, QSO #%4)")
            .arg(freqKhz, 0, 'f', 1).arg(mode).arg(m_qsyBackIndex).arg(totalQsos - m_qsyBackIndex));
}

void MainWindow::onToggleRunSP()
{
    // Cycle: Off → Run → SP → Off — applies to the active radio
    RunMode& mode = (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_runModeR : m_runMode;
    RunMode next;
    if (mode == RunMode::Off)      next = RunMode::Run;
    else if (mode == RunMode::Run) next = RunMode::SP;
    else                           next = RunMode::Off;

    if (!validateRunSPRoles(next)) return;
    mode = next;
    updateRunSPButtons();
}

void MainWindow::onToggleMemoryType()
{
    m_useContestMemories = !m_useContestMemories;
    loadCWMemories();
    loadSsbMemories();
    m_isModified = true;
    updateWindowTitle();

    DebugLogger::instance().log("MainWindow",
        QString("Memory type toggled to %1").arg(m_useContestMemories ? "Contest" : "Station"));
}

int MainWindow::findMemoryIndexByRole(MemoryRole role) const
{
    const QList<CwMemory>& cwMems = m_useContestMemories ? m_contestCwMemories
                                                         : Settings::instance().getCwMemories();
    const QList<SsbMemory>& ssbMems = m_useContestMemories ? m_contestSsbMemories
                                                           : Settings::instance().getSsbMemories();

    QString mode = m_lastMode.toUpper();
    bool isCw = (mode == "CW" || mode == "CWR");

    if (isCw) {
        for (int i = 0; i < cwMems.size(); ++i)
            if (cwMems[i].role == role) return i;
    } else {
        for (int i = 0; i < ssbMems.size(); ++i)
            if (ssbMems[i].role == role) return i;
    }
    return -1;
}

void MainWindow::triggerMemoryByRole(MemoryRole role)
{
    int idx = findMemoryIndexByRole(role);
    if (idx < 0) {
        DebugLogger::instance().log("MainWindow",
            QString("No memory found for role: %1").arg(memoryRoleToString(role)));
        return;
    }

    QString mode = m_lastMode.toUpper();
    if (mode == "CW" || mode == "CWR") {
        if (m_cwConsole)
            m_cwConsole->onMemoryButton(idx);
    } else {
        if (m_ssbMemoriesWidget)
            m_ssbMemoriesWidget->triggerMemory(idx);
    }
}

void MainWindow::onQsoEntryReturn()
{
    QString call = activeCallEdit()->text().trimmed();
    auto& exchFields = activeExchangeFields();
    bool hasExchange = false;
    for (auto* field : exchFields.values()) {
        if (!field->text().trimmed().isEmpty()) {
            hasExchange = true;
            break;
        }
    }

    RunMode runMode = activeRunMode();
    bool& exchangeSent = (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_exchangeSentR : m_exchangeSent;

    if (runMode == RunMode::Off) {
        onLogQso();
        return;
    }

    if (runMode == RunMode::Run) {
        if (call.isEmpty()) {
            // Step 1: call box empty → send CQ
            triggerMemoryByRole(MemoryRole::CQ);
        } else if (!exchangeSent) {
            // Step 2: call filled, exchange not yet sent → send exchange
            triggerMemoryByRole(MemoryRole::RunExchange);
            exchangeSent = true;
        } else {
            // Step 3: exchange was sent → send TU and log
            triggerMemoryByRole(MemoryRole::TU);
            onLogQso();
        }
    } else {
        // S&P mode
        if (!exchangeSent) {
            // Step 1: call filled → send my call; ignore Enter if call is empty
            if (call.isEmpty()) return;
            triggerMemoryByRole(MemoryRole::MyCall);
            exchangeSent = true;
        } else if (hasExchange) {
            // Step 2: exchange filled → send our exchange then log
            triggerMemoryByRole(MemoryRole::SPExchange);
            onLogQso();
        } else {
            // Exchange field still empty — log directly (operator logged manually)
            onLogQso();
        }
    }
}

// ---------------------------------------------------------------------------

void MainWindow::clearEntryForm()
{
    // Clear all exchange fields and reset defaults
    for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
        QString fieldName = it.key();
        QLineEdit* edit = it.value();
        
        // Reset RST fields to defaults based on mode
        if (fieldName.contains("RST", Qt::CaseSensitive)) {
            QString defaultRst = (m_lastMode == "CW" || m_lastMode == "RTTY") ? "599" : 
                                (m_lastMode.contains("DIGI")) ? "+0" : "59";
            edit->setText(defaultRst);
        } else {
            edit->clear();
        }
    }
    
    m_callEdit->clear();
    m_callEdit->setFocus();
    m_exchangeSent = false;
}

void MainWindow::preSaveCall()
{
    QString callsign = activeCallEdit()->text().trimmed().toUpper();
    
    if (callsign.isEmpty()) {
        m_statusLabel->setText("No callsign entered");
        return;
    }
    
    if (!CallHistory::instance().isEnabled()) {
        m_statusLabel->setText("Call history is disabled");
        return;
    }
    
    // Get fields to save from contest definition
    QStringList fieldsToSave;
    if (m_contestEngine) {
        fieldsToSave = m_contestEngine->getCallHistoryFieldsToSave();
    } else {
        fieldsToSave << "CALL" << "EXCHr";
    }
    
    // Build a map of exchange fields to save
    QMap<QString, QString> historyFields;
    QMap<QString, QString> exchangeFields = getExchangeFieldsForQso();
    
    // Only save fields specified in contest definition
    for (const QString& fieldName : fieldsToSave) {
        if (fieldName == "CALL") {
            historyFields["CALL"] = callsign;
        } else {
            QString value = exchangeFields.value(fieldName);
            if (!value.isEmpty()) {
                historyFields[fieldName] = value;
            }
        }
    }
    
    // Add or update the record in call history
    if (!historyFields.isEmpty()) {
        CallHistory::instance().addOrUpdateRecord(callsign, historyFields);
        CallHistory::instance().save();
        
        m_statusLabel->setText(QString("Saved %1 to call history (will continue working on refresh)").arg(callsign));
        DebugLogger::instance().log("MainWindow", 
            QString("Pre-saved %1 to call history with fields: %2").arg(callsign).arg(fieldsToSave.join(", ")));
    } else {
        m_statusLabel->setText(QString("No exchange fields to save for %1").arg(callsign));
    }
}

QMap<QString, QString> MainWindow::getExchangeFieldsForQso()
{
    QMap<QString, QString> result;
    
    // Collect all exchange field values from the UI
    for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
        QString fieldName = it.key();
        QString value = it.value()->text().trimmed().toUpper();
        if (!value.isEmpty()) {
            result[fieldName] = value;
        }
    }
    
    return result;
}

QString MainWindow::getDupeQsoDetails(const QString& callsign, const QList<QsoRecord>& allQsos)
{
    // Find the QSO with this callsign and return its details
    for (int i = allQsos.count() - 1; i >= 0; --i) {
        if (allQsos[i].getCall().toUpper() == callsign.toUpper()) {
            const QsoRecord& qso = allQsos[i];
            
            // Get the serial number (QSO #)
            int qsoNumber = i + 1;  // 1-based index
            
            // Get time in HHmm format from the QDateTime
            QDateTime dateTime = qso.getDateTime();
            QString time = dateTime.toString("hhmm");
            
            // Get frequency
            QString freq = qso.getFrequency();
            
            // Get mode
            QString mode = qso.getMode();
            
            return QString("QSO #%1 @ %2 %3 %4").arg(qsoNumber).arg(time).arg(freq).arg(mode);
        }
    }
    
    return "";
}


void MainWindow::updateWindowTitle()
{
    QString title = QString("ContestLogX v%1").arg(QApplication::applicationVersion());
    
    if (!m_currentFile.isEmpty()) {
        title += " - " + QFileInfo(m_currentFile).fileName();
    } else {
        title += " - [Untitled]";
    }
    
    if (m_isModified) {
        title += " *";
    }
    
    setWindowTitle(title);
}

void MainWindow::updateCallHistory()
{
    QList<QsoRecord> qsos = m_qsoModel->getQsos();
    
    // Get fields to save from contest definition, or use defaults
    QStringList fieldsToSave;
    if (m_contestEngine) {
        fieldsToSave = m_contestEngine->getCallHistoryFieldsToSave();
    } else {
        // Default: CALL and EXCHr
        fieldsToSave << "CALL" << "EXCHr";
    }
    
    for (const QsoRecord& qso : qsos) {
        QString callsign = qso.getCall();
        if (callsign.isEmpty()) continue;
        
        // Build a map of exchange fields to save based on contest definition
        QMap<QString, QString> historyFields;
        
        // Get all exchange fields from the QSO
        QMap<QString, QString> exchangeFields = qso.getExchangeFields();
        
        // Only save fields specified in contest definition (or defaults)
        for (const QString& fieldName : fieldsToSave) {
            if (fieldName == "CALL") {
                // CALL is always the callsign itself
                historyFields["CALL"] = callsign;
            } else {
                // Look for the field in the exchange fields
                QString value = exchangeFields.value(fieldName);
                if (!value.isEmpty()) {
                    historyFields[fieldName] = value;
                }
            }
        }
        
        // Add to call history if we have any fields
        if (!historyFields.isEmpty()) {
            CallHistory::instance().addOrUpdateRecord(callsign, historyFields);
        }
    }
    
    CallHistory::instance().save();
    DebugLogger::instance().log("MainWindow", 
        QString("Updated call history with %1 records (fields: %2)").arg(qsos.size()).arg(fieldsToSave.join(", ")));
}

bool MainWindow::maybeSave()
{
    if (!m_isModified)
        return true;
    
    QMessageBox::StandardButton ret = QMessageBox::warning(this,
        "ContestLogX",
        "The log has been modified.\nDo you want to save your changes?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    
    if (ret == QMessageBox::Save) {
        onSaveLog();
        return !m_isModified;  // Return true only if save succeeded
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }

    // Discard — remove crash backup so it doesn't reappear next session
    removeBackup();
    return true;
}

void MainWindow::saveWindowGeometry()
{
    Settings& settings = Settings::instance();

    // Log current window state before saving
    DebugLogger::instance().log("MainWindow",
        QString("Before saveGeometry: pos()=(%1,%2) framePos=(%3,%4) geometry=(%5,%6,%7x%8) frameGeometry=(%9,%10,%11x%12)")
        .arg(pos().x()).arg(pos().y())
        .arg(frameGeometry().x()).arg(frameGeometry().y())
        .arg(geometry().x()).arg(geometry().y()).arg(geometry().width()).arg(geometry().height())
        .arg(frameGeometry().x()).arg(frameGeometry().y()).arg(frameGeometry().width()).arg(frameGeometry().height()));

    // Use Qt's built-in saveGeometry() which properly handles window manager interactions
    QByteArray geomState = QWidget::saveGeometry();
    settings.setWindowGeometryState(geomState);
    settings.setWindowMaximized(isMaximized());

    DebugLogger::instance().log("MainWindow",
        QString("Saved window geometry state (size=%1 bytes), maximized=%2")
        .arg(geomState.size()).arg(isMaximized()));

    // Write to disk
    settings.save();
}

void MainWindow::restoreWindowGeometry()
{
    Settings& settings = Settings::instance();

    // Compute a safe fallback: 50% of the current screen's available rect,
    // centered. Used whenever Qt's restoreGeometry() fails or no saved
    // state exists, so we never open off-screen or larger than the desktop.
    auto safeFallbackRect = [this]() {
        QScreen* scr = screen();
        if (!scr) scr = QGuiApplication::primaryScreen();
        QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1024, 768);
        int w = static_cast<int>(avail.width() * 0.50);
        int h = static_cast<int>(avail.height() * 0.50);
        int x = avail.x() + (avail.width() - w) / 2;
        int y = avail.y() + (avail.height() - h) / 2;
        return QRect(x, y, w, h);
    };

    // Try Qt's built-in geometry restore first (handles window managers better)
    QByteArray savedGeometry = settings.getWindowGeometryState();
    if (!savedGeometry.isEmpty()) {
        DebugLogger::instance().log("MainWindow",
            QString("Restoring window geometry from saved state (size=%1 bytes)")
            .arg(savedGeometry.size()));
        bool restored = QWidget::restoreGeometry(savedGeometry);
        DebugLogger::instance().log("MainWindow",
            QString("restoreGeometry() returned: %1, pos after restore=(%2,%3)")
            .arg(restored ? "true" : "false").arg(pos().x()).arg(pos().y()));

        if (!restored) {
            QRect safe = safeFallbackRect();
            DebugLogger::instance().log("MainWindow",
                QString("restoreGeometry() failed, using 50%% safe fallback: pos=(%1,%2) size=(%3x%4)")
                .arg(safe.x()).arg(safe.y()).arg(safe.width()).arg(safe.height()));
            resize(safe.width(), safe.height());
            move(safe.topLeft());
        }
    } else {
        // No saved state — open at 50% of available screen, centered.
        QRect safe = safeFallbackRect();
        DebugLogger::instance().log("MainWindow",
            QString("No saved geometry state, using 50%% safe fallback: pos=(%1,%2) size=(%3x%4)")
            .arg(safe.x()).arg(safe.y()).arg(safe.width()).arg(safe.height()));
        setGeometry(safe);
    }

    if (settings.getWindowMaximized()) {
        showMaximized();
    }
}

void MainWindow::onColumnResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(oldSize);
    
    // Save the new column width
    Settings& settings = Settings::instance();
    settings.setColumnWidth(logicalIndex, newSize);
}

void MainWindow::restoreColumnWidths()
{
    Settings& settings = Settings::instance();
    QMap<int, int> widths = settings.getColumnWidths();
    
    for (auto it = widths.constBegin(); it != widths.constEnd(); ++it) {
        int column = it.key();
        int width = it.value();
        if (column < m_qsoTable->horizontalHeader()->count()) {
            m_qsoTable->setColumnWidth(column, width);
        }
    }
}

void MainWindow::onPropagationDataReceived(int sfi, int aIndex, int kIndex)
{
    // DX cluster propagation data — only use as fallback if NOAA hasn't provided data
    if (m_noaaPropagationReceived) return;

    QString propText = QString("SFI %1  A %2  K %3").arg(sfi).arg(aIndex).arg(kIndex);
    m_propagationLabel->setText(propText);
}

void MainWindow::fetchNoaaPropagation()
{
    if (!m_noaaNetworkManager) return;
    QNetworkRequest request(QUrl("https://services.swpc.noaa.gov/text/wwv.txt"));
    request.setTransferTimeout(10000);
    m_noaaNetworkManager->get(request);
}

void MainWindow::onNoaaPropagationReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        DebugLogger::instance().log("MainWindow",
            QString("NOAA propagation fetch failed: %1").arg(reply->errorString()));
        return;
    }

    QString data = QString::fromUtf8(reply->readAll());

    // Parse: "Solar flux 142 and estimated planetary A-index 10."
    // Parse: "The estimated planetary K-index at 2100 UTC on 01 April was 1.67."
    int sfi = 0, aIndex = 0, kIndex = 0;

    QRegularExpression sfiRegex("Solar flux (\\d+)");
    QRegularExpressionMatch sfiMatch = sfiRegex.match(data);
    if (sfiMatch.hasMatch())
        sfi = sfiMatch.captured(1).toInt();

    QRegularExpression aRegex("A-index (\\d+)");
    QRegularExpressionMatch aMatch = aRegex.match(data);
    if (aMatch.hasMatch())
        aIndex = aMatch.captured(1).toInt();

    QRegularExpression kRegex("K-index.*was ([\\d.]+)");
    QRegularExpressionMatch kMatch = kRegex.match(data);
    if (kMatch.hasMatch())
        kIndex = qRound(kMatch.captured(1).toDouble());

    if (sfi > 0) {
        m_noaaPropagationReceived = true;
        QString propText = QString("SFI %1  A %2  K %3").arg(sfi).arg(aIndex).arg(kIndex);
        m_propagationLabel->setText(propText);
        DebugLogger::instance().log("MainWindow",
            QString("NOAA propagation: SFI=%1 A=%2 K=%3").arg(sfi).arg(aIndex).arg(kIndex));

        // Mirror into the Remote Control snapshot so the dashboard can surface it.
        if (m_clxSnapshot) {
            clx::net::PropagationSnapshot ps;
            ps.sfi        = sfi;
            ps.aIndex     = aIndex;
            ps.kIndex     = kIndex;
            ps.fetchedAt  = QDateTime::currentDateTimeUtc();
            m_clxSnapshot->setPropagation(ps);
        }
    }
}

void MainWindow::onSpotLastQso()
{
    // Get the last logged QSO
    if (m_qsoModel->count() == 0) {
        DebugLogger::instance().log("MainWindow", "Spot Last QSO: No QSOs logged yet");
        QMessageBox::information(this, "Spot Last QSO", "No QSOs have been logged yet.");
        return;
    }

    // Get last QSO (most recent is at the end)
    int lastIndex = m_qsoModel->count() - 1;
    QsoRecord lastQso = m_qsoModel->getQso(lastIndex);

    QString callsign = lastQso.getCall();
    double freqKhz = lastQso.getFrequency().toDouble();

    DebugLogger::instance().log("MainWindow", QString("Spot Last QSO: call=%1 freq=%2 kHz").arg(callsign).arg(freqKhz));

    // Populate the DX cluster command field
    if (m_dxClusterPanel) {
        m_dxClusterPanel->setSpotCommand(callsign, freqKhz);
    }
}

void MainWindow::onDxSpotClicked(const QString& callsign, double frequency, const QString& mode)
{
    DebugLogger::instance().log("MainWindow", QString("DX spot clicked: call=%1, changing rig to %2 kHz, mode %3").arg(callsign).arg(frequency).arg(mode));

    // Set callsign in active radio's QSO entry field
    activeCallEdit()->setText(callsign.toUpper());

    // Reset all exchange fields except CALL — restore RST defaults rather than leaving them blank
    auto& exchFields = activeExchangeFields();
    for (auto it = exchFields.begin(); it != exchFields.end(); ++it) {
        const QString& fieldName = it.key();
        if (fieldName == "CALL") continue;
        if (fieldName.contains("RST", Qt::CaseSensitive)) {
            QString curMode = activeMode();
            QString defaultRst = (curMode == "CW" || curMode == "RTTY") ? "599" :
                                 (curMode.contains("DIGI")) ? "+0" : "59";
            it.value()->setText(defaultRst);
        } else {
            it.value()->clear();
        }
    }

    // Check for dupe with the clicked spot's frequency and mode
    QsoRecord tempQso;
    tempQso.setCall(callsign.toUpper());
    tempQso.setFrequency(QString::number(frequency, 'f', 1));
    tempQso.setMode(mode);
    
    // Set band from the clicked spot's frequency
    QString band = m_contestEngine->getBandFromFrequency(frequency);
    if (!band.isEmpty()) {
        tempQso.setBandName(band);
    }
    
     QList<QsoRecord> allQsos = m_qsoModel->getQsos();
    if (m_contestEngine && m_contestEngine->isDupe(tempQso, allQsos)) {
        QString dupeDetails = getDupeQsoDetails(callsign.toUpper(), allQsos);
        QString message = "<span style='color: red;'>⚠</span> DUPE: " + callsign.toUpper();
        if (!dupeDetails.isEmpty()) {
            message += " (" + dupeDetails + ")";
        }
        m_statusLabel->setText(message);
        DebugLogger::instance().log("MainWindow", QString("DUPE DETECTED for %1 from DX spot").arg(callsign));
        flashDupeWarning();
    } else {
        m_statusLabel->setText("Ready");
    }
    
    activeCallEdit()->setFocus();

    // Update active radio's local display immediately
    if (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) {
        m_lastFrequencyR = frequency;
        if (!mode.isEmpty()) m_lastModeR = mode;
        if (m_entryWidgetsR.freqModeButton)
            m_entryWidgetsR.freqModeButton->setText(QString("%1 %2").arg(frequency, 0, 'f', 1).arg(m_lastModeR));
    } else {
        m_lastFrequency = frequency;
        if (!mode.isEmpty()) m_lastMode = mode;
        m_freqModeButton->setText(QString("%1 %2").arg(frequency, 0, 'f', 1).arg(m_lastMode));
    }

    // Change active rig frequency and mode
    RigInterface* rig = activeRigClient();
    if (rig && rig->isConnected()) {
        rig->setFrequency(static_cast<long>(frequency * 1000)); // Convert kHz to Hz
        rig->setMode(mode);
    }
}

void MainWindow::flashDupeWarning()
{
    // Flash the QSO entry widget red for dupe warning
    if (m_qsoEntryGroup) {
        QPalette palette = m_qsoEntryGroup->palette();
        palette.setColor(QPalette::Window, QColor(255, 0, 0));
        m_qsoEntryGroup->setPalette(palette);
        m_qsoEntryGroup->setAutoFillBackground(true);
        m_dupeFlashTimer->start(500);  // Flash for 500ms
    }
}

void MainWindow::onDupeFlashTimeout()
{
    // Restore normal background
    if (m_qsoEntryGroup) {
        m_qsoEntryGroup->setPalette(qApp->palette());
        m_qsoEntryGroup->setAutoFillBackground(false);
    }
    m_dupeFlashTimer->stop();
}

void MainWindow::initCallsignLookup()
{
    Settings& s = Settings::instance();
    QString service = s.getCallsignLookupService();

    if (service == "qrzcq") {
        QString user = s.getQrzcqUsername();
        QString pass = s.getQrzcqPassword();
        if (!user.isEmpty() && !pass.isEmpty()) {
            m_qrzcqApi->setCredentials(user, pass);
            m_qrzcqApi->getSession();
        }
    } else if (service == "qrz") {
        QString user = s.getQrzUsername();
        QString pass = s.getQrzPassword();
        if (!user.isEmpty() && !pass.isEmpty()) {
            m_qrzApi->setCredentials(user, pass);
            m_qrzApi->getSession();
        }
    }
    // "none" → do nothing

    // Update lookup button tooltip
    if (m_qrzButton) {
        if (service == "qrz")
            m_qrzButton->setToolTip("Look up callsign on QRZ.com");
        else
            m_qrzButton->setToolTip("Look up callsign on QRZCQ.com");
    }
}

void MainWindow::triggerAutoLookup(const QString& callsign)
{
    QString service = Settings::instance().getCallsignLookupService();
    if (service == "qrzcq" && m_qrzcqApi->hasValidSession())
        m_qrzcqApi->lookupCallsign(callsign);
    else if (service == "qrzcq" && Settings::instance().getQrzcqAutoLookupEnabled())
        m_qrzcqApi->lookupCallsign(callsign);  // scraping fallback
    else if (service == "qrz" && m_qrzApi->hasValidSession())
        m_qrzApi->lookupCallsign(callsign);
}

void MainWindow::applyPendingStationInfo(QsoRecord& qso, const QString& callsign)
{
    if (m_pendingLookupCallsign.isEmpty() || m_pendingLookupCallsign != callsign)
        return;
    for (auto it = m_pendingStationInfo.constBegin(); it != m_pendingStationInfo.constEnd(); ++it)
        qso.setStationInfo(it.key(), it.value());
    DebugLogger::instance().log("CallsignLookup",
        QString("Applied station info from lookup to QSO with %1").arg(callsign));
}

void MainWindow::onQrzcqSessionObtained(const QString& token)
{
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZCQ session obtained: %1...").arg(token.left(10)));
}

void MainWindow::onQrzcqSessionError(const QString& error)
{
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZCQ session error: %1").arg(error));
}

void MainWindow::onQrzcqCallsignFound(const QrzcqCallsignData& data)
{
    // Cache station info for when the QSO is logged
    m_pendingLookupCallsign = data.call.toUpper();
    m_pendingStationInfo.clear();
    m_pendingStationInfo["NAME"]       = data.name;
    m_pendingStationInfo["QTH"]        = data.city.isEmpty() ? data.qth : data.city;
    m_pendingStationInfo["GRIDSQUARE"] = data.locator;
    m_pendingStationInfo["COUNTRY"]    = data.country;
    m_pendingStationInfo["CONT"]       = data.continent;
    if (data.dxcc > 0) m_pendingStationInfo["DXCC"] = QString::number(data.dxcc);
    if (data.cq  > 0) m_pendingStationInfo["CQZ"]  = QString::number(data.cq);
    if (data.itu > 0) m_pendingStationInfo["ITUZ"] = QString::number(data.itu);

    // Status bar: call name city/qth country
    QString status = data.call;
    if (!data.name.isEmpty())   status += " " + data.name;
    if (!data.city.isEmpty())   status += " " + data.city;
    else if (!data.qth.isEmpty()) status += " " + data.qth;
    if (!data.country.isEmpty()) status += " " + data.country;
    m_statusLabel->setText(status);

    DebugLogger::instance().log("CallsignLookup",
        QString("QRZCQ lookup found: %1 (grid=%2 dxcc=%3 cq=%4 itu=%5 cont=%6)")
            .arg(status).arg(data.locator).arg(data.dxcc)
            .arg(data.cq).arg(data.itu).arg(data.continent));
}

void MainWindow::onQrzcqCallsignNotFound(const QString& callsign)
{
    m_pendingLookupCallsign.clear();
    m_pendingStationInfo.clear();
    m_statusLabel->setText(QString("Callsign %1 not found in QRZCQ").arg(callsign));
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZCQ lookup: %1 not found").arg(callsign));
}

void MainWindow::onQrzcqLookupError(const QString& error)
{
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZCQ lookup error: %1").arg(error));
}

// ----- QRZ.com handlers -----

void MainWindow::onQrzSessionObtained(const QString& token)
{
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZ session obtained: %1...").arg(token.left(10)));
}

void MainWindow::onQrzSessionError(const QString& error)
{
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZ session error: %1").arg(error));
}

void MainWindow::onQrzCallsignFound(const QrzCallsignData& data)
{
    // Build full name from first + last
    QString fullName = data.fname;
    if (!data.name.isEmpty())
        fullName = fullName.isEmpty() ? data.name : fullName + " " + data.name;

    // Cache station info for when the QSO is logged
    m_pendingLookupCallsign = data.call.toUpper();
    m_pendingStationInfo.clear();
    m_pendingStationInfo["NAME"]       = fullName;
    m_pendingStationInfo["QTH"]        = data.addr2;   // city
    m_pendingStationInfo["STATE"]      = data.state;
    m_pendingStationInfo["COUNTRY"]    = data.country;
    m_pendingStationInfo["GRIDSQUARE"] = data.grid;
    if (data.dxcc    > 0) m_pendingStationInfo["DXCC"] = QString::number(data.dxcc);
    if (data.cqzone  > 0) m_pendingStationInfo["CQZ"]  = QString::number(data.cqzone);
    if (data.ituzone > 0) m_pendingStationInfo["ITUZ"] = QString::number(data.ituzone);
    if (!data.iota.isEmpty()) m_pendingStationInfo["IOTA"] = data.iota;

    // Status bar: call name city state country
    QString status = data.call;
    if (!fullName.isEmpty())    status += " " + fullName;
    if (!data.addr2.isEmpty())  status += " " + data.addr2;
    if (!data.state.isEmpty())  status += " " + data.state;
    if (!data.country.isEmpty()) status += " " + data.country;
    m_statusLabel->setText(status);

    DebugLogger::instance().log("CallsignLookup",
        QString("QRZ lookup found: %1 (grid=%2 dxcc=%3 cq=%4 itu=%5)")
            .arg(status).arg(data.grid).arg(data.dxcc)
            .arg(data.cqzone).arg(data.ituzone));
}

void MainWindow::onQrzCallsignNotFound(const QString& callsign)
{
    m_pendingLookupCallsign.clear();
    m_pendingStationInfo.clear();
    m_statusLabel->setText(QString("Callsign %1 not found in QRZ").arg(callsign));
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZ lookup: %1 not found").arg(callsign));
}

void MainWindow::onQrzLookupError(const QString& error)
{
    DebugLogger::instance().log("CallsignLookup",
        QString("QRZ lookup error: %1").arg(error));
}

void MainWindow::onToggleDxCluster(bool checked)
{
    if (m_dxClusterDock) {
        m_dxClusterDock->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::onToggleCwConsole(bool checked)
{
    if (m_cwConsoleDock) {
        m_cwConsoleDock->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::onToggleScoreWidget(bool checked)
{
    if (m_scoreDock) {
        m_scoreDock->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::onToggleScpWidget(bool checked)
{
    if (m_scpWidget) {
        if (checked) {
            // Show the widget
            m_scpWidget->setVisible(true);
            
            // If it's floating, dock it back to the right panel
            if (m_scpWidget->isFloating()) {
                // Get the dock area where it last was (default to right)
                m_scpWidget->setFloating(false);
                // Ensure it's in the right dock area
                addDockWidget(Qt::RightDockWidgetArea, m_scpWidget);
                // Stack it below the score widget if possible
                if (m_scoreDock) {
                    splitDockWidget(m_scoreDock, m_scpWidget, Qt::Vertical);
                }
                DebugLogger::instance().log("MainWindow", "SCP widget docked to right panel");
            } else {
                DebugLogger::instance().log("MainWindow", "SCP widget shown (already docked)");
            }
        } else {
            // Hide the widget
            m_scpWidget->setVisible(false);
            DebugLogger::instance().log("MainWindow", "SCP widget hidden");
        }
        updateScpWidgetMenuText();
        savePanelState();
    }
}

void MainWindow::updateScpWidgetMenuText()
{
    if (!m_scpWidgetAction || !m_scpWidget) {
        return;
    }
    
    // Update menu text based on widget state
    if (m_scpWidget->isVisible()) {
        if (m_scpWidget->isFloating()) {
            m_scpWidgetAction->setText("&Super Check Partial (floating)");
        } else {
            m_scpWidgetAction->setText("&Super Check Partial (docked)");
        }
    } else {
        m_scpWidgetAction->setText("&Super Check Partial");
    }
}

void MainWindow::savePanelState()
{
    Settings& settings = Settings::instance();

    // Save dock widget visibility
    settings.setDxClusterVisible(m_dxClusterDock && m_dxClusterDock->isVisible());
    settings.setCwConsoleVisible(m_cwConsoleDock && m_cwConsoleDock->isVisible());

    // Save splitter state
    if (m_mainSplitter) {
        settings.setMainSplitterState(m_mainSplitter->saveState());
    }

    // Log dock widget sizes before saving
    DebugLogger::instance().log("MainWindow",
        QString("Before save - DX Cluster height=%1, CW Console height=%2, Score height=%3, SCP height=%4, SSB height=%5, Mult height=%6")
        .arg(m_dxClusterDock ? m_dxClusterDock->height() : -1)
        .arg(m_cwConsoleDock ? m_cwConsoleDock->height() : -1)
        .arg(m_scoreDock ? m_scoreDock->height() : -1)
        .arg(m_scpWidget ? m_scpWidget->height() : -1)
        .arg(m_ssbMemoriesWidget ? m_ssbMemoriesWidget->height() : -1)
        .arg(m_multiplierDock ? m_multiplierDock->height() : -1));

    // Save dock widget state (positions, sizes, floating state)
    QByteArray dockState = saveState();
    settings.setDockWidgetState(dockState);
    DebugLogger::instance().log("MainWindow",
        QString("Saved dock widget state (%1 bytes)").arg(dockState.size()));

    // Write to disk
    settings.save();
}

void MainWindow::restorePanelState()
{
    Settings& settings = Settings::instance();

    // Read visibility settings (applied AFTER restoreState below)
    bool dxVisible = settings.getDxClusterVisible();
    bool cwVisible = settings.getCwConsoleVisible();

    // Restore splitter state
    if (m_mainSplitter) {
        QByteArray state = settings.getMainSplitterState();
        if (!state.isEmpty()) {
            m_mainSplitter->restoreState(state);
        }
    }

    // Restore dock widget state (positions, sizes, floating state)
    QByteArray dockState = settings.getDockWidgetState();
    DebugLogger::instance().log("MainWindow",
        QString("getDockWidgetState() returned %1 bytes").arg(dockState.size()));
    if (!dockState.isEmpty()) {
        DebugLogger::instance().log("MainWindow",
            QString("Restoring dock widget state (%1 bytes)").arg(dockState.size()));

        // Use version 0 for compatibility
        bool success = restoreState(dockState, 0);

        // Re-apply corner ownership after restore — restoreState may reflow dock areas.
        setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
        setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

        // restoreState() sets the floating flag and geometry but does not call show() on
        // floating dock widgets (they are top-level windows when floating). Show them now.
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            if (dock->isFloating())
                dock->show();
        }

        // Keep blocking for 2 seconds to allow Qt's layout system to finish applying sizes
        // Unblock after layout has settled
        QTimer::singleShot(2000, this, [this]() {
            m_restoringState = false;
            DebugLogger::instance().log("MainWindow", "State restoration complete, save timer unblocked");
        });

        // Override dock visibility AFTER restoreState — restoreState sets visibility
        // from the binary blob which may be stale; the boolean settings are authoritative.
        if (m_dxClusterDock) m_dxClusterDock->setVisible(dxVisible);
        if (m_cwConsoleDock) m_cwConsoleDock->setVisible(cwVisible);

        DebugLogger::instance().log("MainWindow",
            QString("Dock state restore %1, blocking saves for 2s").arg(success ? "succeeded" : "failed"));

        // Log dock widget positions after restore
        DebugLogger::instance().log("MainWindow", 
            QString("DX Cluster area: %1, floating: %2")
                .arg(dockWidgetArea(m_dxClusterDock))
                .arg(m_dxClusterDock->isFloating()));
        DebugLogger::instance().log("MainWindow",
            QString("CW Console area: %1, floating: %2")
                .arg(dockWidgetArea(m_cwConsoleDock))
                .arg(m_cwConsoleDock->isFloating()));
        DebugLogger::instance().log("MainWindow",
            QString("Score area: %1, floating: %2")
                .arg(dockWidgetArea(m_scoreDock))
                .arg(m_scoreDock->isFloating()));

        // Log dock widget sizes after restore
        DebugLogger::instance().log("MainWindow",
            QString("After restore - DX Cluster height=%1, CW Console height=%2, Score height=%3, SCP height=%4, SSB height=%5, Mult height=%6")
            .arg(m_dxClusterDock ? m_dxClusterDock->height() : -1)
            .arg(m_cwConsoleDock ? m_cwConsoleDock->height() : -1)
            .arg(m_scoreDock ? m_scoreDock->height() : -1)
            .arg(m_scpWidget ? m_scpWidget->height() : -1)
            .arg(m_ssbMemoriesWidget ? m_ssbMemoriesWidget->height() : -1)
            .arg(m_multiplierDock ? m_multiplierDock->height() : -1));

        // Log again after layout has settled
        QTimer::singleShot(1000, this, [this]() {
            DebugLogger::instance().log("MainWindow",
                QString("After layout (1s) - DX Cluster height=%1, CW Console height=%2, Score height=%3, SCP height=%4, SSB height=%5, Mult height=%6")
                .arg(m_dxClusterDock ? m_dxClusterDock->height() : -1)
                .arg(m_cwConsoleDock ? m_cwConsoleDock->height() : -1)
                .arg(m_scoreDock ? m_scoreDock->height() : -1)
                .arg(m_scpWidget ? m_scpWidget->height() : -1)
                .arg(m_ssbMemoriesWidget ? m_ssbMemoriesWidget->height() : -1)
                .arg(m_multiplierDock ? m_multiplierDock->height() : -1));
        });
    } else {
        DebugLogger::instance().log("MainWindow", "No saved dock widget state found");
        // No dock state blob — apply visibility from boolean settings directly
        if (m_dxClusterDock) m_dxClusterDock->setVisible(dxVisible);
        if (m_cwConsoleDock) m_cwConsoleDock->setVisible(cwVisible);
        // Unblock save timer after a brief delay for layout to settle
        QTimer::singleShot(500, this, [this]() {
            m_restoringState = false;
            DebugLogger::instance().log("MainWindow", "State restoration complete (no saved state), save timer unblocked");
        });
    }

    // Sync menu checkmarks with actual visibility
    if (m_dxClusterAction) m_dxClusterAction->setChecked(dxVisible);
    if (m_cwConsoleAction) m_cwConsoleAction->setChecked(cwVisible);
}

void MainWindow::onResetWidgetPositions()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Reset Widget Positions",
        "Reset all widget positions and sizes to defaults?\n\nThe application will need to restart for the changes to take full effect.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QString defaultLayoutPath = Settings::getDataPath() + "/default_layout.json";
    QFile file(defaultLayoutPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Reset Widget Positions",
            "Could not find default_layout.json");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return;

    QJsonObject defaults = doc.object();
    Settings& settings = Settings::instance();

    // Apply UI state from defaults
    if (defaults.contains("ui")) {
        QJsonObject ui = defaults["ui"].toObject();
        if (ui.contains("dockWidgetState")) {
            QByteArray dockState = QByteArray::fromBase64(ui["dockWidgetState"].toString().toLatin1());
            settings.setDockWidgetState(dockState);
            restoreState(dockState, 0);
            // Re-apply corner ownership so the top and bottom dock areas
            // don't extend over the right-side docks (DX cluster, etc.).
            setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
            setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
        }
        if (ui.contains("mainSplitterState") && m_mainSplitter) {
            QByteArray splitterState = QByteArray::fromBase64(ui["mainSplitterState"].toString().toUtf8());
            settings.setMainSplitterState(splitterState);
            m_mainSplitter->restoreState(splitterState);
        }
        settings.setDxClusterVisible(ui["dxClusterVisible"].toBool(true));
        settings.setCwConsoleVisible(ui["cwConsoleVisible"].toBool(true));
    }

    // Apply window geometry from defaults
    if (defaults.contains("window")) {
        QJsonObject win = defaults["window"].toObject();
        if (win.contains("geometryState")) {
            QByteArray geomState = QByteArray::fromBase64(win["geometryState"].toString().toLatin1());
            settings.setWindowGeometryState(geomState);
            restoreGeometry(geomState);
        }
    }

    // Restore visibility and menu state
    restorePanelState();
    settings.save();

    DebugLogger::instance().log("MainWindow", "Widget positions reset to defaults");
}

QString MainWindow::freq2Mode(double freqMHz)
{
    return BandPlan::freq2Mode(freqMHz);
}

bool MainWindow::loadContestDefinition(const QString& filePath, bool restoreStationClass)
{
    DebugLogger::instance().log("MainWindow", QString("loadContestDefinition called with: %1 (restoreStationClass=%2)").arg(filePath).arg(restoreStationClass ? "true" : "false"));
    
    // Save current station class state before reset (it may have been loaded from a file)
    QString savedStationClass = m_contestEngine->getStationClass();
    QString savedStationClassExchange = m_contestEngine->getStationClassExchangeData();
    
    DebugLogger::instance().log("MainWindow", 
        QString("Before reset: stationClass='%1' exchange='%2'").arg(savedStationClass, savedStationClassExchange));
    
    // Reset contest engine state for new log
    m_contestEngine->resetStationClassState();
    
    // Restore station class state if it was saved from file (and if requested)
    if (restoreStationClass) {
        if (!savedStationClass.isEmpty()) {
            m_contestEngine->setStationClass(savedStationClass);
            DebugLogger::instance().log("MainWindow", QString("Restored station class: %1").arg(savedStationClass));
        }
        if (!savedStationClassExchange.isEmpty()) {
            m_contestEngine->setStationClassExchangeData(savedStationClassExchange);
            DebugLogger::instance().log("MainWindow", QString("Restored station class exchange: %1").arg(savedStationClassExchange));
        }
    } else {
        // restoreStationClass=false means don't show a dialog, but a station class may
        // have been pre-set before this call (e.g. loaded from a CLX file). Restore it
        // so contest loading and widget population see the correct class.
        // For truly new logs savedStationClass is empty, so nothing is restored.
        if (!savedStationClass.isEmpty()) {
            m_contestEngine->setStationClass(savedStationClass);
            DebugLogger::instance().log("MainWindow", QString("Restored pre-set station class (no dialog): %1").arg(savedStationClass));
        }
        if (!savedStationClassExchange.isEmpty()) {
            m_contestEngine->setStationClassExchangeData(savedStationClassExchange);
            DebugLogger::instance().log("MainWindow", QString("Restored pre-set station class exchange (no dialog): %1").arg(savedStationClassExchange));
        }
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        DebugLogger::instance().log("MainWindow", 
            QString("Failed to load contest definition: %1").arg(filePath));
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        DebugLogger::instance().log("MainWindow", "Invalid contest definition format");
        return false;
    }
    
    m_contestDefinition = doc.object();
    m_contestFile = QFileInfo(filePath).fileName();
    
    // Load into contest engine
    if (!m_contestEngine->loadContest(m_contestDefinition)) {
        DebugLogger::instance().log("MainWindow", "Failed to load contest into engine");
        m_contestDefinition = QJsonObject();  // Clear on failure
        return false;
    }
    
    // Check if station class selection is needed
    // Only show dialog if we're restoring state (opening existing file), not for new logs
    if (restoreStationClass && m_contestEngine->needsStationClass()) {
        // Use existing station class if available (from loaded file)
        QString currentClass = m_contestEngine->getStationClass();
        
        // Only show dialog if no station class is already set
        if (currentClass.isEmpty()) {
            QStringList classOptions = m_contestEngine->getStationClassOptions();
            QString selectedClass;

            // Auto-select if only one class available
            if (classOptions.size() == 1) {
                selectedClass = classOptions.first().split('|').first();
                DebugLogger::instance().log("MainWindow",
                    QString("Auto-selected single station class: %1").arg(selectedClass));
            } else {
                StationClassDialog dialog(
                    m_contestEngine->getStationClassPrompt(),
                    classOptions,
                    this,
                    currentClass  // Pass current class as default
                );

                if (dialog.exec() == QDialog::Accepted) {
                    selectedClass = dialog.getSelectedClass();
                }
            }

            if (!selectedClass.isEmpty()) {
                m_contestEngine->setStationClass(selectedClass);
                DebugLogger::instance().log("MainWindow",
                    QString("Station class selected: %1").arg(selectedClass));

                // Prompt for operator callsign if contest requests it
                if (m_contestEngine->stationClassPromptsForCallsign()) {
                    QString defaultCall = Settings::instance().getCallsign();
                    QDialog callDialog(this);
                    callDialog.setWindowTitle("Operator Callsign");
                    QVBoxLayout callLayout(&callDialog);
                    QLabel callLabel("Enter operator callsign:");
                    QLineEdit callEdit;
                    callEdit.setText(defaultCall);
                    callEdit.selectAll();
                    QDialogButtonBox callButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                    callLayout.addWidget(&callLabel);
                    callLayout.addWidget(&callEdit);
                    callLayout.addWidget(&callButtons);
                    connect(&callButtons, &QDialogButtonBox::accepted, &callDialog, &QDialog::accept);
                    connect(&callButtons, &QDialogButtonBox::rejected, &callDialog, &QDialog::reject);
                    connect(&callEdit, &QLineEdit::textChanged, [&callEdit](const QString& text) {
                        if (text != text.toUpper()) {
                            int pos = callEdit.cursorPosition();
                            callEdit.blockSignals(true);
                            callEdit.setText(text.toUpper());
                            callEdit.setCursorPosition(pos);
                            callEdit.blockSignals(false);
                        }
                    });
                    if (callDialog.exec() == QDialog::Accepted && !callEdit.text().trimmed().isEmpty()) {
                        m_sessionStationInfo->setCallsign(callEdit.text().trimmed().toUpper());
                        DebugLogger::instance().log("MainWindow",
                            QString("Operator callsign set to: %1").arg(m_sessionStationInfo->callsign()));
                    } else {
                        DebugLogger::instance().log("MainWindow", "Operator callsign prompt cancelled");
                        m_contestDefinition = QJsonObject();
                        return false;
                    }
                }

                // Check if this class needs additional input and we don't have saved data
                if (m_contestEngine->stationClassNeedsInput() && m_contestEngine->getStationClassExchangeData().isEmpty()) {
                    // Prompt for name and ID separately
                    QString namePrompt = m_contestEngine->getStationClassNamePrompt();
                    QString idPrompt = m_contestEngine->getStationClassIdPrompt();
                    QJsonObject inputValidation = m_contestEngine->getStationClassInputValidation();

                    // Create custom dialog for name with uppercase
                    QInputDialog *nameDialog = new QInputDialog(this);
                    nameDialog->setWindowTitle("Station Information");
                    nameDialog->setLabelText(namePrompt.isEmpty() ? "Enter your first name:" : namePrompt);
                    nameDialog->setInputMode(QInputDialog::TextInput);
                    
                    QLineEdit *nameEdit = nameDialog->findChild<QLineEdit*>();
                    if (nameEdit) {
                        // Extract validation rules for name from contest definition
                        QJsonObject nameValidation = inputValidation.value("name").toObject();
                        bool forceUppercase = nameValidation.contains("forceUppercase") ? nameValidation["forceUppercase"].toBool() : true;
                        
                        // Apply uppercase conversion if configured
                        if (forceUppercase) {
                            connect(nameEdit, &QLineEdit::textChanged, [nameEdit](const QString& text) {
                                if (text != text.toUpper()) {
                                    int cursorPos = nameEdit->cursorPosition();
                                    nameEdit->blockSignals(true);
                                    nameEdit->setText(text.toUpper());
                                    nameEdit->setCursorPosition(cursorPos);
                                    nameEdit->blockSignals(false);
                                }
                            });
                        }
                    }
                    
                    bool okName = (nameDialog->exec() == QDialog::Accepted);
                    QString name = nameDialog->textValue();
                    
                    if (!okName || name.isEmpty()) {
                        DebugLogger::instance().log("MainWindow", "Station class name input cancelled");
                        m_contestDefinition = QJsonObject();
                        nameDialog->deleteLater();
                        return false;
                    }
                    nameDialog->deleteLater();
                    
                    // Create custom dialog for ID with validation rules from contest definition
                    QInputDialog *idDialog = new QInputDialog(this);
                    idDialog->setWindowTitle("Station Information");
                    idDialog->setLabelText(idPrompt.isEmpty() ? "Enter ID or location:" : idPrompt);
                    idDialog->setInputMode(QInputDialog::TextInput);
                    
                    QLineEdit *idEdit = idDialog->findChild<QLineEdit*>();
                    if (idEdit) {
                        // Extract validation rules from contest definition
                        QJsonObject idValidation = inputValidation.value("id").toObject();
                        QString validationType = idValidation.contains("type") ? idValidation["type"].toString() : "alphanumeric";
                        QString defaultValue = idValidation.contains("defaultValue") ? idValidation["defaultValue"].toString() : "";
                        bool forceUppercase = idValidation.contains("forceUppercase") ? idValidation["forceUppercase"].toBool() : true;
                        
                        // Set default value if provided
                        if (!defaultValue.isEmpty()) {
                            idDialog->setTextValue(defaultValue);
                        }
                        
                        // Apply validator based on type
                        if (validationType == "numeric") {
                            idEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), idEdit));
                        } else if (validationType == "alphanumeric") {
                            idEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[A-Za-z0-9]*$"), idEdit));
                        }
                        
                        // Apply uppercase conversion if configured
                        if (forceUppercase) {
                            connect(idEdit, &QLineEdit::textChanged, [idEdit](const QString& text) {
                                if (text != text.toUpper()) {
                                    int cursorPos = idEdit->cursorPosition();
                                    idEdit->blockSignals(true);
                                    idEdit->setText(text.toUpper());
                                    idEdit->setCursorPosition(cursorPos);
                                    idEdit->blockSignals(false);
                                }
                            });
                        }
                    }
                    
                    bool okId = (idDialog->exec() == QDialog::Accepted);
                    QString id = idDialog->textValue();
                    
                    if (!okId || id.isEmpty()) {
                        DebugLogger::instance().log("MainWindow", "Station class ID input cancelled");
                        m_contestDefinition = QJsonObject();
                        idDialog->deleteLater();
                        return false;
                    }
                    idDialog->deleteLater();
                    
                    // Set name and ID separately (setter will force uppercase)
                    m_contestEngine->setStationClassExchangeName(name);
                    m_contestEngine->setStationClassExchangeId(id);
                    DebugLogger::instance().log("MainWindow", 
                        QString("Station class exchange data set - Name: %1, ID: %2").arg(name.toUpper(), id.toUpper()));
                }
            } else {
                DebugLogger::instance().log("MainWindow", "Station class selection cancelled");
                m_contestDefinition = QJsonObject();  // Clear on cancellation
                return false;
            }
        } else {
            DebugLogger::instance().log("MainWindow", 
                QString("Using existing station class from loaded file: %1").arg(currentClass));
            
            // If we have saved exchange data, log it
            QString savedName = m_contestEngine->getStationClassExchangeName();
            QString savedId = m_contestEngine->getStationClassExchangeId();
            if (!savedName.isEmpty() || !savedId.isEmpty()) {
                DebugLogger::instance().log("MainWindow", 
                    QString("Using saved exchange data: Name=%1, ID=%2").arg(savedName, savedId));
            }
        }
    }
    
    updateLogHeaders();
    updateQsoEntryFields();
    
    QString contestName = m_contestEngine->getContestName();
    DebugLogger::instance().log("MainWindow", 
        QString("Loaded contest: %1").arg(contestName));
    
    // Update contest name in status bar
    if (m_contestNameLabel) {
        m_contestNameLabel->setText(QString("Contest: %1").arg(contestName));
    }
    
    // Extract and set contest bands for score widget
    if (m_scoreWidget && m_contestDefinition.contains("contest")) {
        QJsonObject contestObj = m_contestDefinition["contest"].toObject();
        if (contestObj.contains("bands")) {
            QJsonArray bandsArray = contestObj["bands"].toArray();
            QStringList contestBands;
            for (const QJsonValue& val : bandsArray) {
                // Bands is an array of strings like ["10m", "15m", "20m"]
                contestBands.append(val.toString());
            }
            if (!contestBands.isEmpty()) {
                m_scoreWidget->setContestBands(contestBands);
                DebugLogger::instance().log("MainWindow", 
                    QString("Set score widget bands: %1").arg(contestBands.join(", ")));
            }
        }
        
        // Set multiplier categories for score widget display
        QStringList multCategories = m_contestEngine->getMultiplierCategories();
        if (!multCategories.isEmpty()) {
            m_scoreWidget->setMultCategories(multCategories);
        }
    }

    // Configure multiplier widget if panel is enabled for this contest
    if (m_multiplierWidget && m_contestDefinition.contains("ui")) {
        QJsonObject ui = m_contestDefinition["ui"].toObject();
        if (ui["showMultiplierPanel"].toBool(false)) {
            DebugLogger::instance().log("MultiplierWidget",
                QString("loadContestDefinition: stationType='%1', effective mults=%2, full mults=%3")
                    .arg(m_contestEngine->getUserPromptValue("stationType"))
                    .arg(m_contestEngine->getEffectiveNamedMultiplierList().size())
                    .arg(m_contestEngine->getNamedMultiplierList().size()));
            m_multiplierWidget->updateWorkedMultipliers(QSet<QString>());  // clear stale state from previous session
            m_multiplierWidget->setMultiplierList(m_contestEngine->getEffectiveNamedMultiplierList());
            m_multiplierWidget->setMultiplierType(m_contestEngine->getMultiplierType());

            // Build bands and modes lists for filter
            QStringList contestBandsList;
            QStringList contestModesList;
            if (m_contestDefinition.contains("contest")) {
                QJsonObject contestObj = m_contestDefinition["contest"].toObject();
                if (contestObj.contains("bands")) {
                    QJsonArray bandsArray = contestObj["bands"].toArray();
                    for (const QJsonValue& val : bandsArray) {
                        contestBandsList.append(val.toString());
                    }
                }
                if (contestObj.contains("modes")) {
                    QJsonArray modesArray = contestObj["modes"].toArray();
                    for (const QJsonValue& val : modesArray) {
                        contestModesList.append(val.toString());
                    }
                }
            }
            m_multiplierWidget->setFilterOptions(contestBandsList, contestModesList);

            DebugLogger::instance().log("MainWindow",
                QString("Multiplier widget configured: %1 mults, type=%2")
                    .arg(m_contestEngine->getEffectiveNamedMultiplierList().size())
                    .arg(m_contestEngine->getMultiplierType()));
        } else {
            m_multiplierWidget->clear();
        }
    }

    if (m_dxClusterPanel)
        m_dxClusterPanel->setBands(m_contestEngine->getAllowedBands());

    // Notify the CW Decoder widgets so the "Practice — Contest Exchange"
    // source entry in the audio-device dropdown becomes selectable.
    if (m_cwDecoderLeft)  m_cwDecoderLeft->refreshPracticeContestAvailability();
    if (m_cwDecoderRight) m_cwDecoderRight->refreshPracticeContestAvailability();

    // Refresh the Remote Control snapshot so /api/status and /api/score
    // reflect the newly-loaded contest.
    updateSnapshotStatus();
    updateSnapshotScore();
    updateSnapshotQsos();
    updateSnapshotMults();

    updateWindowTitle();
    DebugLogger::instance().log("MainWindow", QString("loadContestDefinition completed successfully, m_contestDefinition.isEmpty(): %1").arg(m_contestDefinition.isEmpty() ? "true" : "false"));
    DebugLogger::instance().log("MainWindow", QString("Contest name: %1").arg(m_contestEngine->getContestName()));
    return true;
}

void MainWindow::updateLogHeaders()
{
    QStringList fullHeaders;
    
    // Try to get columns from contest definition first
    if (!m_contestDefinition.isEmpty() && m_contestDefinition.contains("ui")) {
        QJsonObject ui = m_contestDefinition["ui"].toObject();
        if (ui.contains("logColumns")) {
            QJsonArray columns = ui["logColumns"].toArray();
            for (const QJsonValue& val : columns) {
                fullHeaders.append(val.toString());
            }
            
            DebugLogger::instance().log("MainWindow", 
                QString("Using contest-specific log columns: %1").arg(fullHeaders.join(", ")));
        }
    }
    
    // Fall back to default columns if not found in contest definition
    if (fullHeaders.isEmpty()) {
        fullHeaders << "DATE" << "TIME" << "CALL" << "FREQ" << "MODE"
                   << "RSTs" << "RSTr" << "EXCHs" << "EXCHr"
                   << "Nr" << "Dupe" << "M" << "C" << "P" << "COMMENT";

        DebugLogger::instance().log("MainWindow",
            QString("Using default log columns: %1").arg(fullHeaders.join(", ")));
    }

    // Filter columns by visibleWhen conditions in qsoFields
    {
        QStringList filtered;
        for (const QString& col : fullHeaders) {
            if (isFieldVisible(col)) filtered.append(col);
        }
        fullHeaders = filtered;
    }

    // Always prepend "#" (QSO number) as the first column if not already present
    if (!fullHeaders.isEmpty() && fullHeaders.first() != "#") {
        fullHeaders.prepend("#");
    }

    // Enforce C column visibility: show only when contest has DXCC multipliers.
    // This applies regardless of whether columns came from JSON or the default list.
    bool hasDxccMults = m_contestEngine &&
                        m_contestEngine->getMultiplierCategories().contains("dxcc");
    if (hasDxccMults && !fullHeaders.contains("C")) {
        // Insert C directly after M, or before COMMENT if M is absent
        int mIdx = fullHeaders.indexOf("M");
        fullHeaders.insert(mIdx >= 0 ? mIdx + 1 : fullHeaders.size() - 1, "C");
    } else if (!hasDxccMults) {
        fullHeaders.removeAll("C");
    }

    // Update the model
    m_qsoModel->setColumnHeaders(fullHeaders);

    // Set narrow default widths for short columns, then restore any user overrides
    static const QMap<QString, int> defaultColumnWidths = {{"#", 40}, {"M", 30}, {"P", 30}, {"C", 30}};
    for (int i = 0; i < fullHeaders.size(); ++i) {
        if (defaultColumnWidths.contains(fullHeaders[i])) {
            m_qsoTable->setColumnWidth(i, defaultColumnWidths[fullHeaders[i]]);
        }
    }
    restoreColumnWidths();

    // Ensure no column is narrower than its header label
    QHeaderView* header = m_qsoTable->horizontalHeader();
    QFontMetrics fm(header->font());
    const int padding = 18;  // horizontal breathing room around the text
    for (int i = 0; i < fullHeaders.size(); ++i) {
        int minWidth = fm.horizontalAdvance(fullHeaders[i]) + padding;
        int current = m_qsoTable->columnWidth(i);
        if (current < minWidth) {
            m_qsoTable->setColumnWidth(i, minWidth);
            DebugLogger::instance().log("MainWindow",
                QString("Column '%1' too narrow (%2px < %3px min), expanded")
                    .arg(fullHeaders[i]).arg(current).arg(minWidth));
        }
    }

    DebugLogger::instance().log("MainWindow",
        QString("Updated log headers: %1").arg(fullHeaders.join(", ")));
}

void MainWindow::updateQsoEntryFields()
{
    if (m_contestDefinition.isEmpty()) {
        DebugLogger::instance().log("MainWindow", "updateQsoEntryFields: Contest definition is empty");
        return;
    }
    
    DebugLogger::instance().log("MainWindow", "updateQsoEntryFields: Starting...");
    
    // Hide all existing widgets first
    for (int i = 0; i < m_qsoEntryLayout->count(); ++i) {
        if (QWidget* widget = m_qsoEntryLayout->itemAt(i)->widget()) {
            widget->hide();
        }
    }
    
    // Clear existing dynamic exchange fields
    for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
        if (it.value() != m_callEdit && it.value() != m_exchangeEdit) {
            it.value()->deleteLater();
        }
    }
    m_exchangeFields.clear();
    m_entryFieldOrder.clear();
    
    // Read field navigation keys from contest definition
    m_fieldNavigationKeys = "both";  // Default to both space and tab
    if (m_contestDefinition.contains("ui")) {
        QJsonObject ui = m_contestDefinition["ui"].toObject();
        if (ui.contains("fieldNavigation")) {
            QJsonObject fieldNav = ui["fieldNavigation"].toObject();
            if (fieldNav.contains("keys")) {
                QString keys = fieldNav["keys"].toString();
                if (keys == "space" || keys == "tab" || keys == "both") {
                    m_fieldNavigationKeys = keys;
                }
            }
        }
    }
    DebugLogger::instance().log("MainWindow", 
        QString("Field navigation keys: %1").arg(m_fieldNavigationKeys));
    
    // Get exchange field list from contest engine
    QStringList exchangeFields = m_contestEngine->getExchangeFields();
    DebugLogger::instance().log("MainWindow", 
        QString("updateQsoEntryFields: Exchange fields from engine: %1")
        .arg(exchangeFields.join(", ")));
    
    if (!m_qsoEntryLayout) {
        DebugLogger::instance().log("MainWindow", "updateQsoEntryFields: No QSO entry layout");
        return;
    }
    
    // Only create input fields for RECEIVED exchange fields per contest UI definition
    QStringList entryFieldsList;
    if (m_contestDefinition.contains("ui")) {
        QJsonObject ui = m_contestDefinition["ui"].toObject();
        if (ui.contains("entryFields")) {
            QJsonArray entryFieldsArray = ui["entryFields"].toArray();
            for (const QJsonValue& val : entryFieldsArray) {
                entryFieldsList << val.toString();
            }
        }
    }
    
    // Fallback if not specified
    if (entryFieldsList.isEmpty()) {
        entryFieldsList << "RSTr" << "EXCHr";
    }
    
    // Ensure CALL is first if not already in the list
    if (!entryFieldsList.contains("CALL")) {
        entryFieldsList.prepend("CALL");
    } else {
        // Move CALL to the front
        entryFieldsList.removeAll("CALL");
        entryFieldsList.prepend("CALL");
    }
    
    // Create exchange field widgets dynamically
    // Start from the beginning of the layout
    int insertIndex = 0;
    
    // Create fields dynamically
    int insertPos = 0;
    for (const QString& fieldName : entryFieldsList) {
        QLabel* label = nullptr;
        QLineEdit* edit = nullptr;
        
        // Use existing widgets where possible
        if (fieldName == "CALL" && m_callEdit) {
            // Reuse the existing call edit
            edit = m_callEdit;
            edit->show();
            edit->setMaxLength(14);  // Standard callsign length
            edit->setMaximumWidth(120);
            
            // Find its label
            for (int i = 0; i < m_qsoEntryLayout->count(); ++i) {
                QLabel* lbl = qobject_cast<QLabel*>(m_qsoEntryLayout->itemAt(i)->widget());
                if (lbl && lbl->text() == "Call:") {
                    label = lbl;
                    label->show();
                    break;
                }
            }
        } else {
            // Create new widgets
            label = new QLabel(fieldName + ":");
            edit = new QLineEdit();
        }
        
        // Set appropriate width and defaults based on field type
        if (fieldName.contains("RST", Qt::CaseSensitive)) {
            edit->setMaximumWidth(60);  // 2-3 digits + some padding
            edit->setMaxLength(3);
            // Pre-fill RSTr based on current mode (599 for CW, 59 for SSB, +0 for DIGI)
            // Use a safe default if mode is not yet set
            QString currentMode = m_lastMode.isEmpty() ? "USB" : m_lastMode;
            QString defaultRst = (currentMode == "CW" || currentMode == "RTTY") ? "599" : 
                                (currentMode.contains("DIGI")) ? "+0" : "59";
            edit->setText(defaultRst);
            DebugLogger::instance().log("MainWindow", 
                QString("Setting default RST for %1: %2 (mode: %3)")
                .arg(fieldName).arg(defaultRst).arg(currentMode));
        } else if (fieldName.contains("EXCH", Qt::CaseSensitive)) {
            edit->setMinimumWidth(80);
            edit->setMaximumWidth(150);
            // Pre-fill EXCHs with station QTH if this is a sent field
            // (handled by onLogQSO)
        } else {
            edit->setMinimumWidth(80);
            edit->setMaximumWidth(150);
        }
        
        // Force uppercase input for all fields
        connect(edit, &QLineEdit::textChanged, [edit](const QString& text) {
            if (text != text.toUpper()) {
                int cursorPos = edit->cursorPosition();
                edit->setText(text.toUpper());
                edit->setCursorPosition(cursorPos);
            }
        });
        
        // Install event filter for Enter key handling
        edit->installEventFilter(this);
        
        // Store field for later access
        m_exchangeFields[fieldName] = edit;
        m_entryFieldOrder.append(edit);  // Track order for Space-to-advance
        
        // Add to layout in correct position
        if (label) {
            m_qsoEntryLayout->insertWidget(insertPos++, label);
        }
        m_qsoEntryLayout->insertWidget(insertPos++, edit);
        
        DebugLogger::instance().log("MainWindow", 
            QString("Added exchange field: %1").arg(fieldName));
    }
    
    // Show the log button at the end
    m_logButton->show();
    m_clearButton->show();
    m_qrzButton->show();
    
    // Create horizontal layout for buttons (only if not already created)
    // Find if button layout already exists
    QHBoxLayout* existingButtonLayout = nullptr;
    for (int i = m_qsoEntryLayout->count() - 1; i >= 0; --i) {
        QLayoutItem* item = m_qsoEntryLayout->itemAt(i);
        if (item && item->layout() && dynamic_cast<QHBoxLayout*>(item->layout())) {
            // Check if this layout contains the buttons
            if (item->layout()->indexOf(m_logButton) >= 0) {
                existingButtonLayout = dynamic_cast<QHBoxLayout*>(item->layout());
                break;
            }
        }
    }
    
    // If button layout doesn't exist, create it
    if (!existingButtonLayout) {
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        buttonLayout->addWidget(m_qrzButton);
        buttonLayout->addWidget(m_logButton);
        buttonLayout->addWidget(m_clearButton);
        buttonLayout->addStretch();
        m_qsoEntryLayout->addLayout(buttonLayout);
    }
    
    // Set proper tab order: fields in order -> QRZ button -> Log button
    QWidget* lastWidget = nullptr;
    for (const QString& fieldName : entryFieldsList) {
        if (m_exchangeFields.contains(fieldName)) {
            QLineEdit* edit = m_exchangeFields[fieldName];
            if (lastWidget) {
                setTabOrder(lastWidget, edit);
            }
            lastWidget = edit;
        }
    }
    if (lastWidget) {
        setTabOrder(lastWidget, m_qrzButton);
        setTabOrder(m_qrzButton, m_logButton);
        setTabOrder(m_logButton, m_clearButton);
    }

    // Replicate entry fields for Radio R if SO2R is active
    if (m_so2rEnabled && m_entryWidgetsR.entryLayout) {
        // Hide existing Radio R widgets
        for (int i = 0; i < m_entryWidgetsR.entryLayout->count(); ++i) {
            if (QWidget* widget = m_entryWidgetsR.entryLayout->itemAt(i)->widget()) {
                widget->hide();
            }
        }

        // Clean up old dynamic fields
        for (auto it = m_entryWidgetsR.exchangeFields.begin(); it != m_entryWidgetsR.exchangeFields.end(); ++it) {
            if (it.value() != m_entryWidgetsR.callEdit && it.value() != m_entryWidgetsR.exchangeEdit) {
                it.value()->deleteLater();
            }
        }
        m_entryWidgetsR.exchangeFields.clear();
        m_entryWidgetsR.entryFieldOrder.clear();

        int insertPosR = 0;
        for (const QString& fieldName : entryFieldsList) {
            QLabel* label = nullptr;
            QLineEdit* edit = nullptr;

            if (fieldName == "CALL" && m_entryWidgetsR.callEdit) {
                edit = m_entryWidgetsR.callEdit;
                edit->show();
                edit->setMaxLength(14);
                edit->setMaximumWidth(120);
                for (int i = 0; i < m_entryWidgetsR.entryLayout->count(); ++i) {
                    QLabel* lbl = qobject_cast<QLabel*>(m_entryWidgetsR.entryLayout->itemAt(i)->widget());
                    if (lbl && lbl->text() == "Call:") {
                        label = lbl;
                        label->show();
                        break;
                    }
                }
            } else {
                label = new QLabel(fieldName + ":");
                edit = new QLineEdit();
            }

            if (fieldName.contains("RST", Qt::CaseSensitive)) {
                edit->setMaximumWidth(60);
                edit->setMaxLength(3);
                QString currentMode = m_lastModeR.isEmpty() ? "USB" : m_lastModeR;
                QString defaultRst = (currentMode == "CW" || currentMode == "RTTY") ? "599" :
                                    (currentMode.contains("DIGI")) ? "+0" : "59";
                edit->setText(defaultRst);
            } else if (fieldName.contains("EXCH", Qt::CaseSensitive)) {
                edit->setMinimumWidth(80);
                edit->setMaximumWidth(150);
            } else {
                edit->setMinimumWidth(80);
                edit->setMaximumWidth(150);
            }

            connect(edit, &QLineEdit::textChanged, [edit](const QString& text) {
                if (text != text.toUpper()) {
                    int cursorPos = edit->cursorPosition();
                    edit->setText(text.toUpper());
                    edit->setCursorPosition(cursorPos);
                }
            });
            edit->installEventFilter(this);

            m_entryWidgetsR.exchangeFields[fieldName] = edit;
            m_entryWidgetsR.entryFieldOrder.append(edit);

            if (label) {
                m_entryWidgetsR.entryLayout->insertWidget(insertPosR++, label);
            }
            m_entryWidgetsR.entryLayout->insertWidget(insertPosR++, edit);
        }

        // Show buttons
        m_entryWidgetsR.logButton->show();
        m_entryWidgetsR.clearButton->show();
        m_entryWidgetsR.qrzButton->show();
    }
}

void MainWindow::checkDataFileStaleness()
{
    const int STALE_DAYS = 7;
    QString dataPath = Settings::getUserDataPath();

    auto fileAgeDays = [](const QString &path) -> int {
        QFileInfo fi(path);
        if (!fi.exists()) return -1;
        return fi.lastModified().daysTo(QDateTime::currentDateTime());
    };

    int ctyAge = fileAgeDays(dataPath + "/cty.dat");
    int scpAge = fileAgeDays(dataPath + "/master.scp");

    bool ctyStal = ctyAge >= STALE_DAYS;
    bool scpStale = scpAge >= STALE_DAYS;

    if (!ctyStal && !scpStale)
        return;

    // Build a description of what's stale
    QStringList staleFiles;
    if (ctyStal)
        staleFiles << QString("DXCC database (cty.dat) — %1 days old").arg(ctyAge);
    if (scpStale)
        staleFiles << QString("Super Check Partial (master.scp) — %1 days old").arg(scpAge);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Data Files Out of Date");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("One or more data files haven't been updated in over 7 days:");
    msgBox.setInformativeText(staleFiles.join("\n") + "\n\nWould you like to download the latest versions now?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() != QMessageBox::Yes)
        return;

    if (ctyStal)
        onDownloadCtyDat();
    if (scpStale)
        onDownloadScp();
}

void MainWindow::onDownloadCtyDat()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Download DXCC Database");
    msgBox.setText("Download the latest cty.dat from country-files.com?");
    msgBox.setInformativeText("This will download and install the latest DXCC prefix database.");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    
    if (msgBox.exec() == QMessageBox::Ok) {
        // Show progress dialog
        QProgressDialog *progress = new QProgressDialog("Downloading cty.dat...", "Cancel", 0, 100, this);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setValue(0);
        
        // Connect signals
        connect(m_dxccDatabase, &DxccDatabase::downloadProgress, 
                [progress](qint64 received, qint64 total) {
            if (total > 0) {
                progress->setValue((received * 100) / total);
            }
        });
        
        connect(m_dxccDatabase, &DxccDatabase::downloadFinished,
                [this, progress](bool success, const QString& error) {
            progress->close();
            progress->deleteLater();
            
            if (success) {
                QMessageBox::information(this, "Download Complete", 
                    "DXCC database downloaded and loaded successfully!");
            } else {
                QMessageBox::warning(this, "Download Failed", 
                    QString("Failed to download DXCC database:\n%1").arg(error));
            }
        });
        
        // Start download
        if (!m_dxccDatabase->downloadLatest()) {
            progress->close();
            progress->deleteLater();
            QMessageBox::warning(this, "Download Failed", 
                "Download already in progress or failed to start.");
        }
    }
}

void MainWindow::onDownloadScp()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Download Super Check Partial");
    msgBox.setText("Download the latest master.scp from supercheckpartial.com?");
    msgBox.setInformativeText("This will download and install the latest SCP callsign database.");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    
    if (msgBox.exec() == QMessageBox::Ok) {
        QProgressDialog *progress = new QProgressDialog("Downloading master.scp...", "Cancel", 0, 0, this);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->show();
        QApplication::processEvents();
        
        QString targetPath = SuperCheckPartial::instance().getDataFilePath();
        QString errorMessage;
        
        if (SuperCheckPartial::downloadLatestDatabase(targetPath, errorMessage)) {
            progress->close();
            progress->deleteLater();
            QMessageBox::information(this, "Download Complete", 
                "Master.scp downloaded and installed successfully!");
            DebugLogger::instance().log("MainWindow", "SCP database downloaded successfully");
        } else {
            progress->close();
            progress->deleteLater();
            QMessageBox::warning(this, "Download Failed", 
                QString("Failed to download SCP database:\n%1").arg(errorMessage));
            DebugLogger::instance().log("MainWindow", "SCP download failed: " + errorMessage);
        }
    }
}

void MainWindow::onScpDialog()
{
    ScpDialog dialog(m_scpWidget, this);
    dialog.exec();
}

bool MainWindow::isSemanticVersionEqual(const QString& v1, const QString& v2)
{
    // Parse semantic versions like "1.0" and "1.0.0" as equivalent
    auto parseVersion = [](const QString& version) -> QList<int> {
        QList<int> parts;
        for (const QString& part : version.split('.')) {
            parts.append(part.toInt());
        }
        // Pad with zeros to ensure same length
        while (parts.size() < 3) {
            parts.append(0);
        }
        return parts;
    };
    
    QList<int> parts1 = parseVersion(v1);
    QList<int> parts2 = parseVersion(v2);
    
    // Compare major.minor.patch
    return parts1[0] == parts2[0] && parts1[1] == parts2[1] && parts1[2] == parts2[2];
}

void MainWindow::onQsoDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    // Get the QSO at this row
    int row = index.row();
    QsoRecord qso = m_qsoModel->getQsos()[row];

    // Show edit dialog
    QsoEditDialog dialog(qso, this);
    if (dialog.exec() == QDialog::Accepted) {
        QsoRecord editedQso = dialog.getEditedQso();
        
        // Update the QSO in the model
        m_qsoModel->updateQso(row, editedQso);
        
        // Mark as modified
        m_isModified = true;
        
        // Recalculate score
        onRecalculateScore();
    }
}


void MainWindow::onQsoContextMenuRequested(const QPoint& pos)
{
    // Get the index of the clicked row
    QModelIndex index = m_qsoTable->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    m_contextMenuRow = index.row();

    // Create context menu
    QMenu menu(this);
    
    QAction *editAction = menu.addAction("Edit");
    QAction *deleteAction = menu.addAction("Delete");
    
    // Show menu and handle selection
    QAction *selectedAction = menu.exec(m_qsoTable->mapToGlobal(pos));
    
    if (selectedAction == editAction) {
        onEditQso();
    } else if (selectedAction == deleteAction) {
        onDeleteQso();
    }
}

void MainWindow::onEditQso()
{
    if (m_contextMenuRow < 0 || m_contextMenuRow >= m_qsoModel->getQsos().size()) {
        return;
    }

    QsoRecord qso = m_qsoModel->getQsos()[m_contextMenuRow];

    // Show edit dialog (same as double-click)
    QsoEditDialog dialog(qso, this);
    if (dialog.exec() == QDialog::Accepted) {
        QsoRecord editedQso = dialog.getEditedQso();
        
        // Update the QSO in the model
        m_qsoModel->updateQso(m_contextMenuRow, editedQso);

        // Mark as modified
        m_isModified = true;

        // Recalculate score
        onRecalculateScore();
        if (m_backupPath.isEmpty() && m_backupEnabled)
            initializeBackup();
        writeBackup();
    }
}

void MainWindow::onDeleteQso()
{
    if (m_contextMenuRow < 0 || m_contextMenuRow >= m_qsoModel->getQsos().size()) {
        return;
    }

    // Get QSO details for confirmation message
    QsoRecord qso = m_qsoModel->getQsos()[m_contextMenuRow];
    QString callsign = qso.getCall();
    
    // Prompt for confirmation
    QMessageBox::StandardButton result = QMessageBox::question(
        this,
        "Delete QSO",
        QString("Delete QSO with %1?").arg(callsign),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (result == QMessageBox::Yes) {
        m_qsoModel->removeQso(m_contextMenuRow);
        m_isModified = true;
        onRecalculateScore();
        if (m_backupPath.isEmpty() && m_backupEnabled)
            initializeBackup();
        writeBackup();
    }
}

// ---------------------------------------------------------------------------
// Crash-recovery backup
// ---------------------------------------------------------------------------

QString MainWindow::sanitizeForFilename(const QString& s)
{
    QString result;
    for (const QChar& c : s) {
        if (c.isLetterOrNumber())
            result += c.toUpper();
        else
            result += '_';
    }
    // Collapse consecutive underscores
    while (result.contains("__"))
        result.replace("__", "_");
    // Trim leading/trailing underscores
    while (result.startsWith('_')) result.remove(0, 1);
    while (result.endsWith('_'))   result.chop(1);
    return result;
}

void MainWindow::resetBackupState()
{
    // Remove the backup file from the outgoing session (if any) before starting fresh
    if (!m_backupPath.isEmpty() && QFile::exists(m_backupPath)) {
        QFile::remove(m_backupPath);
        DebugLogger::instance().log("MainWindow",
            QString("Removed previous session backup on reset: %1").arg(m_backupPath));
    }
    m_backupPath.clear();
    m_backupEnabled = true;
}

void MainWindow::initializeBackup()
{
    if (!m_contestEngine || m_contestDefinition.isEmpty())
        return;

    QString callsign    = sanitizeForFilename(getSessionCallsign());
    QString contestName = sanitizeForFilename(m_contestEngine->getContestName());
    QString timestamp   = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    if (callsign.isEmpty())    callsign    = "UNKNOWN";
    if (contestName.isEmpty()) contestName = "CONTEST";

    QString filename = callsign + "_" + contestName + "_" + timestamp + ".bak";
    m_backupPath = QDir::tempPath() + "/" + filename;

    if (QFile::exists(m_backupPath)) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this, "Backup File Exists",
            QString("A backup log file already exists:\n%1\n\n"
                    "Delete it and create a new backup?").arg(m_backupPath),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            m_backupEnabled = false;
            m_backupPath.clear();
            QMessageBox::information(this, "No Backup",
                "No backup log will be saved for this session.");
            return;
        }
        QFile::remove(m_backupPath);
    }

    m_backupEnabled = true;
    DebugLogger::instance().log("MainWindow",
        QString("Backup initialized: %1").arg(m_backupPath));
}

void MainWindow::writeBackup()
{
    if (!m_backupEnabled || m_backupPath.isEmpty())
        return;
    if (!m_contestEngine || m_contestDefinition.isEmpty())
        return;

    FileHandler fileHandler;
    fileHandler.setUseContestMemories(m_useContestMemories);
    fileHandler.setContestCwMemories(m_contestCwMemories);
    fileHandler.setContestSsbMemories(m_contestSsbMemories);

    const ContestEngine::ContestScore& cs = m_contestEngine->getRunningScore();
    fileHandler.setComputedScore(cs.contactScore, cs.contestScore);

    fileHandler.saveClxWithContest(
        m_backupPath, m_qsoModel->getQsos(),
        m_contestFile, m_contestDefinition,
        m_contestEngine->getStationClass(),
        m_contestEngine->getStationClassExchangeName(),
        m_contestEngine->getStationClassExchangeId(),
        m_contestEngine->getUserPromptValues(),
        *m_sessionStationInfo);

    DebugLogger::instance().log("MainWindow",
        QString("Backup written: %1 (%2 QSOs)").arg(m_backupPath).arg(m_qsoModel->count()));
}

void MainWindow::removeBackup()
{
    if (!m_backupPath.isEmpty()) {
        if (QFile::exists(m_backupPath)) {
            QFile::remove(m_backupPath);
            DebugLogger::instance().log("MainWindow",
                QString("Backup removed: %1").arg(m_backupPath));
        }
        m_backupPath.clear();
    }
    m_backupEnabled = true;
}

void MainWindow::checkForCrashBackups()
{
    if (!m_contestEngine || m_contestDefinition.isEmpty())
        return;

    QString callsign    = sanitizeForFilename(getSessionCallsign());
    QString contestName = sanitizeForFilename(m_contestEngine->getContestName());
    if (callsign.isEmpty() || contestName.isEmpty())
        return;

    QString pattern = callsign + "_" + contestName + "_*.bak";
    QDir tempDir(QDir::tempPath());
    QStringList bakFiles = tempDir.entryList({pattern}, QDir::Files, QDir::Time);

    if (bakFiles.isEmpty())
        return;

    QStringList bakPaths;
    for (const QString& f : bakFiles)
        bakPaths.append(tempDir.filePath(f));

    // Show restore dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Crash Recovery Backup Found");
    dialog.setMinimumWidth(520);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        QString("%1 backup log file(s) found for this contest.\n"
                "Select one to restore, or choose an action below.").arg(bakFiles.count()),
        &dialog));

    QListWidget *listWidget = new QListWidget(&dialog);
    for (const QString& path : bakPaths) {
        QFileInfo fi(path);
        listWidget->addItem(fi.fileName() +
            "  (" + fi.lastModified().toString("yyyy-MM-dd HH:mm:ss") + ")");
    }
    if (!bakPaths.isEmpty())
        listWidget->setCurrentRow(0);
    layout->addWidget(listWidget);

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *restoreBtn = buttons->addButton("Restore Selected", QDialogButtonBox::AcceptRole);
    QPushButton *deleteBtn  = buttons->addButton("Delete All",        QDialogButtonBox::DestructiveRole);
    QPushButton *ignoreBtn  = buttons->addButton("Ignore",            QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    bool deleteAll  = false;
    int  restoreIdx = -1;

    connect(restoreBtn, &QPushButton::clicked, &dialog, [&]() {
        restoreIdx = listWidget->currentRow();
        dialog.accept();
    });
    connect(deleteBtn, &QPushButton::clicked, &dialog, [&]() {
        deleteAll = true;
        dialog.reject();
    });
    connect(ignoreBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();

    if (deleteAll) {
        for (const QString& path : bakPaths)
            QFile::remove(path);
        DebugLogger::instance().log("MainWindow", "Deleted all crash backup files");
        return;
    }

    if (restoreIdx < 0 || restoreIdx >= bakPaths.size())
        return;

    // Restore from selected backup
    QString selectedPath = bakPaths[restoreIdx];
    FileHandler fh;
    QList<QsoRecord> restoredQsos;
    QString contestFile, stationClass, contestVersion, stationClassExchangeName, stationClassExchangeId;
    QMap<QString, QString> userPromptValues;
    fh.loadClxWithContest(selectedPath, restoredQsos, contestFile, stationClass, contestVersion,
                          stationClassExchangeName, stationClassExchangeId, userPromptValues);

    // Restore station class and exchange data into the contest engine
    if (m_contestEngine) {
        if (!stationClass.isEmpty())
            m_contestEngine->setStationClass(stationClass);
        if (!stationClassExchangeName.isEmpty())
            m_contestEngine->setStationClassExchangeName(stationClassExchangeName);
        if (!stationClassExchangeId.isEmpty())
            m_contestEngine->setStationClassExchangeId(stationClassExchangeId);
        if (!userPromptValues.isEmpty()) {
            for (auto it = userPromptValues.constBegin(); it != userPromptValues.constEnd(); ++it)
                m_contestEngine->setUserPromptValue(it.key(), it.value());
            applyRestrictedModeFromUserPrompts();
        }
    }

    // Restore restricted mode from the CLX metadata
    ClxFile clxRestore;
    if (clxRestore.load(selectedPath)) {
        QString loadedMode = clxRestore.contest().mode();
        if (!loadedMode.isEmpty() && m_contestEngine)
            m_contestEngine->setRestrictedMode(loadedMode);
    }

    m_qsoModel->clear();
    for (const QsoRecord& q : restoredQsos)
        m_qsoModel->addQso(q);

    m_isModified = true;
    updateWindowTitle();
    onRecalculateScore();
    m_statusLabel->setText(
        QString("Restored %1 QSOs from backup — please save your log").arg(restoredQsos.size()));

    // Delete all backup files now that we've restored
    for (const QString& path : bakPaths)
        QFile::remove(path);

    DebugLogger::instance().log("MainWindow",
        QString("Restored %1 QSOs from backup: %2").arg(restoredQsos.size()).arg(selectedPath));
}

QString MainWindow::generateSummaryString()
{
    QString summary;
    QTextStream out(&summary);
    
    // Header
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "CONTEST SUMMARY SHEET\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "\n";

    out << "Contest: " << m_contestEngine->getContestName() << "\n";
    out << "Callsign: " << getSessionCallsign() << "\n";
    
    // Calculate operating hours using offTimeGapMinutes from contest definition (default: 30 mins)
    double operatingHours = 0.0;
    if (m_qsoModel->rowCount() > 0) {
        int offTimeGapThreshold = 30; // Default fallback
        
        // Try to get offTimeGapMinutes from contest definition
        if (!m_contestDefinition.isEmpty() && m_contestDefinition.contains("contest")) {
            QJsonObject contestObj = m_contestDefinition["contest"].toObject();
            if (contestObj.contains("offTimeGapMinutes")) {
                offTimeGapThreshold = contestObj["offTimeGapMinutes"].toInt();
            }
        }
        
        QDateTime firstQsoTime = m_qsoModel->getQso(0).getDateTime();
        QDateTime lastQsoTime = m_qsoModel->getQso(m_qsoModel->rowCount() - 1).getDateTime();
        
        qint64 totalMinutes = firstQsoTime.secsTo(lastQsoTime) / 60;
        qint64 offTimeMinutes = 0;
        
        // Find gaps that count as off-time; use a 15-minute floor when not specified
        if (offTimeGapThreshold <= 0)
            offTimeGapThreshold = 15;

        for (int i = 0; i < m_qsoModel->rowCount() - 1; ++i) {
            QDateTime currentQsoTime = m_qsoModel->getQso(i).getDateTime();
            QDateTime nextQsoTime = m_qsoModel->getQso(i + 1).getDateTime();
            qint64 gapMinutes = currentQsoTime.secsTo(nextQsoTime) / 60;

            if (gapMinutes >= offTimeGapThreshold) {
                offTimeMinutes += gapMinutes;
            }
        }
        
        qint64 onTimeMinutes = totalMinutes - offTimeMinutes;
        operatingHours = onTimeMinutes / 60.0;
    }
    
    out << "Operating Hours: " << QString::number(operatingHours, 'f', 1) << "\n";
    
    // Add club line if it's set in settings
    QString club = Settings::instance().getCabrilloClub();
    if (!club.isEmpty()) {
        out << "Club: " << club << "\n";
    }
    
    out << "Date: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    out << "\n";
    
    // Scoring Summary
    out << "SCORING SUMMARY\n";
    out << "-" << QString("-").repeated(63) << "-" << "\n";
    out << "\n";
    
    ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
    
    // Get QSO count
    int totalQsos = m_qsoModel->rowCount();
    int uniqueQsos = 0;
    for (int i = 0; i < totalQsos; ++i) {
        if (!m_qsoModel->getQso(i).isDupe()) {
            uniqueQsos++;
        }
    }
    
    out << "Total QSOs logged:        " << totalQsos << "\n";
    out << "Unique QSOs:              " << uniqueQsos << "\n";
    out << "Duplicates:               " << (totalQsos - uniqueQsos) << "\n";
    out << "\n";
    
    out << "Contact Points:           " << score.contactScore << "\n";
    
    // Determine which multipliers are being counted
    QStringList multTypes;
    
    // Handle Objective Multipliers (WFD-style)
    QString multType = m_contestEngine->getMultiplierType();
    if (multType == "objectiveMultipliers") {
        out << "\n";
        out << "OBJECTIVE MULTIPLIERS CLAIMED\n";
        out << "-" << QString("-").repeated(63) << "-" << "\n";
        
        // Get the OM options and selected ones
        QMap<QString, int> omOptions = m_contestEngine->getObjectiveMultiplierOptions();
        QStringList selected = m_contestEngine->getSelectedObjectiveMultipliers();
        
        int totalOMPoints = 0;
        if (!selected.isEmpty()) {
            for (const QString& omCode : selected) {
                if (omOptions.contains(omCode)) {
                    int points = omOptions[omCode];
                    totalOMPoints += points;
                    
                    // Find the label from the prompt definition
                    QString label = omCode;
                    if (m_contestDefinition.contains("userPrompts")) {
                        QJsonArray prompts = m_contestDefinition["userPrompts"].toArray();
                        for (const QJsonValue& promptVal : prompts) {
                            QJsonObject prompt = promptVal.toObject();
                            if (prompt["id"].toString() == "objectiveMultipliers") {
                                QJsonArray options = prompt["options"].toArray();
                                for (const QJsonValue& optVal : options) {
                                    QJsonObject opt = optVal.toObject();
                                    if (opt["value"].toString() == omCode) {
                                        label = opt["label"].toString();
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    
                    out << "  " << label << "\n";
                }
            }
            out << "\n";
            out << "Total Objective Multiplier Points: " << totalOMPoints << "\n";
            out << "Objective Multiplier: " << totalOMPoints << " + 1 = " << (totalOMPoints + 1) << "\n";
        } else {
            out << "  (No objectives completed)\n";
            out << "\n";
            out << "Objective Multiplier: 0 + 1 = 1\n";
        }
        out << "\n";
    } else {
        // Traditional multiplier display
        if (score.namedMultCount > 0) {
            QString namedLabel = m_contestEngine->getNamedMultsLabel();
            QString namedLine = namedLabel + ":";
            out << namedLine.leftJustified(26) << score.namedMultCount << "\n";
            multTypes.append(namedLabel);
        }
        if (score.dxccMultCount > 0) {
            // eadx100 mults flow into dxccMultCount but should display as
            // EADX-100 when the contest uses the URE list rather than ARRL DXCC.
            QStringList engineCats = m_contestEngine->getMultiplierCategories();
            const bool isEadx100 = engineCats.contains("eadx100") && !engineCats.contains("dxcc");
            const QString label = isEadx100 ? "EADX-100 Multipliers:    " : "DXCC Multipliers:         ";
            out << label << score.dxccMultCount << "\n";
            multTypes.append(isEadx100 ? "EADX-100" : "DXCC");
        }
        if (score.ituRegionMultCount > 0) {
            out << "ITU Region Multipliers:   " << score.ituRegionMultCount << "\n";
            multTypes.append("ITU Region");
        }
        if (score.namedCallPrefixCount > 0) {
            out << "Call Prefix Multipliers:  " << score.namedCallPrefixCount << "\n";
            multTypes.append("Call Prefix");
        }
        if (score.gridSquareMultCount > 0) {
            out << "Grid Square Multipliers:  " << score.gridSquareMultCount << "\n";
            multTypes.append("Grid Square");
        }

        out << "\n";
    }
    
    // Show the scoring calculation
    if (multType == "objectiveMultipliers") {
        int omTotal = score.objectiveMultiplierCount;
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × (" << omTotal << " OM + 1)";
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    } else if (multTypes.size() == 1) {
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × " << score.multipliers << " multipliers";
        if (score.scoreMultiplier > 1) {
            out << " × " << score.scoreMultiplier << " (power)";
        }
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    } else if (multTypes.size() > 1) {
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × " << score.multipliers << " multipliers";
        out << " (" << multTypes.join(" + ") << ")";
        if (score.scoreMultiplier > 1) {
            out << " × " << score.scoreMultiplier << " (power)";
        }
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    }
    
    out << "\n";
    out << "CLAIMED SCORE: " << score.contestScore << "\n";
    out << "\n";

    // Band/Mode QSO Breakdown
    {
        // Get contest bands in order
        QStringList contestBands;
        if (m_contestDefinition.contains("contest")) {
            QJsonArray bandsArray = m_contestDefinition["contest"].toObject()["bands"].toArray();
            for (const QJsonValue& val : bandsArray)
                contestBands.append(val.toString());
        }
        if (contestBands.isEmpty())
            contestBands << "160m" << "80m" << "40m" << "20m" << "15m" << "10m";

        // Tally QSOs per band and mode category (CW, PH, DIG)
        QMap<QString, int> cwByBand, phByBand, digByBand;
        for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
            QsoRecord qso = m_qsoModel->getQso(i);
            if (qso.isDupe()) continue;
            QString band = qso.getBand();
            QString mode = qso.getMode().toUpper();
            if (mode == "CW")
                cwByBand[band]++;
            else if (mode == "USB" || mode == "LSB" || mode == "SSB" || mode == "FM" || mode == "AM")
                phByBand[band]++;
            else
                digByBand[band]++;
        }

        out << "BAND/MODE BREAKDOWN\n";
        out << "-" << QString("-").repeated(63) << "-" << "\n";
        out << QString("%1  %2  %3  %4\n")
               .arg("Band", -6).arg("CW Qs", 6).arg("Ph Qs", 6).arg("Dig Qs", 6);
        out << QString("-").repeated(30) << "\n";

        int totalCW = 0, totalPH = 0, totalDIG = 0;
        for (const QString& band : contestBands) {
            int cw = cwByBand.value(band, 0);
            int ph = phByBand.value(band, 0);
            int dg = digByBand.value(band, 0);
            totalCW += cw;
            totalPH += ph;
            totalDIG += dg;

            // Strip trailing 'm' for display (e.g., "160m" -> "160")
            QString bandLabel = band;
            if (bandLabel.endsWith('m')) bandLabel.chop(1);

            out << QString("%1  %2  %3  %4\n")
                   .arg(bandLabel, -6)
                   .arg(cw ? QString::number(cw) : "", 6)
                   .arg(ph ? QString::number(ph) : "", 6)
                   .arg(dg ? QString::number(dg) : "", 6);
        }

        out << QString("-").repeated(30) << "\n";
        out << QString("%1  %2  %3  %4\n")
               .arg("Total", -6)
               .arg(totalCW, 6).arg(totalPH, 6).arg(totalDIG, 6);
        out << "\n";
    }

    // Multiplier Details (skip for objectiveMultipliers - already shown above)
    if (multType != "objectiveMultipliers") {
        out << "MULTIPLIER DETAILS\n";
        out << "-" << QString("-").repeated(63) << "-" << "\n";
        {
            QString multRule;
            if      (multType == "multsOnce")         multRule = "Each multiplier counted once for the entire contest";
            else if (multType == "multsPerBand")       multRule = "Each multiplier counted once per band";
            else if (multType == "multsPerMode")       multRule = "Each multiplier counted once per mode";
            else if (multType == "multsPerBandAndMode") multRule = "Each multiplier counted once per band and mode";
            if (!multRule.isEmpty())
                out << "Rule: " << multRule << "\n";
        }
        out << "\n";

        QStringList multCategories = m_contestEngine->getMultiplierCategories();

        // Check if callsigns are multipliers (e.g., CWops CWT)
        bool callsignIsMult = false;
        if (m_contestDefinition.contains("multipliers")) {
            QJsonObject multipliers = m_contestDefinition["multipliers"].toObject();
            if (multipliers.contains("sources")) {
                QJsonArray sources = multipliers["sources"].toArray();
                for (const QJsonValue& val : sources) {
                    QJsonObject source = val.toObject();
                    if (source["type"].toString() == "callsign") {
                        callsignIsMult = true;
                        break;
                    }
                }
            }
        }

        // Handle callsign multipliers
        if (callsignIsMult) {
            QSet<QString> workedCalls;

            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                if (qso.getPoints() == 0) continue;
                workedCalls.insert(qso.getCall().toUpper());
            }

            if (!workedCalls.isEmpty()) {
                out << "Callsigns (Worked: " << workedCalls.size() << ")\n";

                QStringList sortedCalls = QStringList(workedCalls.begin(), workedCalls.end());
                std::sort(sortedCalls.begin(), sortedCalls.end());

                // Format with ~80 chars per line
                QString line;
                for (const QString& call : sortedCalls) {
                    QString entry = QString("%1 ").arg(call);

                    if ((line + entry).length() > 80 && !line.isEmpty()) {
                        out << line << "\n";
                        line.clear();
                    }
                    line += entry;
                }

                // Write remaining line
                if (!line.isEmpty()) {
                    out << line << "\n";
                }
                out << "\n";
            }
        }

        // Process each multiplier category
        for (const QString& category : multCategories) {
        if (multType == "multsOnce") {
            // Simple case: just list all mults for this category
            QSet<QString> workedMults;

            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                if (qso.getPoints() == 0) continue;
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);

                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category)
                        workedMults.insert(mult.value);
                }
            }

            // Automatic multipliers (e.g. WVQP credits "WV" to WV stations)
            // are earned without a QSO — include them in the named-mult list
            // so the detail count matches the scoring summary.
            if (category == "named" || category == "namedMults") {
                const QStringList autoMults = m_contestEngine->getAutomaticMultipliers();
                for (const QString& am : autoMults)
                    workedMults.insert(am);
            }

            if (!workedMults.isEmpty()) {
                QString categoryDisplay = (category == "named" || category == "namedMults") ? m_contestEngine->getNamedMultsLabel() : (category == "dxcc") ? "DXCC Entities" : (category == "eadx100") ? "EADX-100 Entities" : (category == "namedCallPrefixes" || category == "wpxPrefix") ? "Call Prefixes" : (category == "gridSquares") ? "Grid Squares" : category;
                out << categoryDisplay << " (Worked: " << workedMults.size() << ")\n";

                QStringList sortedMults = QStringList(workedMults.begin(), workedMults.end());
                std::sort(sortedMults.begin(), sortedMults.end());

                for (int i = 0; i < sortedMults.size(); ++i) {
                    out << QString("%1 ").arg(QString("%1").arg(sortedMults[i], -4));

                    if ((i + 1) % 6 == 0) {
                        out << "\n";
                    }
                }
                if (sortedMults.size() % 6 != 0) {
                    out << "\n";
                }
                out << "\n";
            }
        } else if (multType == "multsPerBand") {
            // Breakdown by band
            QMap<QString, QSet<QString>> multsPerBand;

            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                if (qso.getPoints() == 0) continue;
                QString band = qso.getBand();
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);

                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category)
                        multsPerBand[band].insert(mult.value);
                }
            }

            // Automatic multipliers (e.g. ACQP credits each AC operator's own
            // province per band where they worked any AC station) are credited
            // by the engine into m_workedNamedMultsPerBand without an
            // originating QSO. Surface them in the named-mult per-band display
            // so the printed list matches the scoring summary's count.
            if (category == "named" || category == "namedMults") {
                const QStringList autoMults = m_contestEngine->getAutomaticMultipliers();
                if (!autoMults.isEmpty()) {
                    QSet<QString> autoSet(autoMults.begin(), autoMults.end());
                    const QSet<QString> workedPerBand = m_contestEngine->getWorkedNamedMultsPerBand();
                    for (const QString& key : workedPerBand) {
                        int idx = key.indexOf('_');
                        if (idx < 0) continue;
                        QString mult = key.left(idx);
                        QString band = key.mid(idx + 1);
                        if (autoSet.contains(mult))
                            multsPerBand[band].insert(mult);
                    }
                }
            }

            for (const auto& band : multsPerBand.keys()) {
                QString categoryDisplay = (category == "named" || category == "namedMults") ? m_contestEngine->getNamedMultsLabel() : (category == "dxcc") ? "DXCC Entities" : (category == "eadx100") ? "EADX-100 Entities" : (category == "namedCallPrefixes" || category == "wpxPrefix") ? "Call Prefixes" : (category == "gridSquares") ? "Grid Squares" : category;
                out << categoryDisplay << " - " << band << " (Worked: " << multsPerBand[band].size() << ")\n";

                QStringList sortedMults = QStringList(multsPerBand[band].begin(), multsPerBand[band].end());
                std::sort(sortedMults.begin(), sortedMults.end());

                for (int i = 0; i < sortedMults.size(); ++i) {
                    out << QString("%1 ").arg(QString("%1").arg(sortedMults[i], -4));

                    if ((i + 1) % 6 == 0) {
                        out << "\n";
                    }
                }
                if (sortedMults.size() % 6 != 0) {
                    out << "\n";
                }
                out << "\n";
            }
        } else if (multType == "multsPerMode") {
            // Breakdown by mode. Normalize the QSO's literal mode value to
            // the same {CW, SSB, DIGITAL} mode-categories the scoring engine
            // uses (see ContestEngine::updateRunningScore) — otherwise SSB
            // QSOs split into LSB / USB sections in the printout, where the
            // same multiplier (e.g. INMRN) appears under both because the
            // operator worked LSB on 80m AND USB on 20m, double-counting in
            // the printed total even though the actual score correctly counts
            // it once. Keeps display consistent with score regardless of the
            // contest's mode mix.
            auto normalizeMode = [](const QString& m) -> QString {
                const QString u = m.toUpper();
                if (u == "CW") return "CW";
                if (u == "SSB" || u == "USB" || u == "LSB" || u == "FM" || u == "AM") return "SSB";
                if (u == "RTTY" || u == "PSK" || u == "FT8" || u == "FT4" || u == "DIGITAL") return "DIGITAL";
                return u;
            };

            QMap<QString, QSet<QString>> multsPerMode;

            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                if (qso.getPoints() == 0) continue;
                QString mode = normalizeMode(qso.getMode());
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);

                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category)
                        multsPerMode[mode].insert(mult.value);
                }
            }

            // Automatic multipliers (e.g. ALQP credits "AL" to Alabama
            // operators per mode where they worked any AL station) are
            // credited by the engine into m_workedNamedMultsPerMode without
            // an originating QSO. Surface them in the named-mult per-mode
            // display so the printed list matches the scoring summary count.
            if (category == "named" || category == "namedMults") {
                const QStringList autoMults = m_contestEngine->getAutomaticMultipliers();
                if (!autoMults.isEmpty()) {
                    QSet<QString> autoSet(autoMults.begin(), autoMults.end());
                    const QSet<QString> workedPerMode = m_contestEngine->getWorkedNamedMultsPerMode();
                    for (const QString& key : workedPerMode) {
                        int idx = key.indexOf('_');
                        if (idx < 0) continue;
                        QString mult = key.left(idx);
                        QString mode = key.mid(idx + 1);
                        if (autoSet.contains(mult))
                            multsPerMode[mode].insert(mult);
                    }
                }
            }

            for (const auto& mode : multsPerMode.keys()) {
                QString categoryDisplay = (category == "named" || category == "namedMults") ? m_contestEngine->getNamedMultsLabel() : (category == "dxcc") ? "DXCC Entities" : (category == "eadx100") ? "EADX-100 Entities" : (category == "namedCallPrefixes" || category == "wpxPrefix") ? "Call Prefixes" : (category == "gridSquares") ? "Grid Squares" : category;
                out << categoryDisplay << " - " << mode << " (Worked: " << multsPerMode[mode].size() << ")\n";

                QStringList sortedMults = QStringList(multsPerMode[mode].begin(), multsPerMode[mode].end());
                std::sort(sortedMults.begin(), sortedMults.end());

                for (int i = 0; i < sortedMults.size(); ++i) {
                    out << QString("%1 ").arg(QString("%1").arg(sortedMults[i], -4));

                    if ((i + 1) % 6 == 0) {
                        out << "\n";
                    }
                }
                if (sortedMults.size() % 6 != 0) {
                    out << "\n";
                }
                out << "\n";
            }
        } else if (multType == "multsPerBandAndMode") {
            // Breakdown by band and mode
            QMap<QString, QSet<QString>> multsPerBandMode;

            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                if (qso.getPoints() == 0) continue;
                QString band = qso.getBand();
                QString mode = qso.getMode();
                QString key = band + "/" + mode;
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);

                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category)
                        multsPerBandMode[key].insert(mult.value);
                }
            }

            for (const auto& key : multsPerBandMode.keys()) {
                QString categoryDisplay = (category == "named" || category == "namedMults") ? m_contestEngine->getNamedMultsLabel() : (category == "dxcc") ? "DXCC Entities" : (category == "eadx100") ? "EADX-100 Entities" : (category == "namedCallPrefixes" || category == "wpxPrefix") ? "Call Prefixes" : (category == "gridSquares") ? "Grid Squares" : category;
                out << categoryDisplay << " - " << key << " (Worked: " << multsPerBandMode[key].size() << ")\n";

                QStringList sortedMults = QStringList(multsPerBandMode[key].begin(), multsPerBandMode[key].end());
                std::sort(sortedMults.begin(), sortedMults.end());

                for (int i = 0; i < sortedMults.size(); ++i) {
                    out << QString("%1 ").arg(QString("%1").arg(sortedMults[i], -4));

                    if ((i + 1) % 6 == 0) {
                        out << "\n";
                    }
                }
                if (sortedMults.size() % 6 != 0) {
                    out << "\n";
                }
                out << "\n";
            }
        }
        }  // Close for loop over categories
    }  // Close if (multType != "objectiveMultipliers")

    // Bonus Station Details
    QList<ContestEngine::BonusStationGroup> bonusGroups = m_contestEngine->getBonusStationGroups();
    if (!bonusGroups.isEmpty()) {
        out << "\n";
        out << "BONUS STATION DETAILS\n";
        out << "-" << QString("-").repeated(63) << "-" << "\n";

        // Build set of worked callsigns from the QSO list
        QSet<QString> workedCalls;
        for (const QsoRecord& qso : m_qsoModel->getQsos())
            workedCalls.insert(qso.getCall().toUpper());

        for (const ContestEngine::BonusStationGroup& group : bonusGroups) {
            QStringList worked, missed;
            for (const QString& call : group.stations) {
                if (workedCalls.contains(call))
                    worked.append(call);
                else
                    missed.append(call);
            }
            worked.sort();
            missed.sort();

            // Count unique dedup keys (matching the engine's bonus accounting in
            // updateRunningScore). For bonusOnce this is just unique callsigns;
            // for bonusPerBand/bonusPerMode/bonusPerBandAndMode it can be higher
            // because the same bonus station credits separately on each band/mode.
            QSet<QString> seenKeys;
            for (const QsoRecord& qso : m_qsoModel->getQsos()) {
                QString call = qso.getCall().toUpper();
                if (!group.stations.contains(call)) continue;
                QString key;
                if      (group.type == "bonusOnce")           key = call;
                else if (group.type == "bonusPerBand")        key = call + "_" + qso.getBand();
                else if (group.type == "bonusPerMode")        key = call + "_" + qso.getMode().toUpper();
                else /* bonusPerBandAndMode */                key = call + "_" + qso.getBand() + "_" + qso.getMode().toUpper();
                seenKeys.insert(key);
            }
            int groupBonus = seenKeys.size() * group.pointsEach;
            out << group.name << " (+" << group.pointsEach << " pts each";
            if      (group.type == "bonusOnce")         out << ", once overall";
            else if (group.type == "bonusPerBand")       out << ", once per band";
            else if (group.type == "bonusPerMode")       out << ", once per mode";
            else if (group.type == "bonusPerBandAndMode") out << ", once per band+mode";
            out << "): " << groupBonus << " bonus pts\n";
            if (!worked.isEmpty())
                out << "  Worked (" << worked.size() << "): " << worked.join("  ") << "\n";
            if (!missed.isEmpty())
                out << "  Not worked (" << missed.size() << "): " << missed.join("  ") << "\n";
            out << "\n";
        }
    }

    out << "Generated by: ContestLogX " << QApplication::applicationVersion() << "\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    
    return summary;
}

void MainWindow::onCreateSummarySheet()
{
    DebugLogger::instance().log("MainWindow", 
        QString("onCreateSummarySheet: m_contestDefinition.isEmpty()=%1")
        .arg(m_contestDefinition.isEmpty() ? "true" : "false"));
    
    if (m_contestDefinition.isEmpty()) {
        QMessageBox::warning(this, "No Contest", "No contest is currently loaded");
        return;
    }
    
    if (m_qsoModel->rowCount() == 0) {
        QMessageBox::warning(this, "No QSOs", "No QSOs to export");
        return;
    }
    
    // Get save file location with default filename
    QString callsign = getSessionCallsign();
    QString contestName = m_contestEngine->getContestName();
    QString year = QString::number(QDate::currentDate().year());
    
    // Sanitize filenames - replace spaces and special chars with underscores
    QString sanitizedContest = contestName.toLower().replace(QRegularExpression("[^a-z0-9]+"), "_").remove(QRegularExpression("^_|_$"));
    QString sanitizedCall = callsign.toLower().replace(QRegularExpression("[^a-z0-9]+"), "_");
    
    QString defaultFilename = QString("%1_%2_%3.txt").arg(sanitizedCall, sanitizedContest, year);
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Summary Sheet", defaultFilename,
        "Text Files (*.txt);;All Files (*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot create file: " + file.errorString());
        return;
    }
    
    // Generate summary and write to file
    QString summaryText = generateSummaryString();
    QTextStream out(&file);
    out << summaryText;

    file.close();
    
    // Create a custom dialog with checkbox
    QDialog resultDialog(this);
    resultDialog.setWindowTitle("Summary Sheet Saved");
    resultDialog.setMinimumWidth(400);
    
    QVBoxLayout layout(&resultDialog);
    
    QLabel messageLabel(QString("Summary sheet saved to:\n%1").arg(fileName));
    layout.addWidget(&messageLabel);
    
    layout.addSpacing(10);
    
    QCheckBox viewCheckbox("View summary sheet now");
    viewCheckbox.setChecked(true);  // Default to checked
    layout.addWidget(&viewCheckbox);
    
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok);
    layout.addWidget(&buttonBox);
    
    connect(&buttonBox, &QDialogButtonBox::accepted, &resultDialog, &QDialog::accept);
    
    resultDialog.exec();
    
    // If user checked the box, open the file with default text editor
    if (viewCheckbox.isChecked()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
    }
}

void MainWindow::generateSummaryToDebugLog()
{
    if (m_contestDefinition.isEmpty()) {
        DebugLogger::instance().log("MainWindow", "generateSummaryToDebugLog: No contest is currently loaded");
        return;
    }
    
    if (m_qsoModel->rowCount() == 0) {
        DebugLogger::instance().log("MainWindow", "generateSummaryToDebugLog: No QSOs to export");
        return;
    }
    
    // Generate summary using the helper function
    QString summary = generateSummaryString();
    
    // Log the summary
    DebugLogger::instance().log("MainWindow", "=== SUMMARY SHEET START ===");
    for (const QString& line : summary.split('\n')) {
        DebugLogger::instance().log("MainWindow", line);
    }
    DebugLogger::instance().log("MainWindow", "=== SUMMARY SHEET END ===");
}

// ──────────────────────────────────────────────────────────────────────────────
// Online Score Publishing
// ──────────────────────────────────────────────────────────────────────────────

void MainWindow::onToggleOnlineScoring(bool enabled)
{
    if (!enabled) {
        // Disable
        if (m_scorePostTimer) m_scorePostTimer->stop();
        if (m_onlineScoringLabel) { m_onlineScoringLabel->hide(); m_onlineScoringSeparator->hide(); }
        DebugLogger::instance().log("OnlineScore", "Online scoring disabled");
        return;
    }

    // Check global enable setting
    if (!Settings::instance().getOnlineScoringEnabled()) {
        m_onlineScoringAction->setChecked(false);
        QMessageBox::information(this, "Online Scoring",
            "Online scoring is disabled. Enable it in Preferences > Online Scoring.");
        return;
    }

    // Validate required fields
    QStringList missing;
    QString osCall = Settings::instance().getOnlineScoringCallsign();
    QString osPass = Settings::instance().getOnlineScoringPassword();
    if (osCall.isEmpty()) missing << "Online Scoring Callsign (in Preferences)";
    if (osPass.isEmpty()) missing << "Online Scoring Password (in Preferences)";

    if (m_sessionStationInfo) {
        if (m_sessionStationInfo->cqZone() <= 0) missing << "CQ Zone";
        if (m_sessionStationInfo->ituZone() <= 0) missing << "ITU Zone";
        if (m_sessionStationInfo->state().isEmpty()) missing << "State/Province";
        if (m_sessionStationInfo->grid().isEmpty()) missing << "Grid Square";
    } else {
        missing << "Station Info (not configured)";
    }

    // Check contest has contestOnlineScore block
    if (m_contestDefinition.isEmpty() ||
        !m_contestDefinition.contains("contestOnlineScore")) {
        missing << "Contest online score configuration (not available for this contest)";
    }

    if (!missing.isEmpty()) {
        m_onlineScoringAction->setChecked(false);
        QMessageBox::warning(this, "Cannot Enable Online Scoring",
            "The following required fields are missing:\n\n- " + missing.join("\n- "));
        return;
    }

    // Configure client
    m_onlineScoreClient->setCredentials(osCall, osPass);

    // Start timer
    if (Settings::instance().getOnlineScoringPerQso()) {
        DebugLogger::instance().log("OnlineScore", "Online scoring enabled (per-QSO mode)");
    } else {
        int intervalMs = Settings::instance().getOnlineScoringInterval() * 60 * 1000;
        m_scorePostTimer->setInterval(intervalMs);
        m_scorePostTimer->start();
        DebugLogger::instance().log("OnlineScore",
            QString("Online scoring enabled (every %1 min)").arg(Settings::instance().getOnlineScoringInterval()));
    }

    if (m_onlineScoringLabel) {
        m_onlineScoringLabel->setText("Score: posting...");
        m_onlineScoringLabel->show();
        m_onlineScoringSeparator->show();
    }

    // Post immediately on enable
    onPostScore();
}

void MainWindow::onPostScore()
{
    if (!m_onlineScoringAction || !m_onlineScoringAction->isChecked()) return;
    if (!m_contestEngine || !m_onlineScoreClient) return;
    if (m_onlineScoreClient->isPostInFlight()) return;

    ScorePostData data;

    // Contest ID
    QJsonObject osConfig = m_contestDefinition["contestOnlineScore"].toObject();
    data.contestId = osConfig["contestId"].toString();

    // Check for mode-dependent contest ID mapping
    if (osConfig.contains("contestIdMapping")) {
        QJsonObject mapping = osConfig["contestIdMapping"].toObject();
        for (auto it = mapping.begin(); it != mapping.end(); ++it) {
            QString promptId = it.key();
            QString promptValue = m_contestEngine->getUserPromptValue(promptId);
            QJsonObject idMap = it.value().toObject();
            if (idMap.contains(promptValue))
                data.contestId = idMap[promptValue].toString();
        }
    }

    // Station info
    data.callsign = getSessionCallsign();
    data.ops = data.callsign;
    data.club = Settings::instance().getCabrilloClub();

    // QTH from station info
    if (m_sessionStationInfo) {
        data.cqZone = m_sessionStationInfo->cqZone();
        data.ituZone = m_sessionStationInfo->ituZone();
        data.arrlSection = m_sessionStationInfo->arrlSection();
        data.stPrvOth = m_sessionStationInfo->state();
        data.grid = m_sessionStationInfo->grid();
    }

    // DXCC country from callsign lookup
    if (m_dxccDatabase) {
        auto entity = m_dxccDatabase->lookupCallsign(data.callsign);
        if (entity.dxcc > 0)
            data.dxccCountry = entity.primaryPrefix;
    }

    // Operating class from userPrompts
    QString powerPrompt = m_contestEngine->getUserPromptValue("powerCategory");
    if (powerPrompt == "HP") data.power = "HIGH";
    else if (powerPrompt == "LP") data.power = "LOW";
    else if (powerPrompt == "QRP") data.power = "QRP";

    QString opCat = m_contestEngine->getUserPromptValue("operatingCategory");
    if (opCat.contains("MULTI") || opCat.startsWith("MS") || opCat.startsWith("M2") || opCat.startsWith("MM")) {
        data.opsCategory = "MULTI-OP";
        if (opCat.startsWith("M2")) data.transmitter = "TWO";
        else if (opCat.startsWith("MM")) data.transmitter = "UNLIMITED";
    }

    // Mode from contest mode prompt or derive from QSOs
    QString modePrompt = m_contestEngine->getUserPromptValue("contestMode");
    if (modePrompt == "CW") data.mode = "CW";
    else if (modePrompt == "SSB" || modePrompt == "Phone") data.mode = "PH";
    else if (modePrompt == "RTTY") data.mode = "RY";
    else if (modePrompt == "DIGI" || modePrompt == "Digital") data.mode = "DG";
    // else stays "MIXED"

    // Score and breakdown
    ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
    data.totalScore = score.contestScore;

    // Get mult attributes from contest definition
    QString mult1Attr = osConfig["mult1Attribute"].toString();
    QString mult2Attr = osConfig["mult2Attribute"].toString();

    // Build band/mode breakdown from BandModeStats
    auto modeLabel = [](const QString& m) -> QString {
        if (m == "CW") return "CW";
        if (m == "SSB" || m == "USB" || m == "LSB" || m == "FM" || m == "AM") return "PH";
        if (m == "RTTY") return "RY";
        return "DG";
    };

    // Tally QSOs, points, and mults per band/mode
    struct BandModeTally {
        int qsos = 0;
        int points = 0;
        QMap<QString, QSet<QString>> multSets; // attr -> set of unique mults
    };
    QMap<QString, BandModeTally> tallies; // key = "band/mode"

    int totalQsos = 0, totalPoints = 0;
    QMap<QString, QSet<QString>> totalMultSets;

    for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
        QsoRecord qso = m_qsoModel->getQso(i);
        if (qso.isDupe()) continue;

        QString band = qso.getBand();
        if (band.endsWith('m')) band.chop(1); // "40m" -> "40"
        QString mode = modeLabel(qso.getMode());
        QString key = band + "/" + mode;

        tallies[key].qsos++;
        tallies[key].points += qso.getPoints();
        totalQsos += 1;
        totalPoints += qso.getPoints();

        // Multipliers
        QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);
        for (const auto& mult : mults) {
            QString attr;
            if (mult.category == "namedMults") attr = mult1Attr;
            else if (mult.category == "dxcc") attr = (mult2Attr.isEmpty() ? mult1Attr : mult2Attr);
            else if (mult.category == "gridSquares") attr = "gridsquare";
            if (!attr.isEmpty()) {
                tallies[key].multSets[attr].insert(mult.value);
                totalMultSets[attr].insert(mult.value);
            }
        }
    }

    // Convert tallies to breakdown entries
    for (auto it = tallies.constBegin(); it != tallies.constEnd(); ++it) {
        QStringList parts = it.key().split('/');
        ScoreBreakdownEntry entry;
        entry.band = parts[0];
        entry.mode = parts[1];
        entry.qsoCount = it->qsos;
        entry.points = it->points;
        for (auto mit = it->multSets.constBegin(); mit != it->multSets.constEnd(); ++mit)
            entry.mults[mit.key()] = mit.value().size();
        data.breakdown.append(entry);
    }

    // Totals row
    ScoreBreakdownEntry totals;
    totals.band = "total";
    totals.mode = "ALL";
    totals.qsoCount = totalQsos;
    totals.points = totalPoints;
    for (auto mit = totalMultSets.constBegin(); mit != totalMultSets.constEnd(); ++mit)
        totals.mults[mit.key()] = mit.value().size();
    data.breakdown.append(totals);

    m_onlineScoreClient->postScore(data);
}

void MainWindow::onScorePostSuccess(const QString& timestamp)
{
    if (m_onlineScoringLabel) {
        m_onlineScoringLabel->setText(QString("Score: %1 UTC").arg(timestamp));
        m_onlineScoringLabel->setStyleSheet("");
    }
}

void MainWindow::onScorePostFailed(const QString& error)
{
    if (m_onlineScoringLabel) {
        m_onlineScoringLabel->setText("Score: Error");
        m_onlineScoringLabel->setToolTip(error);
        m_onlineScoringLabel->setStyleSheet("color: red;");
    }
    DebugLogger::instance().log("OnlineScore", QString("Post failed: %1").arg(error));
}

void MainWindow::onScorePostAuthFailed()
{
    // Auto-disable
    if (m_onlineScoringAction) m_onlineScoringAction->setChecked(false);
    if (m_scorePostTimer) m_scorePostTimer->stop();
    if (m_onlineScoringLabel) { m_onlineScoringLabel->hide(); m_onlineScoringSeparator->hide(); }

    QMessageBox::warning(this, "Online Scoring Disabled",
        "Online scoring has been disabled after 3 consecutive authentication failures.\n\n"
        "Please check your online scoring credentials in Preferences, then re-enable from the Contest menu.");
}

QString MainWindow::getSessionCallsign() const
{
    if (m_sessionStationInfo && !m_sessionStationInfo->callsign().isEmpty()) {
        return m_sessionStationInfo->callsign();
    }
    // Fallback to Settings if session info not set
    return Settings::instance().getCallsign();
}

// ---------- SO2R support ----------

RigInterface* MainWindow::activeRigClient() const
{
    if (m_so2rEnabled && m_activeRadio == ActiveRadio::Right && m_rigClientR)
        return m_rigClientR;
    return m_rigClient;
}

CwKeyerInterface* MainWindow::activeKeyer() const
{
    const bool right = (m_so2rEnabled && m_activeRadio == ActiveRadio::Right);
    WinKeyerClient* wk = right ? m_winKeyerR : m_winKeyerL;
    if (wk && wk->isConnected())
        return wk;
    return activeRigClient();
}

void MainWindow::setupCwKeyers()
{
    Settings& settings = Settings::instance();

    auto setup = [&](WinKeyerClient*& keyer, bool right) {
        // Tear down any existing keyer for this radio first.
        if (keyer) {
            disconnect(keyer, nullptr, this, nullptr);
            keyer->closePort();
            keyer->deleteLater();
            keyer = nullptr;
        }
        const QString source = settings.getCwKeyerSource(right);
        const QString port = settings.getCwKeyerPort(right);
        DebugLogger::instance().log("MainWindow",
            QString("setupCwKeyers %1: source=%2 port=%3")
                .arg(right ? "R" : "L").arg(source, port));
        if (source != "winkeyer" || port.isEmpty())
            return;

        // A configured WinKeyer is meant to be used, so connect it. Connect
        // asynchronously so the boot-wait/handshake never freezes the UI;
        // re-route the console once it actually connects.
        keyer = new WinKeyerClient(this);
        const QString side = right ? "R" : "L";
        connect(keyer, &WinKeyerClient::connected, this, [this, side]() {
            DebugLogger::instance().log("MainWindow",
                QString("WinKeyer (%1) connected").arg(side));
            updateCwConsoleRouting();
        });
        connect(keyer, &WinKeyerClient::error, this, [side](const QString& e) {
            DebugLogger::instance().log("MainWindow",
                QString("WinKeyer (%1) error: %2").arg(side, e));
        });
        keyer->connectAsync(port);
    };

    setup(m_winKeyerL, false);
    setup(m_winKeyerR, true);
}

void MainWindow::updateCwConsoleRouting()
{
    if (!m_cwConsole) return;
    // Rig client identifies the active radio (for decoder muting); the keyer
    // (WinKeyer if configured and connected, else the rig) does the sending.
    m_cwConsole->setRigClient(activeRigClient());
    CwKeyerInterface* k = activeKeyer();
    m_cwConsole->setKeyer(k);
    const bool viaWinKeyer = (k != static_cast<CwKeyerInterface*>(activeRigClient()));
    DebugLogger::instance().log("MainWindow",
        QString("CW routing -> %1 (winKeyerL=%2 connected=%3)")
            .arg(viaWinKeyer ? "WinKeyer" : "rig")
            .arg(m_winKeyerL != nullptr)
            .arg(m_winKeyerL && m_winKeyerL->isConnected()));
}

double MainWindow::activeFrequency() const
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_lastFrequencyR : m_lastFrequency;
}

QString MainWindow::activeMode() const
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_lastModeR : m_lastMode;
}

int MainWindow::activeWpm() const
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_lastWpmR : m_lastWpm;
}

QLineEdit* MainWindow::activeCallEdit() const
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_entryWidgetsR.callEdit : m_callEdit;
}

QLineEdit* MainWindow::activeExchangeEdit() const
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_entryWidgetsR.exchangeEdit : m_exchangeEdit;
}

QMap<QString, QLineEdit*>& MainWindow::activeExchangeFields()
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_entryWidgetsR.exchangeFields : m_exchangeFields;
}

QList<QLineEdit*>& MainWindow::activeEntryFieldOrder()
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_entryWidgetsR.entryFieldOrder : m_entryFieldOrder;
}

MainWindow::RunMode MainWindow::activeRunMode() const
{
    return (m_so2rEnabled && m_activeRadio == ActiveRadio::Right) ? m_runModeR : m_runMode;
}

void MainWindow::setActiveRunMode(RunMode mode)
{
    if (m_so2rEnabled && m_activeRadio == ActiveRadio::Right)
        m_runModeR = mode;
    else
        m_runMode = mode;
}

void MainWindow::switchActiveRadio()
{
    if (!m_so2rEnabled) return;

    m_activeRadio = (m_activeRadio == ActiveRadio::Left) ? ActiveRadio::Right : ActiveRadio::Left;

    // Route CW/TTS to the active radio
    updateCwConsoleRouting();
    if (m_ttsManager)
        m_ttsManager->setRigClient(activeRigClient());

    updateActiveRadioIndicator();

    // Move focus to the active radio's call edit
    QLineEdit* callEdit = activeCallEdit();
    if (callEdit) {
        callEdit->setFocus();
        callEdit->selectAll();
    }

    QString radioName = (m_activeRadio == ActiveRadio::Left) ? "Radio L" : "Radio R";
    QString switchKey = Settings::instance().getShortcut("switchRadio");
    if (switchKey.isEmpty()) switchKey = "`";
    m_statusLabel->setText(QString("Active: %1 — %2 to switch").arg(radioName, switchKey));
    DebugLogger::instance().log("MainWindow", QString("Switched active radio to %1").arg(radioName));
}

void MainWindow::updateActiveRadioIndicator()
{
    if (!m_so2rEnabled) return;

    QString activeGroupStyle = "QGroupBox { border: 2px solid #4CAF50; border-radius: 4px; margin-top: 0.5em; }"
                               "QGroupBox::title { color: #4CAF50; }";
    QString inactiveGroupStyle = "QGroupBox { border: 1px solid gray; border-radius: 4px; margin-top: 0.5em; }";

    if (m_entryWidgets.entryGroup)
        m_entryWidgets.entryGroup->setStyleSheet(
            m_activeRadio == ActiveRadio::Left ? activeGroupStyle : inactiveGroupStyle);
    if (m_entryWidgetsR.entryGroup)
        m_entryWidgetsR.entryGroup->setStyleSheet(
            m_activeRadio == ActiveRadio::Right ? activeGroupStyle : inactiveGroupStyle);

    // Green title bar on the active dock, default on inactive
    QString activeDockStyle = "QDockWidget::title { background: #4CAF50; color: white; padding: 4px; }";
    QString inactiveDockStyle = "QDockWidget::title { }";

    if (m_entryDock)
        m_entryDock->setStyleSheet(
            m_activeRadio == ActiveRadio::Left ? activeDockStyle : inactiveDockStyle);
    if (m_entryDockR)
        m_entryDockR->setStyleSheet(
            m_activeRadio == ActiveRadio::Right ? activeDockStyle : inactiveDockStyle);
}

void MainWindow::createRadioRRigClient()
{
    Settings& settings = Settings::instance();
    m_rigBackendR = settings.getRadioRRigBackend();
    if (m_rigBackendR == "hamlib") {
        m_rigClientR = new HamlibClient(this);
    } else if (m_rigBackendR == "mocked") {
        m_rigClientR = new MockedRigClient(this);
    } else {
        m_rigClientR = new FlrigClient(this);
        m_rigBackendR = "flrig";
    }

    // Polling timer for Radio R
    m_rigPollTimerR = new QTimer(this);
    m_rigPollTimerR->setInterval(settings.getFlrigPollInterval());
    connect(m_rigPollTimerR, &QTimer::timeout, this, &MainWindow::onUpdateRigDisplayR);

    // Rig connections for Radio R
    connect(m_rigClientR, SIGNAL(connected()), this, SLOT(onRigConnectedR()));
    connect(m_rigClientR, SIGNAL(disconnected()), this, SLOT(onRigDisconnectedR()));

    DebugLogger::instance().log("MainWindow", QString("Created Radio R rig client: %1").arg(m_rigBackendR));
}

void MainWindow::destroyRadioRRigClient()
{
    if (m_rigPollTimerR) {
        m_rigPollTimerR->stop();
        delete m_rigPollTimerR;
        m_rigPollTimerR = nullptr;
    }
    if (m_rigClientR) {
        if (m_rigClientR->isConnected())
            m_rigClientR->disconnectFromRig();
        delete m_rigClientR;
        m_rigClientR = nullptr;
    }
    m_lastFrequencyR = 0.0;
    m_lastModeR.clear();
    m_lastWpmR = 0;
    DebugLogger::instance().log("MainWindow", "Destroyed Radio R rig client");
}

void MainWindow::wireEntryPanelConnections(EntryPanelWidgets& w, bool isRadioR)
{
    // Clicking Log on a radio's panel activates that radio first
    connect(w.logButton, &QPushButton::clicked, this, [this, isRadioR]() {
        if (m_so2rEnabled && isRadioR && m_activeRadio != ActiveRadio::Right) {
            m_activeRadio = ActiveRadio::Right;
            updateActiveRadioIndicator();
        } else if (m_so2rEnabled && !isRadioR && m_activeRadio != ActiveRadio::Left) {
            m_activeRadio = ActiveRadio::Left;
            updateActiveRadioIndicator();
        }
        onLogQso();
    });
    if (isRadioR) {
        connect(w.clearButton, &QPushButton::clicked, this, &MainWindow::clearEntryFormR);
    } else {
        connect(w.clearButton, &QPushButton::clicked, this, &MainWindow::clearEntryForm);
    }
    connect(w.qrzButton, &QPushButton::clicked, this, &MainWindow::onQrzLookup);

    // Run / S&P / Off toggle buttons
    connect(w.offButton, &QPushButton::clicked, this, [this, isRadioR]() {
        if (isRadioR) m_runModeR = RunMode::Off; else m_runMode = RunMode::Off;
        updateRunSPButtons();
    });
    connect(w.runButton, &QPushButton::clicked, this, [this, isRadioR]() {
        if (!validateRunSPRoles(RunMode::Run)) { updateRunSPButtons(); return; }
        if (isRadioR) m_runModeR = RunMode::Run; else m_runMode = RunMode::Run;
        updateRunSPButtons();
    });
    connect(w.spButton, &QPushButton::clicked, this, [this, isRadioR]() {
        if (!validateRunSPRoles(RunMode::SP)) { updateRunSPButtons(); return; }
        if (isRadioR) m_runModeR = RunMode::SP; else m_runMode = RunMode::SP;
        updateRunSPButtons();
    });
    connect(w.callEdit, &QLineEdit::textChanged, this, &MainWindow::onCallChanged);
    connect(w.exchangeEdit, &QLineEdit::textChanged, this, &MainWindow::onExchangeChanged);
}

void MainWindow::clearEntryFormR()
{
    if (!m_entryWidgetsR.callEdit) return;
    m_entryWidgetsR.callEdit->clear();
    m_entryWidgetsR.exchangeEdit->clear();
    for (auto it = m_entryWidgetsR.exchangeFields.begin(); it != m_entryWidgetsR.exchangeFields.end(); ++it) {
        if (it.value() != m_entryWidgetsR.callEdit)
            it.value()->clear();
    }
    m_exchangeSentR = false;
    m_entryWidgetsR.callEdit->setFocus();
}

void MainWindow::enableSo2r()
{
    if (m_so2rEnabled) return;
    m_so2rEnabled = true;

    // Create Radio R rig client
    createRadioRRigClient();

    // Create Radio R entry dock
    QWidget *entryPanelR = createEntryPanel(m_entryWidgetsR, "R");
    m_entryDockR = new QDockWidget("Radio R", this);
    m_entryDockR->setObjectName("entryDockR");
    m_entryDockR->setWidget(entryPanelR);
    m_entryDockR->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_entryDockR->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_entryDockR);

    // Wire Radio R return-to-dock and freq/mode button
    connect(m_entryWidgetsR.returnToDockLabel, &QLabel::linkActivated, this, [this]() {
        m_entryDockR->setFloating(false);
    });
    connect(m_entryWidgetsR.freqModeButton, &QPushButton::clicked, this, &MainWindow::onFreqModeButtonClicked);

    // Return Radio R to dock when floating window is closed/minimized (matches Radio L behaviour)
    connect(m_entryDockR, &QDockWidget::topLevelChanged, this, [this](bool floating) {
        if (m_entryWidgetsR.returnToDockLabel)
            m_entryWidgetsR.returnToDockLabel->setVisible(floating);
        if (floating) {
            QTimer::singleShot(0, this, [this]() {
                if (!m_entryDockR || !m_entryDockR->isFloating()) return;
                QWindow *win = m_entryDockR->window()->windowHandle();
                if (!win) return;
                win->setFlag(Qt::WindowMaximizeButtonHint, false);
                connect(win, &QWindow::visibilityChanged,
                        this, [this](QWindow::Visibility v) {
                    if (v == QWindow::Minimized || v == QWindow::Maximized || v == QWindow::Hidden)
                        QTimer::singleShot(0, this, [this]() {
                            if (m_entryDockR) m_entryDockR->setFloating(false);
                        });
                });
            });
        }
    });
    connect(m_entryDockR, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible && !m_restoringState && m_entryDockR)
            QTimer::singleShot(0, m_entryDockR, &QWidget::show);
    });

    // Wire Radio R entry panel connections
    wireEntryPanelConnections(m_entryWidgetsR, true);

    // Wire SCP to Radio R call field
    ScpLineEdit *scpLineEditR = qobject_cast<ScpLineEdit*>(m_entryWidgetsR.callEdit);
    if (scpLineEditR && m_scpWidget) {
        scpLineEditR->setScpWidget(m_scpWidget);
        scpLineEditR->setScpEnabled(Settings::instance().getScpEnabled());
    }

    // Relabel Radio L dock
    m_entryDock->setWindowTitle("Radio L");

    // Stack both entry docks vertically — both always visible
    splitDockWidget(m_entryDock, m_entryDockR, Qt::Vertical);

    // Set active radio to Left and show indicator
    m_activeRadio = ActiveRadio::Left;
    updateActiveRadioIndicator();

    // Auto-connect Radio R if configured
    Settings& settings = Settings::instance();
    bool autoConnect = false;
    QString host;
    int port = 0;
    if (m_rigBackendR == "hamlib") {
        autoConnect = settings.getRadioRHamlibAutoConnect();
        host = settings.getRadioRHamlibHost();
        port = settings.getRadioRHamlibPort();
    } else if (m_rigBackendR == "mocked") {
        autoConnect = settings.getRadioRMockedAutoConnect();
        host = "mocked";
    } else {
        autoConnect = settings.getRadioRFlrigAutoConnect();
        host = settings.getRadioRFlrigHost();
        port = settings.getRadioRFlrigPort();
    }
    if (autoConnect && m_rigClientR) {
        QTimer::singleShot(500, this, [this, host, port]() {
            if (m_rigClientR && m_rigClientR->connectToRig(host, port)) {
                onRigConnectedR();
            }
        });
    }

    // Update exchange fields for Radio R if a contest is loaded
    if (!m_contestDefinition.isEmpty()) {
        updateQsoEntryFields();
    }

    Settings::instance().setSo2rEnabled(true);
    updateRigStatusLabel();
    DebugLogger::instance().log("MainWindow", "SO2R mode enabled");
}

void MainWindow::disableSo2r()
{
    if (!m_so2rEnabled) return;

    // Switch to Radio L first
    m_activeRadio = ActiveRadio::Left;

    // Route CW/TTS back to Radio L
    if (m_cwConsole)
        m_cwConsole->setRigClient(m_rigClient);
    if (m_ttsManager)
        m_ttsManager->setRigClient(m_rigClient);

    // Destroy Radio R rig client
    destroyRadioRRigClient();

    // Remove Radio R entry dock
    if (m_entryDockR) {
        removeDockWidget(m_entryDockR);
        delete m_entryDockR;
        m_entryDockR = nullptr;
    }
    m_entryWidgetsR = EntryPanelWidgets();

    // Restore Radio L dock title and clear all SO2R styling
    m_entryDock->setWindowTitle("QSO Entry");
    m_entryDock->setStyleSheet("");
    if (m_entryWidgets.entryGroup)
        m_entryWidgets.entryGroup->setStyleSheet("");

    m_so2rEnabled = false;
    m_runModeR = RunMode::Off;
    m_exchangeSentR = false;

    // Shrink the entry dock back to its minimum height
    QWidget* entryWidget = m_entryDock->widget();
    if (entryWidget) {
        int minH = entryWidget->minimumSizeHint().height();
        m_entryDock->setFixedHeight(minH);
        // Release the fixed constraint after a layout pass so the dock remains resizable
        QTimer::singleShot(0, this, [this]() {
            m_entryDock->setMinimumHeight(0);
            m_entryDock->setMaximumHeight(QWIDGETSIZE_MAX);
        });
    }

    Settings::instance().setSo2rEnabled(false);
    updateRigStatusLabel();
    m_statusLabel->setText("SO2R disabled");
    DebugLogger::instance().log("MainWindow", "SO2R mode disabled");
}

void MainWindow::onToggleSo2r(bool enabled)
{
    if (enabled)
        enableSo2r();
    else
        disableSo2r();

    // Keep menu action in sync (may be triggered from rig dialog)
    if (m_so2rAction && m_so2rAction->isChecked() != enabled)
        m_so2rAction->setChecked(enabled);

    // Radio R decoder menu entry is only meaningful in SO2R mode.
    if (m_cwDecoderRightAction)
        m_cwDecoderRightAction->setVisible(enabled);
    // Refresh decoder spawning so a newly-enabled SO2R picks up Radio R audio
    // (and so a newly-disabled SO2R drops the Radio R decoder).
    spawnOrRefreshCwDecoders();
}

void MainWindow::onRigConnectedR()
{
    if (!m_rigClientR) return;
    m_rigPollTimerR->start();
    QString rigName = m_rigClientR->getRigName();
    DebugLogger::instance().log("MainWindow", QString("Radio R connected: %1").arg(rigName));
    m_statusLabel->setText(QString("Radio R connected: %1").arg(rigName.isEmpty() ? m_rigBackendR : rigName));
    updateRigStatusLabel();
}

void MainWindow::onRigDisconnectedR()
{
    if (m_rigPollTimerR)
        m_rigPollTimerR->stop();
    DebugLogger::instance().log("MainWindow", "Radio R disconnected");
    m_statusLabel->setText("Radio R disconnected");
    if (m_entryWidgetsR.freqModeButton)
        m_entryWidgetsR.freqModeButton->setText("--- ---");
    updateRigStatusLabel();
}

void MainWindow::onUpdateRigDisplayR()
{
    if (!m_rigClientR || !m_rigClientR->isConnected()) return;

    double freqHz = m_rigClientR->getFrequency();
    double freqKHz = freqHz / 1000.0;
    QString mode = m_rigClientR->getMode();
    int wpm = m_rigClientR->getCWSpeed();

    m_lastFrequencyR = freqKHz;
    if (!mode.isEmpty() && mode != m_lastModeR) {
        QString oldMode = m_lastModeR;
        m_lastModeR = mode;
        updateRstDefaults(oldMode, mode, m_entryWidgetsR.exchangeFields);
    }
    if (wpm > 0)
        m_lastWpmR = wpm;

    // Update Radio R freq/mode button
    if (m_entryWidgetsR.freqModeButton) {
        QString displayMode = m_lastModeR;
        m_entryWidgetsR.freqModeButton->setText(
            QString("%1 %2").arg(m_lastFrequencyR, 0, 'f', 1).arg(displayMode));
    }
}

void MainWindow::onRigBackendChangedR(const QString& backend)
{
    if (backend == m_rigBackendR) return;

    destroyRadioRRigClient();
    m_rigBackendR = backend;
    Settings::instance().setRadioRRigBackend(backend);
    createRadioRRigClient();

    DebugLogger::instance().log("MainWindow", QString("Radio R backend changed to %1").arg(backend));
}

// ---------- CW Decoder lifecycle (SPEC-005) ----------

void MainWindow::spawnOrRefreshCwDecoders()
{
    Settings& s = Settings::instance();

    auto ensureDecoder = [this, &s](bool right) {
        const QString device = right ? s.getRadioRAudioInputDevice()
                                     : s.getRadioLAudioInputDevice();
        CwDecoderWidget*& slot = right ? m_cwDecoderRight : m_cwDecoderLeft;
        QAction* action = right ? m_cwDecoderRightAction : m_cwDecoderLeftAction;

        if (device.isEmpty()) {
            // Operator has "(none)" — ensure no widget exists for this radio.
            // The menu entry remains visible and active; clicking it prompts
            // the operator to configure an audio device (see setupMenus).
            if (slot) {
                slot->endDecoding();
                removeDockWidget(slot);
                slot->deleteLater();
                slot = nullptr;
            }
            if (action) action->setChecked(false);
            return;
        }

        const bool wasNew = (slot == nullptr);
        if (wasNew) {
            slot = new CwDecoderWidget(
                right ? clx::audio::RadioSide::Right : clx::audio::RadioSide::Left, this);
            // Hand the widget a non-owning ContestEngine pointer so the
            // "Practice — Contest Exchange" source can pull the active
            // contest's exchange format and named-mult values.
            slot->setContestEngine(m_contestEngine);
            // Top dock area — sits above the QSO log. The TopRightCorner
            // was assigned to the right dock area in setupUi so this does
            // not extend over DX Cluster / Band Map / etc.
            addDockWidget(Qt::TopDockWidgetArea, slot);
            slot->hide();

            connect(slot, &CwDecoderWidget::callClicked,
                    this, &MainWindow::onDecoderCallClicked);
            connect(slot, &CwDecoderWidget::rstClicked,
                    this, &MainWindow::onDecoderRstClicked);

            // Keep the Window menu entry's checked state in sync with actual
            // dock visibility (e.g., operator closes the dock via the X).
            connect(slot, &QDockWidget::visibilityChanged, this,
                    [this, right](bool visible) {
                QAction* a = right ? m_cwDecoderRightAction : m_cwDecoderLeftAction;
                if (a) a->setChecked(visible);
            });

            // Wire PTT state from the owning rig client to the decoder.
            RigInterface* rig = right ? m_rigClientR : m_rigClient;
            if (rig) {
                connect(rig, &RigInterface::pttStateChanged, this,
                        [this, right, &s](bool active) {
                    const bool muteEnabled = right
                        ? s.getRadioRMuteDecoderOnPtt()
                        : s.getRadioLMuteDecoderOnPtt();
                    CwDecoderWidget* w = right ? m_cwDecoderRight : m_cwDecoderLeft;
                    if (w && muteEnabled) w->setPttMute(active);
                });
            }
        }
        // (Re)start decoding against the current device.
        slot->beginDecoding(device);
        slot->show();
        if (action) action->setChecked(true);
    };

    ensureDecoder(false);
    ensureDecoder(true);
}

void MainWindow::onAudioConfigChanged(bool isRightRadio)
{
    Q_UNUSED(isRightRadio);
    spawnOrRefreshCwDecoders();
}

void MainWindow::onDecoderCallClicked(const QString& callsign, int binIndex)
{
    Q_UNUSED(binIndex);
    // Route to the owning radio's CALL field. The widget that emitted the
    // signal is the sender(); use it to determine owning radio deterministically.
    CwDecoderWidget* w = qobject_cast<CwDecoderWidget*>(sender());
    if (!w) return;

    QLineEdit* target = nullptr;
    if (w->isRightRadio()) {
        target = m_entryWidgetsR.callEdit;
    } else {
        target = m_entryWidgets.callEdit;
    }
    if (!target) return;

    // Preserve focus — do NOT call setFocus() (FR-022 / Principle III).
    // setText fires the textChanged handler (onCallChanged), which runs
    // SCP / call-history / dupe check / exchange pre-fill. textEdited is
    // emitted explicitly for any handler that listens to it specifically.
    target->setText(callsign);
    emit target->textEdited(callsign);

    // The keyboard Space/Tab advance handler triggers QRZ/QRZCQ auto-lookup
    // when the operator leaves the CALL field. Click-fill doesn't take that
    // path, so mirror the lookup call here to keep behavior parity between
    // keyboard entry and click-fill.
    const QString clean = callsign.trimmed().toUpper();
    if (clean.length() >= 2) {
        triggerAutoLookup(clean);
    }

    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder",
            QString("CALL click-filled '%1' for %2 (triggered auto-lookup)")
                .arg(clean)
                .arg(w->isRightRadio() ? "Radio R" : "Radio L"));
    }
}

void MainWindow::onDecoderRstClicked(const QString& rst, int binIndex)
{
    Q_UNUSED(binIndex);
    CwDecoderWidget* w = qobject_cast<CwDecoderWidget*>(sender());
    if (!w) {
        DebugLogger::instance().log("CwDecoder",
            QString("onDecoderRstClicked('%1') — sender() is not a "
                    "CwDecoderWidget, ignoring").arg(rst));
        return;
    }

    const bool right = w->isRightRadio();
    // Radio L (default / non-SO2R) stores its exchange field pointers in
    // the top-level m_exchangeFields map — only Radio R uses the struct
    // member m_entryWidgetsR.exchangeFields. Route to the right source.
    const auto& exchangeMap = right ? m_entryWidgetsR.exchangeFields
                                    : m_exchangeFields;

    QLineEdit* target = nullptr;
    auto it = exchangeMap.find("RSTr");
    if (it != exchangeMap.end()) target = it.value();

    if (!target) {
        DebugLogger::instance().log("CwDecoder",
            QString("onDecoderRstClicked('%1') from %2 — no 'RSTr' exchange "
                    "field in current contest (map has %3 keys); click has "
                    "nowhere to fill")
                .arg(rst)
                .arg(right ? "Radio R" : "Radio L")
                .arg(exchangeMap.size()));
        QStringList keys;
        for (auto k = exchangeMap.cbegin(); k != exchangeMap.cend(); ++k) {
            keys << k.key();
        }
        DebugLogger::instance().log("CwDecoder",
            QString("  Available exchange fields: %1").arg(keys.join(", ")));
        return;
    }

    if (DebugLogger::instance().isCwDecoderDebugEnabled()) {
        DebugLogger::instance().log("CwDecoder",
            QString("Filling RSTr with '%1' for %2 (was='%3')")
                .arg(rst)
                .arg(right ? "Radio R" : "Radio L")
                .arg(target->text()));
    }

    target->setText(rst);
    emit target->textEdited(rst);
}

void MainWindow::notifyInternalCwSend(bool isRightRadio, int textChars, int sendWpm)
{
    CwDecoderWidget* w = isRightRadio ? m_cwDecoderRight : m_cwDecoderLeft;
    if (!w) return;

    const bool muteEnabled = isRightRadio
        ? Settings::instance().getRadioRMuteDecoderOnPtt()
        : Settings::instance().getRadioLMuteDecoderOnPtt();
    if (!muteEnabled) return;

    // Duration = text_chars × 60 / (WPM × 5) × 1000 + grace (per research R9).
    if (sendWpm <= 0) sendWpm = 25;
    const int baseMs = static_cast<int>(std::ceil(
        static_cast<double>(textChars) * 60.0 / (sendWpm * 5.0) * 1000.0));
    const int grace = isRightRadio
        ? Settings::instance().getRadioRDecoderPttGraceMs()
        : Settings::instance().getRadioLDecoderPttGraceMs();
    w->muteForInternalSend(baseMs + grace);
}

// =======================================================================
// Remote Control — HTTP server for LAN dashboards + minimal rig control.
// See TODO item 3 for the full scope.
// =======================================================================

QString MainWindow::ensureRemoteControlToken()
{
    Settings& s = Settings::instance();
    QString token = s.getRemoteControlToken();
    if (token.isEmpty()) {
        // 128-bit hex token from QUuid, no dashes or braces — short enough
        // to paste manually if needed, long enough that brute-forcing on
        // a LAN is not a realistic threat model.
        token = QUuid::createUuid().toString(QUuid::Id128);
        s.setRemoteControlToken(token);
    }
    return token;
}

void MainWindow::initRemoteControl()
{
    m_clxSnapshot = new clx::net::ClxSnapshot();
    m_httpServer  = new clx::net::HttpServer(m_clxSnapshot, this);

    connect(m_httpServer, &clx::net::HttpServer::errorOccurred, this,
            [](const QString& msg) {
                DebugLogger::instance().log("HttpServer", msg);
            });

    registerRemoteRoutes();
    updateSnapshotStatus();

    // Lightweight 2-second refresh timer for state the snapshot doesn't
    // naturally get pushed — current rig freq/mode and rate numbers.
    // Score/mults/QSOs are pushed directly from their update sites so
    // the dashboard reflects them instantly.
    auto* refresh = new QTimer(this);
    refresh->setInterval(2000);
    connect(refresh, &QTimer::timeout, this, [this]() {
        if (!m_clxSnapshot) return;
        updateSnapshotRig(false);
        if (m_so2rEnabled) updateSnapshotRig(true);
        updateSnapshotRate();
    });
    refresh->start();

    if (Settings::instance().getRemoteControlEnabled()) {
        ensureRemoteControlToken();
        m_httpServer->start();
    }
}

void MainWindow::updateSnapshotStatus()
{
    if (!m_clxSnapshot) return;
    m_clxSnapshot->setRunning(true);
    m_clxSnapshot->setContestName(m_contestEngine
                                  ? m_contestEngine->getContestName()
                                  : QString());
    m_clxSnapshot->setContestFile(m_contestFile);
    m_clxSnapshot->setSo2rEnabled(m_so2rEnabled);
    if (!m_clxSnapshot->copy().startedAt.isValid()) {
        m_clxSnapshot->setStartedAt(QDateTime::currentDateTimeUtc());
    }
}

void MainWindow::registerRemoteRoutes()
{
    if (!m_httpServer || !m_clxSnapshot) return;

    using clx::net::HttpRequest;
    using clx::net::HttpResponse;

    auto jsonResp = [](const QJsonObject& j) -> HttpResponse {
        HttpResponse r;
        r.body = QJsonDocument(j).toJson(QJsonDocument::Compact);
        return r;
    };

    auto rigToJson = [](const clx::net::RigSnapshot& r) {
        QJsonObject o;
        o["backend"]    = r.backend;
        o["connected"]  = r.connected;
        o["freqHz"]     = static_cast<double>(r.freqHz);
        o["mode"]       = r.mode;
        o["band"]       = r.band;
        o["pttActive"]  = r.pttActive;
        o["runSpMode"]  = r.runSpMode;
        return o;
    };

    // GET / — static mobile dashboard HTML, served from Qt resources.
    // Auth still applies: operator must hit the URL with ?token=<t>
    // (what the "Copy URL for Phone" button provides). The dashboard
    // then reads the token from window.location.search and appends it
    // to every subsequent /api/* fetch.
    auto dashboardHandler = [](const HttpRequest&) -> HttpResponse {
        QFile f(QStringLiteral(":/dashboard.html"));
        HttpResponse r;
        if (!f.open(QIODevice::ReadOnly)) {
            r.status = 500;
            r.body = QByteArray("{\"error\":\"dashboard not found\"}");
            return r;
        }
        r.body = f.readAll();
        r.contentType = QStringLiteral("text/html; charset=utf-8");
        return r;
    };
    m_httpServer->registerRoute("GET", "/",               dashboardHandler);
    m_httpServer->registerRoute("GET", "/dashboard.html", dashboardHandler);

    // GET /api/status — "is CLX up and what's it doing?"
    m_httpServer->registerRoute("GET", "/api/status",
        [this, jsonResp](const HttpRequest&) {
            const auto st = m_clxSnapshot->copy();
            QJsonObject j;
            j["running"]        = st.running;
            j["contestName"]    = st.contestName;
            j["contestFile"]    = st.contestFile;
            j["so2rEnabled"]    = st.so2rEnabled;
            j["startedAtUtc"]   = st.startedAt.isValid()
                                   ? st.startedAt.toString(Qt::ISODate)
                                   : QString();
            j["nowUtc"]         = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            j["version"]        = qApp->applicationVersion();
            return jsonResp(j);
        });

    // GET /api/score — score breakdown matching the score widget.
    m_httpServer->registerRoute("GET", "/api/score",
        [this, jsonResp](const HttpRequest&) {
            const auto st = m_clxSnapshot->copy();
            QJsonObject j;
            j["totalQsos"]     = st.score.totalQsos;
            j["totalPoints"]   = st.score.totalPoints;
            j["namedMults"]    = st.score.namedMults;
            j["dxccMults"]     = st.score.dxccMults;
            j["ituMults"]      = st.score.ituMults;
            j["gridMults"]     = st.score.gridMults;
            j["prefixMults"]   = st.score.prefixMults;
            j["finalScore"]    = static_cast<double>(st.score.finalScore);
            QJsonObject byBandMode;
            for (auto it = st.score.qsosByBandMode.begin(); it != st.score.qsosByBandMode.end(); ++it) {
                QJsonObject modes;
                for (auto mit = it.value().begin(); mit != it.value().end(); ++mit) {
                    modes[mit.key()] = mit.value();
                }
                byBandMode[it.key()] = modes;
            }
            j["qsosByBandMode"] = byBandMode;
            return jsonResp(j);
        });

    // GET /api/rate — rate numbers.
    m_httpServer->registerRoute("GET", "/api/rate",
        [this, jsonResp](const HttpRequest&) {
            const auto st = m_clxSnapshot->copy();
            QJsonObject j;
            j["currentHourlyRate"]   = st.rate.currentHourlyRate;
            j["lastHourRate"]        = st.rate.lastHourRate;
            j["sessionAverageRate"]  = st.rate.sessionAverageRate;
            return jsonResp(j);
        });

    // GET /api/qsos?limit=N&offset=N — recent QSOs newest-first.
    m_httpServer->registerRoute("GET", "/api/qsos",
        [this, jsonResp](const HttpRequest& req) {
            const auto st = m_clxSnapshot->copy();
            const int total = st.recentQsos.size();
            int limit  = req.query.value(QStringLiteral("limit"),  QStringLiteral("20")).toInt();
            int offset = req.query.value(QStringLiteral("offset"), QStringLiteral("0")).toInt();
            if (limit  <= 0) limit  = 20;
            if (limit  > 200) limit = 200;
            if (offset <  0) offset = 0;

            QJsonArray arr;
            // recentQsos is stored newest-last; emit newest-first for the UI.
            for (int i = 0; i < limit && offset + i < total; ++i) {
                const auto& q = st.recentQsos.at(total - 1 - offset - i);
                QJsonObject o;
                o["dateUtc"]  = q.dateUtc;
                o["timeUtc"]  = q.timeUtc;
                o["call"]     = q.call;
                o["freqHz"]   = static_cast<double>(q.freqHz);
                o["mode"]     = q.mode;
                o["rstSent"]  = q.rstSent;
                o["rstRcvd"]  = q.rstRcvd;
                o["exchSent"] = q.exchSent;
                o["exchRcvd"] = q.exchRcvd;
                o["points"]   = q.points;
                arr.append(o);
            }
            QJsonObject j;
            j["total"]  = total;
            j["limit"]  = limit;
            j["offset"] = offset;
            j["qsos"]   = arr;
            return jsonResp(j);
        });

    // GET /api/rig — freq/mode/band per radio.
    m_httpServer->registerRoute("GET", "/api/rig",
        [this, jsonResp, rigToJson](const HttpRequest&) {
            const auto st = m_clxSnapshot->copy();
            QJsonObject j;
            j["radioL"] = rigToJson(st.rigL);
            if (st.so2rEnabled) {
                j["radioR"] = rigToJson(st.rigR);
            }
            return jsonResp(j);
        });

    // GET /api/mults — worked named multipliers (states/sections/zones/etc).
    m_httpServer->registerRoute("GET", "/api/mults",
        [this, jsonResp](const HttpRequest&) {
            const auto st = m_clxSnapshot->copy();
            QJsonArray arr;
            for (const QString& m : st.workedNamedMults) arr.append(m);
            QJsonObject j;
            j["workedNamed"] = arr;
            j["count"]       = static_cast<int>(arr.size());
            return jsonResp(j);
        });

    // GET /api/propagation — cached NOAA SFI/A/K.
    m_httpServer->registerRoute("GET", "/api/propagation",
        [this, jsonResp](const HttpRequest&) {
            const auto st = m_clxSnapshot->copy();
            QJsonObject j;
            j["sfi"]        = st.propagation.sfi;
            j["aIndex"]     = st.propagation.aIndex;
            j["kIndex"]     = st.propagation.kIndex;
            j["fetchedAt"]  = st.propagation.fetchedAt.isValid()
                              ? st.propagation.fetchedAt.toUTC().toString(Qt::ISODate)
                              : QString();
            return jsonResp(j);
        });

    // ---------- V2 write endpoints (rig control) ----------
    //
    // Handlers run on the Qt main thread (HTTP server is event-loop-based).
    // Direct calls to the rig backend are safe here — consistent with every
    // other UI-driven rig call in MainWindow. Worst case a synchronous
    // flrig XML-RPC timeout (2s) briefly stalls UI; same exposure exists
    // for operator clicks today, so no regression.
    //
    // Snapshot doesn't need explicit push — the 2-second refresh timer in
    // initRemoteControl() catches up the new rig state on the next tick.

    // Shared body parser — returns 400 on malformed JSON, empty optional
    // means "use defaults or no field set".
    auto parseJsonBody = [jsonResp](const HttpRequest& req, QJsonObject& out) -> std::optional<HttpResponse> {
        if (req.body.isEmpty()) { out = QJsonObject{}; return std::nullopt; }
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(req.body, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            HttpResponse r;
            r.status = 400;
            r.body = QByteArray("{\"error\":\"body must be a JSON object\"}");
            return r;
        }
        out = doc.object();
        return std::nullopt;
    };

    // Resolve radio side from body. Default L. Rejects R when SO2R is off.
    auto resolveRig = [this](const QJsonObject& body, RigInterface*& rigOut,
                             bool& isRightOut, HttpResponse& errOut) -> bool {
        const QString radio = body.value(QStringLiteral("radio")).toString("L").toUpper();
        isRightOut = (radio == QLatin1String("R"));
        if (isRightOut && !m_so2rEnabled) {
            errOut.status = 400;
            errOut.body = QByteArray("{\"error\":\"SO2R not enabled; radio R unavailable\"}");
            return false;
        }
        rigOut = isRightOut ? m_rigClientR : m_rigClient;
        if (!rigOut) {
            errOut.status = 503;
            errOut.body = QByteArray("{\"error\":\"rig not available\"}");
            return false;
        }
        return true;
    };

    // POST /api/rig/qsy — tune rig to {freq_hz, mode}. Either/both optional.
    m_httpServer->registerRoute("POST", "/api/rig/qsy",
        [this, jsonResp, parseJsonBody, resolveRig](const HttpRequest& req) {
            QJsonObject body;
            if (auto err = parseJsonBody(req, body)) return *err;
            RigInterface* rig = nullptr; bool isR = false; HttpResponse err;
            if (!resolveRig(body, rig, isR, err)) return err;

            bool anyChange = false;
            if (body.contains(QStringLiteral("freq_hz"))) {
                const double freqHz = body.value(QStringLiteral("freq_hz")).toDouble(0.0);
                if (freqHz <= 0) {
                    HttpResponse r; r.status = 400;
                    r.body = QByteArray("{\"error\":\"freq_hz must be > 0\"}");
                    return r;
                }
                if (!rig->setFrequency(freqHz)) {
                    HttpResponse r; r.status = 502;
                    r.body = QByteArray("{\"error\":\"rig rejected setFrequency\"}");
                    return r;
                }
                anyChange = true;
            }
            if (body.contains(QStringLiteral("mode"))) {
                const QString mode = body.value(QStringLiteral("mode")).toString();
                if (!mode.isEmpty() && !rig->setMode(mode)) {
                    HttpResponse r; r.status = 502;
                    r.body = QByteArray("{\"error\":\"rig rejected setMode\"}");
                    return r;
                }
                anyChange = true;
            }
            QJsonObject j;
            j["ok"] = true;
            j["changed"] = anyChange;
            return jsonResp(j);
        });

    // POST /api/rig/band — jump rig to the low edge of the named band.
    // Body: {radio: "L|R", band: "20m|40m|..."}. Useful for quick band
    // changes from a phone without having to know the exact frequency.
    auto bandToFreqHz = [](const QString& band) -> double {
        // Low-edge anchors — operator fine-tunes from there. Matches the
        // band-edge constants in BandPlan::freq2Band().
        static const QHash<QString, double> t = {
            {"160m", 1800000}, {"80m", 3500000}, {"60m", 5330500},
            {"40m",  7000000}, {"30m",10100000}, {"20m",14000000},
            {"17m", 18068000}, {"15m",21000000}, {"12m",24890000},
            {"10m", 28000000}, {"6m", 50000000}, {"2m",144000000},
        };
        return t.value(band.toLower(), 0.0);
    };
    m_httpServer->registerRoute("POST", "/api/rig/band",
        [this, jsonResp, parseJsonBody, resolveRig, bandToFreqHz](const HttpRequest& req) {
            QJsonObject body;
            if (auto err = parseJsonBody(req, body)) return *err;
            RigInterface* rig = nullptr; bool isR = false; HttpResponse err;
            if (!resolveRig(body, rig, isR, err)) return err;

            const QString band = body.value(QStringLiteral("band")).toString();
            const double freqHz = bandToFreqHz(band);
            if (freqHz <= 0) {
                HttpResponse r; r.status = 400;
                r.body = QByteArray("{\"error\":\"unknown or missing 'band' (e.g. '20m')\"}");
                return r;
            }
            if (!rig->setFrequency(freqHz)) {
                HttpResponse r; r.status = 502;
                r.body = QByteArray("{\"error\":\"rig rejected setFrequency\"}");
                return r;
            }
            QJsonObject j;
            j["ok"]     = true;
            j["band"]   = band;
            j["freqHz"] = freqHz;
            return jsonResp(j);
        });

    // POST /api/rig/run_mode — set Run/S&P/Off for the specified radio.
    // Body: {radio: "L|R", mode: "Run|S&P|Off"}.
    // Note: skips the modal "missing memory roles" validation that the UI
    // button handlers run — a phone request shouldn't pop a dialog on the
    // shack PC. Operator's responsibility to have memories set up; the
    // mode change takes effect either way, and F-key sends just fail silently
    // if roles aren't assigned (consistent with other headless invocations).
    m_httpServer->registerRoute("POST", "/api/rig/run_mode",
        [this, jsonResp, parseJsonBody](const HttpRequest& req) {
            QJsonObject body;
            if (auto err = parseJsonBody(req, body)) return *err;
            const QString radio = body.value(QStringLiteral("radio")).toString("L").toUpper();
            const bool isR = (radio == QLatin1String("R"));
            if (isR && !m_so2rEnabled) {
                HttpResponse r; r.status = 400;
                r.body = QByteArray("{\"error\":\"SO2R not enabled; radio R unavailable\"}");
                return r;
            }
            const QString modeStr = body.value(QStringLiteral("mode")).toString().trimmed();
            RunMode target;
            if (modeStr.compare("Run",  Qt::CaseInsensitive) == 0) target = RunMode::Run;
            else if (modeStr == QLatin1String("S&P")
                  || modeStr.compare("SP", Qt::CaseInsensitive) == 0) target = RunMode::SP;
            else if (modeStr.compare("Off", Qt::CaseInsensitive) == 0) target = RunMode::Off;
            else {
                HttpResponse r; r.status = 400;
                r.body = QByteArray("{\"error\":\"mode must be 'Run', 'S&P', or 'Off'\"}");
                return r;
            }
            if (isR) m_runModeR = target; else m_runMode = target;
            updateRunSPButtons();
            QJsonObject j;
            j["ok"]   = true;
            j["radio"]= isR ? "R" : "L";
            j["mode"] = (target == RunMode::Run) ? "Run"
                      : (target == RunMode::SP)  ? "S&P" : "Off";
            return jsonResp(j);
        });
}

// ----- Snapshot update helpers — called from existing update sites -----

void MainWindow::updateSnapshotScore()
{
    if (!m_clxSnapshot || !m_contestEngine) return;
    const ContestEngine::ContestScore src = m_contestEngine->getRunningScore();

    clx::net::ScoreSnapshot s;
    s.totalQsos    = m_qsoModel ? m_qsoModel->rowCount() : 0;
    s.totalPoints  = src.contactScore;
    s.namedMults   = src.namedMultCount;
    s.dxccMults    = src.dxccMultCount;
    s.ituMults     = src.ituRegionMultCount;
    s.gridMults    = src.gridSquareMultCount;
    s.prefixMults  = src.namedCallPrefixCount;
    s.finalScore   = src.contestScore;

    for (auto it = src.bandStats.begin(); it != src.bandStats.end(); ++it) {
        QHash<QString, int> modeBreakdown;
        if (it.value().cwQsos)      modeBreakdown["CW"] = it.value().cwQsos;
        if (it.value().ssbQsos)     modeBreakdown["PH"] = it.value().ssbQsos;
        if (it.value().digitalQsos) modeBreakdown["DIG"] = it.value().digitalQsos;
        s.qsosByBandMode.insert(it.key(), modeBreakdown);
    }
    m_clxSnapshot->setScore(s);
    updateSnapshotRate();
    updateSnapshotMults();
}

void MainWindow::updateSnapshotRate()
{
    if (!m_clxSnapshot || !m_qsoModel) return;
    // Compute rates directly from the QSO model's timestamps. Three
    // windows: last 10 min (×6 for hourly extrapolation), last 60 min
    // (actual), and session average (total QSOs / operating hours).
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 tenMinMs = 10LL  * 60 * 1000;
    const qint64 oneHourMs = 60LL * 60 * 1000;
    int last10 = 0, last60 = 0, total = 0;
    qint64 firstQsoMs = 0;
    for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
        const QsoRecord& q = m_qsoModel->getQso(i);
        const QDateTime ts = q.getDateTime();
        if (!ts.isValid()) continue;
        const qint64 ms = ts.toUTC().toMSecsSinceEpoch();
        if (firstQsoMs == 0 || ms < firstQsoMs) firstQsoMs = ms;
        ++total;
        if (nowMs - ms <= tenMinMs)  ++last10;
        if (nowMs - ms <= oneHourMs) ++last60;
    }
    clx::net::RateSnapshot r;
    r.currentHourlyRate  = last10 * 6;
    r.lastHourRate       = last60;
    if (total > 0 && firstQsoMs > 0) {
        const double hours = (nowMs - firstQsoMs) / (1000.0 * 3600.0);
        r.sessionAverageRate = hours > 0.05 ? static_cast<int>(total / hours) : 0;
    }
    m_clxSnapshot->setRate(r);
}

void MainWindow::updateSnapshotRig(bool rightRadio)
{
    if (!m_clxSnapshot) return;
    RigInterface* rig = rightRadio ? m_rigClientR : m_rigClient;
    if (!rig) return;
    clx::net::RigSnapshot r;
    r.backend    = Settings::instance().getRigBackend();
    r.connected  = rig->isConnected();
    const double freqHz = rig->getFrequency();
    r.freqHz     = static_cast<qint64>(freqHz);
    r.mode       = rig->getMode();
    r.band       = BandPlan::freq2Band(freqHz / 1000.0);   // freq2Band takes kHz
    r.pttActive  = false;   // tracked via pttStateChanged signal; wire up later if needed
    const RunMode rm = rightRadio ? m_runModeR : m_runMode;
    r.runSpMode = (rm == RunMode::Run) ? "Run"
                : (rm == RunMode::SP)  ? "S&P"
                                       : "Off";
    m_clxSnapshot->setRig(rightRadio, r);
}

void MainWindow::updateSnapshotQsos()
{
    if (!m_clxSnapshot || !m_qsoModel) return;
    QVector<clx::net::QsoSnapshot> all;
    all.reserve(m_qsoModel->rowCount());
    for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
        const QsoRecord& q = m_qsoModel->getQso(i);
        const QDateTime ts = q.getDateTime();
        clx::net::QsoSnapshot s;
        s.dateUtc = ts.isValid() ? ts.toUTC().toString("yyyy-MM-dd") : QString();
        s.timeUtc = ts.isValid() ? ts.toUTC().toString("HHmmss") : QString();
        s.call    = q.getCall();
        // QsoRecord stores the frequency as a string in kHz; the snapshot's
        // freqHz wants Hz. Empty / invalid → 0.
        s.freqHz  = static_cast<qint64>(q.getFrequency().toDouble() * 1000.0);
        s.mode    = q.getMode();
        s.rstSent = q.getRstSent();
        s.rstRcvd = q.getRstReceived();
        s.exchSent = q.getExchangeSent();
        s.exchRcvd = q.getExchangeReceived();
        s.points  = q.getPoints();
        all.append(s);
    }
    m_clxSnapshot->setAllQsos(all);
}

void MainWindow::updateSnapshotMults()
{
    if (!m_clxSnapshot || !m_contestEngine) return;
    QStringList worked;
    for (const QString& m : m_contestEngine->getWorkedNamedMults()) worked.append(m);
    m_clxSnapshot->setWorkedNamedMults(worked);
}

