#include "mainwindow.h"
#include "rigcontroldialog.h"
#include "freqmodedialog.h"
#include "cwwindow.h"
#include "cwmemoriesdialog.h"
#include "dxclusterpanel.h"
#include "scorewidget.h"
#include "scpwidget.h"
#include "scplineedit.h"
#include "stationsetupdialog.h"
#include "stationclassdialog.h"
#include "contestselectdialog.h"
#include "shortcutsdialog.h"
#include "cabrillodialog.h"
#include "callhistorydialog.h"
#include "scpdialog.h"
#include "cabrilloexport.h"
#include "contestengine.h"
#include "filehandler.h"
#include "clxfile.h"
#include "loadingworker.h"
#include "scoringworker.h"
#include "settings.h"
#include "callhistory.h"
#include "supercheckpartial.h"
#include "../utils/bandplan.h"
#include "debuglogger.h"
#include "DxccDatabase.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QProgressDialog>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QRegularExpressionValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QHeaderView>
#include <QSplitter>
#include <QThread>
#include <QDockWidget>
#include <QCloseEvent>
#include <QtMath>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_callEdit(nullptr)
    , m_exchangeEdit(nullptr)
    , m_logButton(nullptr)
    , m_qsoTable(nullptr)
    , m_statusLabel(nullptr)
    , m_freqModeButton(nullptr)
    , m_contestNameLabel(nullptr)
    , m_qsoCountLabel(nullptr)
    , m_rigStatusLabel(nullptr)
    , m_wpmLabel(nullptr)
    , m_propagationLabel(nullptr)
    , m_dxClusterPanel(nullptr)
    , m_cwConsole(nullptr)
    , m_qsoModel(new QsoListModel(this))
    , m_currentFile("")
    , m_isModified(false)
    , m_showingLogFileNotFoundDialog(false)
    , m_testMode(false)
    , m_debugLogMode(false)
    , m_contestEngine(new ContestEngine(this))
    , m_dxccDatabase(new DxccDatabase(this))
    , m_flrigClient(new FlrigClient(this))
    , m_rigPollTimer(new QTimer(this))
    , m_lastFrequency(14250.0)
    , m_lastMode("USB")
    , m_lastWpm(28)
    , m_contextMenuRow(-1)
{
    // Validate startup requirements
    // Check from current working directory first
    QString ctyPath = m_dxccDatabase->getDataPath() + "/cty.dat";
    bool hasCtyDat = QFile::exists(ctyPath);
    
    // Check for contests directory with at least one JSON file in current directory
    QDir contestsDir("contests");
    bool hasContestsDir = contestsDir.exists();
    QStringList contestFiles = contestsDir.entryList(QStringList() << "*.json", QDir::Files);
    bool hasContestFiles = !contestFiles.isEmpty();
    
    // If not found in current directory, check relative to executable
    if (!hasContestFiles) {
        QString appDir = QCoreApplication::applicationDirPath();
        QDir appContestsDir(appDir + "/../contests");
        if (appContestsDir.exists()) {
            QStringList appContestFiles = appContestsDir.entryList(QStringList() << "*.json", QDir::Files);
            if (!appContestFiles.isEmpty()) {
                // Change to parent directory of executable (application root)
                QDir::setCurrent(appDir + "/..");
                DebugLogger::instance().log("MainWindow", QString("Changed working directory to: %1").arg(QDir::currentPath()));
                
                // Re-check after directory change
                ctyPath = m_dxccDatabase->getDataPath() + "/cty.dat";
                hasCtyDat = QFile::exists(ctyPath);
                hasContestFiles = true;
            }
        }
    }
    
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
    
    m_qsoCountLabel = new QLabel("QSOs: 0");
    statusBar()->addPermanentWidget(m_qsoCountLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_rigStatusLabel = new QLabel("Rig: Disconnected");
    statusBar()->addPermanentWidget(m_rigStatusLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_wpmLabel = new QLabel("WPM: --");
    statusBar()->addPermanentWidget(m_wpmLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_propagationLabel = new QLabel("");
    statusBar()->addPermanentWidget(m_propagationLabel);
    
    // Setup rig polling timer (500ms interval by default)
    Settings& settings = Settings::instance();
    int pollInterval = settings.getFlrigPollInterval();
    m_rigPollTimer->setInterval(pollInterval);
    connect(m_rigPollTimer, &QTimer::timeout, this, &MainWindow::onUpdateRigDisplay);
    
    // Load CW memories
    loadCWMemories();
    
    // Auto-reconnect to flrig if previously connected
    bool autoConnect = settings.getFlrigAutoConnect();
    DebugLogger::instance().log("MainWindow", QString("AutoConnect setting: %1").arg(autoConnect ? "true" : "false"));
    if (autoConnect) {
        QString host = settings.getFlrigHost();
        int port = settings.getFlrigPort();
        DebugLogger::instance().log("MainWindow", QString("Will auto-connect to flrig at %1:%2 in 500ms").arg(host).arg(port));
        QTimer::singleShot(500, this, [this, host, port]() {
            DebugLogger::instance().log("MainWindow", QString("Auto-connect timer fired, connecting to %1:%2").arg(host).arg(port));
            m_flrigClient->connectToRig(host, port);
        });
    } else {
        DebugLogger::instance().log("MainWindow", "Auto-connect disabled");
    }
    
    // Check if station setup is configured
    QString callsign = settings.getCallsign();
    if (callsign.isEmpty()) {
        DebugLogger::instance().log("MainWindow", "Station not configured, showing station setup dialog");
        QTimer::singleShot(100, this, &MainWindow::onStationSetup);
    } else {
        // Check for --log command-line argument
        QStringList args = QApplication::arguments();
        int logIndex = args.indexOf("--log");
        int testIndex = args.indexOf("--test-only");
        
        if (testIndex != -1) {
            m_testMode = true;
            DebugLogger::instance().log("MainWindow", "Test mode enabled");
        }
        
        if (logIndex != -1 && logIndex + 1 < args.count()) {
            QString logFilePath = args[logIndex + 1];
            m_debugLogMode = true;
            DebugLogger::instance().log("MainWindow", QString("Loading log file from command line: %1").arg(logFilePath));
            DebugLogger::instance().log("MainWindow", "Debug log mode enabled - summary sheet will be written to debug log");
            QTimer::singleShot(100, this, [this, logFilePath]() {
                loadLogFile(logFilePath);
            });
        } else {
            DebugLogger::instance().log("MainWindow", "Station configured, showing contest selection dialog");
            QTimer::singleShot(100, this, &MainWindow::onNewLog);
        }
    }
    
    // Restore window geometry or use defaults
    restoreWindowGeometry();
    restorePanelState();
    
    // Load saved CW WPM
    int savedWpm = settings.getCwWpm();
    m_lastWpm = savedWpm;
    m_wpmLabel->setText(QString("WPM: %1").arg(savedWpm));
}

MainWindow::~MainWindow()
{
    // Cleanup happens in closeEvent
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
        }
    }

    // Save window geometry
    saveWindowGeometry();
    
    // Save dock widget state (positions, sizes, floating state)
    savePanelState();
    
    // Accept the close event
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(obj);
        
        // Handle Space and/or Tab keys to advance to next text input field (wraps around)
        // Which keys are used depends on contest definition
        bool handleSpace = (m_fieldNavigationKeys == "space" || m_fieldNavigationKeys == "both");
        bool handleTab = (m_fieldNavigationKeys == "tab" || m_fieldNavigationKeys == "both");
        
        if (((keyEvent->key() == Qt::Key_Space && handleSpace) || 
             (keyEvent->key() == Qt::Key_Tab && handleTab)) && lineEdit) {
            // Check if this field is in our entry field list
            int currentIndex = m_entryFieldOrder.indexOf(lineEdit);
            if (currentIndex >= 0) {
                // Move to next field, wrapping to first field if at the end
                int nextIndex = (currentIndex + 1) % m_entryFieldOrder.size();
                m_entryFieldOrder[nextIndex]->setFocus();
                return true; // Event handled - don't insert space/tab
            }
        }
        
        // Handle Shift+Tab to go to previous field (only if tab navigation is enabled)
        if (keyEvent->key() == Qt::Key_Backtab && handleTab && lineEdit) {
            // Check if this field is in our entry field list
            int currentIndex = m_entryFieldOrder.indexOf(lineEdit);
            if (currentIndex >= 0) {
                // Move to previous field, wrapping to last field if at the beginning
                int prevIndex = (currentIndex - 1 + m_entryFieldOrder.size()) % m_entryFieldOrder.size();
                m_entryFieldOrder[prevIndex]->setFocus();
                return true; // Event handled
            }
        }
        
        // Check if Enter or Return was pressed in any QSO entry field
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (lineEdit && m_exchangeFields.values().contains(lineEdit)) {
                // Trigger Log QSO
                onLogQso();
                return true; // Event handled
            }
        }
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
    m_qsoTable->verticalHeader()->setVisible(false);
    m_qsoTable->setMinimumHeight(400);
    
    // Add double-click handling for QSO editing
    connect(m_qsoTable, &QTableView::doubleClicked, this, &MainWindow::onQsoDoubleClicked);
    
    m_qsoTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_qsoTable, &QTableView::customContextMenuRequested, this, &MainWindow::onQsoContextMenuRequested);
    connect(m_qsoTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, &MainWindow::onColumnResized);
    restoreColumnWidths();
    
    leftLayout->addWidget(m_qsoTable, 1);
    
    // Entry form at BOTTOM
    QWidget *entryPanel = new QWidget(this);
    QHBoxLayout *entryPanelLayout = new QHBoxLayout(entryPanel);
    entryPanelLayout->setContentsMargins(2, 2, 2, 2);
    entryPanelLayout->setSpacing(5);
    
    m_freqModeButton = new QPushButton("14250.0 USB", this);
    m_freqModeButton->setFlat(false);
    m_freqModeButton->setMinimumWidth(120);
    m_freqModeButton->setMinimumHeight(50);
    m_freqModeButton->setStyleSheet("QPushButton { text-align: center; padding: 8px; font-weight: bold; font-size: 11pt; }");
    connect(m_freqModeButton, &QPushButton::clicked, this, &MainWindow::onFreqModeButtonClicked);
    entryPanelLayout->addWidget(m_freqModeButton);
    
    m_qsoEntryGroup = new QGroupBox("QSO Entry", this);
    m_qsoEntryGroup->setObjectName("qsoEntryGroup");
    m_qsoEntryLayout = new QHBoxLayout(m_qsoEntryGroup);
    m_qsoEntryLayout->setSpacing(5);
    
    m_qsoEntryLayout->addWidget(new QLabel("Call:"));
    ScpLineEdit *callEdit = new ScpLineEdit();
    m_callEdit = callEdit;  // Store as base QLineEdit pointer for compatibility
    callEdit->setMaxLength(14);  // Standard callsign length
    callEdit->setMaximumWidth(120); // Max width for 10 chars
    // SCP widget will be wired in createConnections() after m_scpWidget is created
    
    // Force uppercase input
    connect(callEdit, &QLineEdit::textChanged, [this](const QString& text) {
        if (text != text.toUpper()) {
            int cursorPos = m_callEdit->cursorPosition();
            m_callEdit->setText(text.toUpper());
            m_callEdit->setCursorPosition(cursorPos);
        }
    });
    m_qsoEntryLayout->addWidget(callEdit);
    
    m_qsoEntryLayout->addWidget(new QLabel("Exchange:"));
    m_exchangeEdit = new QLineEdit();
    m_exchangeEdit->setMinimumWidth(100);
    // Force uppercase input
    connect(m_exchangeEdit, &QLineEdit::textChanged, [this](const QString& text) {
        if (text != text.toUpper()) {
            int cursorPos = m_exchangeEdit->cursorPosition();
            m_exchangeEdit->setText(text.toUpper());
            m_exchangeEdit->setCursorPosition(cursorPos);
        }
    });
    m_qsoEntryLayout->addWidget(m_exchangeEdit);
    
    m_logButton = new QPushButton("Log QSO");
    m_qsoEntryLayout->addWidget(m_logButton);
    m_qsoEntryLayout->addStretch();
    
    // Set proper tab order
    setTabOrder(m_callEdit, m_exchangeEdit);
    setTabOrder(m_exchangeEdit, m_logButton);
    
    // Install event filters for Enter key handling
    m_callEdit->installEventFilter(this);
    m_exchangeEdit->installEventFilter(this);
    
    entryPanelLayout->addWidget(m_qsoEntryGroup, 1);
    leftLayout->addWidget(entryPanel, 0);
    
    mainSplitter->addWidget(leftPanel);
    
    // Convert all right side panels to QDockWidgets for flexibility
    
    // DX Cluster Panel as QDockWidget
    m_dxClusterDock = new QDockWidget("DX Cluster", this);
    m_dxClusterDock->setObjectName("dxClusterDock");  // Required for saveState/restoreState
    m_dxClusterPanel = new DxClusterPanel(m_dxClusterDock);
    m_dxClusterPanel->setMinimumHeight(200);
    m_dxClusterDock->setWidget(m_dxClusterPanel);
    m_dxClusterDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dxClusterDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_dxClusterDock);
    
    // Connect propagation data signal
    connect(m_dxClusterPanel, &DxClusterPanel::propagationDataReceived, 
            this, &MainWindow::onPropagationDataReceived);
    
    // Connect spot clicked signal to change rig frequency/mode
    connect(m_dxClusterPanel, &DxClusterPanel::spotClicked,
            this, &MainWindow::onDxSpotClicked);
    
    // CW Console as QDockWidget
    m_cwConsoleDock = new QDockWidget("CW Console", this);
    m_cwConsoleDock->setObjectName("cwConsoleDock");  // Required for saveState/restoreState
    m_cwConsole = new CWWindow(m_flrigClient, m_cwConsoleDock);
    m_cwConsole->setMinimumHeight(250);
    m_cwConsoleDock->setWidget(m_cwConsole);
    m_cwConsoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_cwConsoleDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_cwConsoleDock);
    
    // Score Widget as QDockWidget
    m_scoreDock = new QDockWidget("Score", this);
    m_scoreDock->setObjectName("scoreDock");  // Required for saveState/restoreState
    m_scoreWidget = new ScoreWidget(m_scoreDock);
    m_scoreWidget->setMinimumHeight(300);
    m_scoreDock->setWidget(m_scoreWidget);
    m_scoreDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_scoreDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_scoreDock);
    
    // SCP Widget as QDockWidget
    m_scpWidget = new ScpWidget(this);
    m_scpWidget->setMinimumHeight(100);
    m_scpWidget->setMaximumHeight(200);
    m_scpWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_scpWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_scpWidget);
    
    // Position SCP widget below DX Cluster - use splitDockWidget to control placement
    // This ensures SCP stays under DX Cluster, and CW Console/Score widgets are below it
    splitDockWidget(m_dxClusterDock, m_scpWidget, Qt::Vertical);
    
    m_scpWidget->hide();  // Hidden by default, user can show via Window menu
    
    // Store as m_scpDock for consistency with other docks
    m_scpDock = m_scpWidget;
    
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
    connect(m_dxClusterDock, &QDockWidget::dockLocationChanged, this, [this]() {
        savePanelState();
    });
    connect(m_cwConsoleDock, &QDockWidget::dockLocationChanged, this, [this]() {
        savePanelState();
    });
    connect(m_scoreDock, &QDockWidget::dockLocationChanged, this, [this]() {
        savePanelState();
    });
    if (m_scpWidget) {
        connect(m_scpWidget, &QDockWidget::dockLocationChanged, this, [this]() {
            savePanelState();
        });
    }
    
    mainLayout->addWidget(mainSplitter);
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
    
    fileMenu->addSeparator();
    
    QAction *stationSetupAction = fileMenu->addAction("&Station Setup...");
    connect(stationSetupAction, &QAction::triggered, this, &MainWindow::onStationSetup);
    
    fileMenu->addSeparator();
    
    QAction *downloadCtyAction = fileMenu->addAction("&Download DXCC Database (cty.dat)...");
    connect(downloadCtyAction, &QAction::triggered, this, &MainWindow::onDownloadCtyDat);
    
    QAction *downloadScpAction = fileMenu->addAction("Download Super Check Partial (master.scp)...");
    connect(downloadScpAction, &QAction::triggered, this, &MainWindow::onDownloadScp);
    
    fileMenu->addSeparator();
    
    QAction *callHistoryAction = fileMenu->addAction("Manage &Call History...");
    connect(callHistoryAction, &QAction::triggered, this, &MainWindow::onManageCallHistory);
    
    fileMenu->addSeparator();
    
    QAction *shortcutsAction = fileMenu->addAction("&Shortcuts...");
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::onShortcuts);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    // Rig menu
    QMenu *rigMenu = menuBar()->addMenu("&Rig");
    
    QAction *rigControlAction = rigMenu->addAction("flrig &Connection...");
    connect(rigControlAction, &QAction::triggered, this, &MainWindow::onRigControl);
    
    rigMenu->addSeparator();
    
    QAction *editCWMemAction = rigMenu->addAction("Edit CW &Memories...");
    connect(editCWMemAction, &QAction::triggered, this, &MainWindow::onEditCWMemories);
    
    // Contest menu
    QMenu *contestMenu = menuBar()->addMenu("&Contest");
    
    QAction *recalcScoreAction = contestMenu->addAction("&Recalculate score");
    connect(recalcScoreAction, &QAction::triggered, this, &MainWindow::onRecalculateScore);
    
    contestMenu->addSeparator();
    
    QAction *scpAction = contestMenu->addAction("&Super Check Partial...");
    connect(scpAction, &QAction::triggered, this, &MainWindow::onScpDialog);
    
    contestMenu->addSeparator();
    
    QAction *cabrilloAction = contestMenu->addAction("&Generate Cabrillo log...");
    connect(cabrilloAction, &QAction::triggered, this, &MainWindow::onExportCabrillo);
    
    QAction *summaryAction = contestMenu->addAction("&Create summary sheet...");
    connect(summaryAction, &QAction::triggered, this, &MainWindow::onCreateSummarySheet);
    
    // Window menu
    QMenu *windowMenu = menuBar()->addMenu("&Window");
    
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
    
    // Debug menu
    QMenu *debugMenu = menuBar()->addMenu("&Debug");
    
    m_flrigDebugAction = debugMenu->addAction("Enable &Flrig Debug Logging");
    m_flrigDebugAction->setCheckable(true);
    bool flrigDebugEnabled = Settings::instance().getFlrigDebugEnabled();
    m_flrigDebugAction->setChecked(flrigDebugEnabled);
    DebugLogger::instance().setFlrigDebugEnabled(flrigDebugEnabled);
    connect(m_flrigDebugAction, &QAction::triggered, this, &MainWindow::onToggleFlrigDebug);
    
    m_mainWindowDebugAction = debugMenu->addAction("Enable &MainWindow Debug Logging");
    m_mainWindowDebugAction->setCheckable(true);
    bool mainWindowDebugEnabled = Settings::instance().getMainWindowDebugEnabled();
    m_mainWindowDebugAction->setChecked(mainWindowDebugEnabled);
    DebugLogger::instance().setMainWindowDebugEnabled(mainWindowDebugEnabled);
    connect(m_mainWindowDebugAction, &QAction::triggered, this, &MainWindow::onToggleMainWindowDebug);
    
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
    
    m_cwWindowDebugAction = debugMenu->addAction("Enable C&WWindow Debug Logging");
    m_cwWindowDebugAction->setCheckable(true);
    bool cwWindowDebugEnabled = Settings::instance().getCWWindowDebugEnabled();
    m_cwWindowDebugAction->setChecked(cwWindowDebugEnabled);
    DebugLogger::instance().setCWWindowDebugEnabled(cwWindowDebugEnabled);
    connect(m_cwWindowDebugAction, &QAction::triggered, this, &MainWindow::onToggleCWWindowDebug);
    
    m_dxccDatabaseDebugAction = debugMenu->addAction("Enable &DxccDatabase Debug Logging");
    m_dxccDatabaseDebugAction->setCheckable(true);
    bool dxccDatabaseDebugEnabled = Settings::instance().getDxccDatabaseDebugEnabled();
    m_dxccDatabaseDebugAction->setChecked(dxccDatabaseDebugEnabled);
    DebugLogger::instance().setDxccDatabaseDebugEnabled(dxccDatabaseDebugEnabled);
    connect(m_dxccDatabaseDebugAction, &QAction::triggered, this, &MainWindow::onToggleDxccDatabaseDebug);
    
    m_scpDebugAction = debugMenu->addAction("Enable &Super Check Partial Debug Logging");
    m_scpDebugAction->setCheckable(true);
    bool scpDebugEnabled = Settings::instance().getScpDebugEnabled();
    m_scpDebugAction->setChecked(scpDebugEnabled);
    DebugLogger::instance().setScpDebugEnabled(scpDebugEnabled);
    connect(m_scpDebugAction, &QAction::triggered, this, &MainWindow::onToggleScpDebug);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createConnections()
{
    connect(m_logButton, &QPushButton::clicked, this, &MainWindow::onLogQso);
    connect(m_callEdit, &QLineEdit::textChanged, this, &MainWindow::onCallChanged);
    connect(m_exchangeEdit, &QLineEdit::textChanged, this, &MainWindow::onExchangeChanged);
    
    connect(m_qsoModel, &QsoListModel::qsoAdded, this, [this]() {
        m_qsoCountLabel->setText(QString("QSOs: %1").arg(m_qsoModel->count()));
        m_isModified = true;
        updateWindowTitle();
    });
    
    // Rig connections
    connect(m_flrigClient, &FlrigClient::connected, this, &MainWindow::onRigConnected);
    connect(m_flrigClient, &FlrigClient::disconnected, this, &MainWindow::onRigDisconnected);
    
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
            Q_UNUSED(visible);
            updateScpWidgetMenuText();
        });
    }
}

void MainWindow::onNewLog()
{
    if (!maybeSave())
        return;
    
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
                    
                    // Load the contest definition if specified
                    if (!contestFile.isEmpty()) {
                        QString contestPath = QCoreApplication::applicationDirPath() + "/contests/" + contestFile;
                        if (!QFile::exists(contestPath)) {
                            contestPath = "contests/" + contestFile;
                        }
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
                    
                    // Clear the model first
                    m_qsoModel->clear();
                    
                    // Now add all QSOs to the model once
                    progressDialog->setLabelText("Loading QSOs...");
                    progressDialog->setRange(0, loadedQsos.size());
                    QApplication::processEvents();
                    
                    for (int i = 0; i < loadedQsos.size(); ++i) {
                        m_qsoModel->addQso(loadedQsos[i]);
                        
                        // Update progress every 50 QSOs to avoid excessive redraws
                        if (i % 50 == 0) {
                            progressDialog->setValue(i);
                            QApplication::processEvents();
                        }
                    }
                    progressDialog->setValue(loadedQsos.size());
                    
                    m_currentFile = selectedFile;
                    m_isModified = false;
                    updateWindowTitle();
                    updateQsoEntryFields();
                    
                    progressDialog->close();
                    progressDialog->deleteLater();
                    
                    // Auto-recalculate score to validate and mark dupes/out-of-band
                    onRecalculateScore();
                    
                    m_statusLabel->setText(QString("Loaded %1 QSOs").arg(loadedQsos.size()));
                });
                
                loadThread->start();
            } else {
                // User selected a contest definition - create new log
                // Clear the QSO model first so old data isn't visible
                m_qsoModel->clear();
                m_currentFile.clear();
                
                // Pass false to NOT restore the previous station class
                loadContestDefinition(selectedFile, false);
                
                // Prompt for station class if the contest requires it
                if (m_contestEngine && m_contestEngine->needsStationClass()) {
                    StationClassDialog classDialog(
                        m_contestEngine->getStationClassPrompt(),
                        m_contestEngine->getStationClassOptions(),
                        this);
                    if (classDialog.exec() == QDialog::Accepted) {
                        QString selectedClass = classDialog.getSelectedClass();
                        m_contestEngine->setStationClass(selectedClass);
                        DebugLogger::instance().log("MainWindow", 
                            QString("Station class selected: %1").arg(selectedClass));
                        
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
                
                m_isModified = false;
                updateWindowTitle();
                clearEntryForm();
                m_statusLabel->setText("New log created");
            }
        }
    }
}

void MainWindow::onOpenLog()
{
    if (!maybeSave())
        return;
    
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
            FileHandler fileHandler;
            
            QList<QsoRecord> temp;
            fileHandler.loadClxWithContest(fileName, temp, contestFile, stationClass, loadedContestVersion, stationClassExchangeName, stationClassExchangeId);
            
            // Also load the station info from the CLX file
            ClxFile clxFile;
            QString loadedMode;
            if (clxFile.load(fileName)) {
                QString loadedCallsign = clxFile.station().callsign();
                if (!loadedCallsign.isEmpty()) {
                    Settings::instance().setCallsign(loadedCallsign);
                    DebugLogger::instance().log("MainWindow", QString("Loaded callsign from CLX: %1").arg(loadedCallsign));
                }
                // Also load operator name and state if available
                QString operatorName = clxFile.station().operatorName();
                QString operatorState = clxFile.station().state();
                if (!operatorName.isEmpty()) {
                    Settings::instance().setOperatorName(operatorName);
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator name from CLX: %1").arg(operatorName));
                }
                if (!operatorState.isEmpty()) {
                    Settings::instance().setState(operatorState);
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator state from CLX: %1").arg(operatorState));
                }
                // Also load the contest mode
                loadedMode = clxFile.contest().mode();
                if (!loadedMode.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded contest mode from CLX: %1").arg(loadedMode));
                }
            }
            
            // Load the contest definition if specified
            if (!contestFile.isEmpty()) {
                QString contestPath = QCoreApplication::applicationDirPath() + "/contests/" + contestFile;
                if (!QFile::exists(contestPath)) {
                    contestPath = "contests/" + contestFile;
                }
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
        
        // Clear the model first
        m_qsoModel->clear();
        
        // Now add all QSOs to the model once
        progressDialog->setLabelText("Loading QSOs...");
        progressDialog->setRange(0, loadedQsos.size());
        QApplication::processEvents();
        
        for (int i = 0; i < loadedQsos.size(); ++i) {
            m_qsoModel->addQso(loadedQsos[i]);
            
            // Update progress every 50 QSOs to avoid excessive redraws
            if (i % 50 == 0) {
                progressDialog->setValue(i);
                QApplication::processEvents();
            }
        }
        progressDialog->setValue(loadedQsos.size());
        
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
            QString myCallsign = Settings::instance().getCallsign();
            
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
                
                m_statusLabel->setText("File loaded: " + fileName + " (" + 
                    QString::number(m_qsoModel->rowCount()) + " QSOs)");
            });
            
            scoringThread->start();
        } else {
            DebugLogger::instance().log("MainWindow", "Contest is still empty, not recalculating score");
            m_statusLabel->setText("File loaded: " + fileName + " (" + 
                QString::number(loadedQsos.count()) + " QSOs)");
        }
    });
    
    loadThread->start();
}

void MainWindow::loadLogFile(const QString& filename)
{
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
            FileHandler fileHandler;
            
            QList<QsoRecord> temp;
            fileHandler.loadClxWithContest(filename, temp, contestFile, stationClass, loadedContestVersion, stationClassExchangeName, stationClassExchangeId);
            
            // Also load the station info from the CLX file
            ClxFile clxFile;
            QString loadedMode;
            if (clxFile.load(filename)) {
                QString loadedCallsign = clxFile.station().callsign();
                if (!loadedCallsign.isEmpty()) {
                    Settings::instance().setCallsign(loadedCallsign);
                    DebugLogger::instance().log("MainWindow", QString("Loaded callsign from CLX: %1").arg(loadedCallsign));
                }
                // Also load operator name and state if available
                QString operatorName = clxFile.station().operatorName();
                QString operatorState = clxFile.station().state();
                if (!operatorName.isEmpty()) {
                    Settings::instance().setOperatorName(operatorName);
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator name from CLX: %1").arg(operatorName));
                }
                if (!operatorState.isEmpty()) {
                    Settings::instance().setState(operatorState);
                    DebugLogger::instance().log("MainWindow", QString("Loaded operator state from CLX: %1").arg(operatorState));
                }
                // Also load the contest mode
                loadedMode = clxFile.contest().mode();
                if (!loadedMode.isEmpty()) {
                    DebugLogger::instance().log("MainWindow", QString("Loaded contest mode from CLX: %1").arg(loadedMode));
                }
            }
            
            DebugLogger::instance().log("MainWindow", 
                QString("Loaded from CLX: contestFile='%1' stationClass='%2' exchangeName='%3' exchangeId='%4'").arg(contestFile, stationClass, stationClassExchangeName, stationClassExchangeId));
            
            // Load the contest definition if specified
            if (!contestFile.isEmpty()) {
                QString contestPath = QCoreApplication::applicationDirPath() + "/contests/" + contestFile;
                if (!QFile::exists(contestPath)) {
                    contestPath = "contests/" + contestFile;
                }
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
        
        // Clear the model first
        m_qsoModel->clear();
        
        // Now add all QSOs to the model once
        progressDialog->setLabelText("Loading QSOs...");
        progressDialog->setRange(0, loadedQsos.size());
        QApplication::processEvents();
        
        for (int i = 0; i < loadedQsos.size(); ++i) {
            m_qsoModel->addQso(loadedQsos[i]);
            
            // Update progress every 50 QSOs to avoid excessive redraws
            if (i % 50 == 0) {
                progressDialog->setValue(i);
                QApplication::processEvents();
            }
        }
        progressDialog->setValue(loadedQsos.size());
        
        m_currentFile = filename;
        m_isModified = false;
        updateWindowTitle();
        
        progressDialog->close();
        progressDialog->deleteLater();
        
        // Auto-recalculate score on background thread to avoid blocking UI
        QString myCallsign = Settings::instance().getCallsign();
        
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
                
                // Reset modified flag since we just loaded the file
                m_isModified = false;
                
                // Update score display
                if (m_scoreWidget) {
                    m_scoreWidget->resetScore();
                    auto score = m_contestEngine->getRunningScore();
                    m_scoreWidget->updateScore(score);
                }
                
                m_statusLabel->setText("File loaded: " + filename + " (" + 
                    QString::number(scoredQsos.count()) + " QSOs)");
                
                // If in debug log mode, generate summary sheet to debug log
                if (m_debugLogMode) {
                    generateSummaryToDebugLog();
                }
                
                // If in test mode, log the score and exit
                if (m_testMode) {
                    auto score = m_contestEngine->getRunningScore();
                    DebugLogger::instance().log("MainWindow", 
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
        success = fileHandler.saveClxWithContest(m_currentFile, m_qsoModel->getQsos(), m_contestFile, m_contestDefinition, stationClass, stationClassExchangeName, stationClassExchangeId);
    } else {
        success = fileHandler.save(m_currentFile, m_qsoModel->getQsos());
    }
    
    if (success) {
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
        "ContestLogX 2.0 Format (*.clx);;"
        "ADIF Files (*.adi *.adif);;"
        "CSV Files (*.csv);;"
        "All Files (*)");
    
    if (fileName.isEmpty())
        return;
    
    // Ensure .clx extension (the default format we save in)
    if (!fileName.endsWith(".clx", Qt::CaseInsensitive) &&
        !fileName.endsWith(".csv", Qt::CaseInsensitive) &&
        !fileName.endsWith(".adi", Qt::CaseInsensitive) &&
        !fileName.endsWith(".adif", Qt::CaseInsensitive)) {
        // No recognized extension, add .clx
        fileName += ".clx";
    }
    
    m_currentFile = fileName;
    onSaveLog();
}

void MainWindow::onStationSetup()
{
    Settings& settings = Settings::instance();
    
    // Load current station info
    StationInfo info;
    info.setCallsign(settings.getCallsign());
    info.setOperatorName(settings.getOperatorName());
    info.setGrid(settings.getGridSquare());
    info.setState(settings.getState());
    
    DebugLogger::instance().log("MainWindow", 
        QString("Station Setup - Before: Call=%1 Name=%2 Grid=%3 State=%4")
        .arg(info.callsign()).arg(info.operatorName()).arg(info.grid()).arg(info.state()));
    
    StationSetupDialog dialog(info, this);
    if (dialog.exec() == QDialog::Accepted) {
        StationInfo newInfo = dialog.stationInfo();
        
        DebugLogger::instance().log("MainWindow", 
            QString("Station Setup - After: Call=%1 Name=%2 Grid=%3 State=%4")
            .arg(newInfo.callsign()).arg(newInfo.operatorName()).arg(newInfo.grid()).arg(newInfo.state()));
        
        settings.setCallsign(newInfo.callsign());
        settings.setOperatorName(newInfo.operatorName());
        settings.setGridSquare(newInfo.grid());
        settings.setState(newInfo.state());
        
        DebugLogger::instance().log("MainWindow", "Calling settings.save()");
        settings.save();
        DebugLogger::instance().log("MainWindow", "settings.save() completed");
    } else {
        DebugLogger::instance().log("MainWindow", "Station Setup - Dialog was cancelled");
    }
}

void MainWindow::onExit()
{
    if (maybeSave()) {
        qApp->quit();
    }
}

void MainWindow::onShortcuts()
{
    ShortcutsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onManageCallHistory()
{
    CallHistoryDialog dialog(this);
    dialog.exec();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
        clearEntryForm();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}


void MainWindow::onCallChanged(const QString& text)
{
    // Force uppercase
    if (text != text.toUpper()) {
        int cursorPos = m_callEdit->cursorPosition();
        m_callEdit->setText(text.toUpper());
        m_callEdit->setCursorPosition(cursorPos);
        return;  // Return early to avoid processing twice
    }
    
    // Look up previous QSO with this callsign and pre-fill exchange
    QString callsign = text.trimmed().toUpper();
    if (callsign.isEmpty()) {
        return;
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
            
            if (found) {
                DebugLogger::instance().log("MainWindow", 
                    QString("Pre-filled exchange from call history for %1").arg(callsign));
                return;
            }
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
    
    // Get callsign from either the dynamic CALL field or the original m_callEdit
    QString callsign;
    if (m_exchangeFields.contains("CALL")) {
        callsign = m_exchangeFields["CALL"]->text().trimmed().toUpper();
    } else {
        callsign = m_callEdit->text().trimmed().toUpper();
    }
    
    if (callsign.isEmpty()) {
        QMessageBox::warning(this, "Invalid QSO", "Callsign cannot be empty");
        return;
    }
    
    qso.setCall(callsign);
    // Use frequency from rig (stored in m_lastFrequency)
    qso.setFrequency(QString::number(m_lastFrequency, 'f', 1));
    
    // Get band from frequency
    QString band = m_contestEngine->getBandFromFrequency(m_lastFrequency);
    if (!band.isEmpty()) {
        // Find band index for setBand()
        const char* bands[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
        for (int i = 0; i < 11; i++) {
            if (band == bands[i]) {
                qso.setBand(i);
                break;
            }
        }
    }
    
    qso.setMode(m_lastMode);
    qso.setDateTime(QDateTime::currentDateTimeUtc());
    qso.setSerial(m_qsoModel->count() + 1);
    
    // Validate that the mode is allowed for this contest
    QStringList allowedModes = m_contestEngine->getAllowedModes();
    if (!allowedModes.isEmpty()) {
        bool modeValid = allowedModes.contains(m_lastMode.toUpper());
        
        // If SSB is allowed, also accept LSB and USB
        if (!modeValid && allowedModes.contains("SSB")) {
            modeValid = (m_lastMode == "LSB" || m_lastMode == "USB");
        }
        
        if (!modeValid) {
            QString errorMsg = QString("Invalid mode '%1'. This contest only allows: %2")
                .arg(m_lastMode)
                .arg(allowedModes.join(", "));
            m_statusLabel->setText(errorMsg);
            DebugLogger::instance().log("MainWindow", errorMsg);
            return;
        }
    }
    
    // If this log was loaded from a file, check if the mode is restricted to the original mode
    QString restrictedMode = m_contestEngine->getRestrictedMode();
    if (!restrictedMode.isEmpty()) {
        bool modeMatches = m_lastMode.toUpper() == restrictedMode.toUpper();
        
        // If the restricted mode is SSB, also allow LSB and USB
        if (!modeMatches && restrictedMode.toUpper() == "SSB") {
            modeMatches = (m_lastMode == "LSB" || m_lastMode == "USB");
        }
        
        if (!modeMatches) {
            QString errorMsg = QString("This log file is restricted to %1 mode only. Cannot log %2 contacts.")
                .arg(restrictedMode)
                .arg(m_lastMode);
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
    
    // Get exchange sent from contest class and station settings
    QString stationQth = Settings::instance().getState();
    int nextSerial = m_qsoModel->count() + 1;
    
    // Try to get split NAME and EXCH from contest engine
    QString sentName = m_contestEngine->getSentExchangeName();
    QString sentExch = m_contestEngine->getSentExchangeId();
    
    // If name is not set from contest engine, try getting it from Settings
    if (sentName.isEmpty()) {
        sentName = Settings::instance().getOperatorName();
    }
    
    // If exchange is not set from contest engine, try getting it from Settings
    if (sentExch.isEmpty()) {
        sentExch = Settings::instance().getState();
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
    
    // Set exchange fields from dynamic inputs (received exchange)
    if (!m_exchangeFields.isEmpty()) {
        DebugLogger::instance().log("MainWindow", 
            QString("Processing %1 exchange fields").arg(m_exchangeFields.size()));
        
        QString receivedName;
        QString receivedExch;
        
        for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
            QString fieldName = it.key();
            QString value = it.value()->text().trimmed().toUpper();
            
            DebugLogger::instance().log("MainWindow", 
                QString("Field: %1 = '%2'").arg(fieldName).arg(value));
            
            if (fieldName == "CALL") {
                // Already handled above
                continue;
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
            QString defaultRst = (m_lastMode == "CW" || m_lastMode == "RTTY") ? "599" : 
                                (m_lastMode.contains("DIGI")) ? "+0" : "59";
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
                 QString myCallsign = Settings::instance().getCallsign();
                 int points = m_contestEngine->calculatePoints(qso, myCallsign);
                 qso.setPoints(points);
                 m_statusLabel->setText("QSO logged");
                 DebugLogger::instance().log("MainWindow", 
                     QString("QSO worth %1 points").arg(points));
             }
        }
    }
    
     // Add the QSO first so it's included in score calculations
    m_qsoModel->addQso(qso);
    
    // Update running score and get total multiplier count
    if (m_contestEngine && m_scoreWidget) {
        QString myCallsign = Settings::instance().getCallsign();
        QList<QsoRecord> allQsos = m_qsoModel->getQsos();
        
        // Get the multiplier credit BEFORE updating running score
        // This uses the previous QSOs to determine if this is a new mult
        QList<QsoRecord> previousQsos = allQsos.mid(0, allQsos.count() - 1);  // All except the one we just added
        ContestEngine::QsoMultiplierCredit credit = m_contestEngine->getQsoMultiplierCredit(qso, previousQsos);
        
        // Update the QSO with mult credit
        int lastQsoIndex = m_qsoModel->count() - 1;
        m_qsoModel->updateMultiplierCount(lastQsoIndex, credit.namedMultCount);
        m_qsoModel->updateDxccCount(lastQsoIndex, credit.dxccMultCount);
        m_qsoModel->updateItuRegionCount(lastQsoIndex, credit.ituRegionMultCount);
        
        // Now update running score with the updated QSO
        allQsos = m_qsoModel->getQsos();
        m_contestEngine->updateRunningScore(allQsos, myCallsign, false);  // Suppress verbose logging
        
        // Get the running score which includes calculated multipliers
        ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
        
        // Update score widget
        m_scoreWidget->updateScore(score);
        
        DebugLogger::instance().log("MainWindow", 
            QString("QSO logged: %1 points, %2 total mults, %3 DXCCs, %4 total score")
                .arg(qso.getPoints())
                .arg(score.multipliers)
                .arg(score.dxccCount)
                .arg(score.contestScore));
    }
    
    clearEntryForm();
    m_callEdit->setFocus();
    
    // Update QSO count in status bar
    m_qsoCountLabel->setText(QString("QSOs: %1").arg(m_qsoModel->count()));
}

void MainWindow::onRigControl()
{
    RigControlDialog dialog(m_flrigClient, this);
    connect(&dialog, &RigControlDialog::pollIntervalChanged, this, [this](int ms) {
        m_rigPollTimer->setInterval(ms);
	// even though this is mainwindow, this belongs in the Flrig filter
        DebugLogger::instance().log("Flrig", QString("Rig poll interval changed to %1 ms").arg(ms));
    });
    dialog.exec();
}

void MainWindow::onRigConnected()
{
    m_rigStatusLabel->setText("Rig: Connected");
    m_rigStatusLabel->setStyleSheet("QLabel { color: green; }");
    m_rigPollTimer->start();
}

void MainWindow::onRigDisconnected()
{
    m_rigStatusLabel->setText("Rig: Disconnected");
    m_rigStatusLabel->setStyleSheet("QLabel { color: red; }");
    m_rigPollTimer->stop();
}

void MainWindow::onUpdateRigDisplay()
{
    if (!m_flrigClient->isConnected()) {
        m_rigPollTimer->stop();
        return;
    }
    
    // Request frequency, mode, and WPM from rig
    // Catch errors to prevent UI lag when radio is off or unresponsive
    double freq = 0;
    QString mode;
    int wpm = 0;
    
    try {
        freq = m_flrigClient->getFrequency();
        mode = m_flrigClient->getMode();
        wpm = m_flrigClient->getCWSpeed();
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
    }
    
    // Update mode if changed
    if (!mode.isEmpty() && mode != m_lastMode) {
        // Map common mode names
        QString mappedMode = mode;
        if (mode == "SSB" || mode == "PKTUSB") mappedMode = "USB";
        else if (mode == "PKTLSB") mappedMode = "LSB";
        else if (mode == "RTTY" || mode == "RTTYR") mappedMode = "DIG";
        
        m_lastMode = mappedMode;
        DebugLogger::instance().log("MainWindow", QString("Updated mode to %1").arg(mappedMode));
    }
    
    // Update WPM if changed and valid
    if (wpm > 0 && wpm != m_lastWpm) {
        m_lastWpm = wpm;
        m_wpmLabel->setText(QString("WPM: %1").arg(wpm));
    }
    
    // Update freq/mode button
    if (freq > 0 && !mode.isEmpty()) {
        m_freqModeButton->setText(QString("%1 %2")
            .arg(freq / 1000.0, 0, 'f', 1)
            .arg(mode));
    }
}

void MainWindow::onFreqModeButtonClicked()
{
    // Show frequency/mode entry dialog
    FreqModeDialog dialog(this);
    dialog.setFrequency(m_lastFrequency);
    dialog.setMode(m_lastMode);
    
    if (dialog.exec() == QDialog::Accepted) {
        double newFreq = dialog.frequency();
        QString newMode = dialog.mode();
        
        // Update local display
        m_lastFrequency = newFreq;
        m_lastMode = newMode;
        m_freqModeButton->setText(QString("%1 %2")
            .arg(newFreq, 0, 'f', 1)
            .arg(newMode));
        
        // Send to rig if connected
        if (m_flrigClient->isConnected()) {
            // Convert kHz to Hz for flrig
            m_flrigClient->setFrequency(newFreq * 1000.0);
            m_flrigClient->setMode(newMode);
            m_statusLabel->setText(QString("Rig set to %1 kHz %2")
                .arg(newFreq, 0, 'f', 1)
                .arg(newMode));
        }
        
        // Return focus to call field
        if (m_callEdit) {
            m_callEdit->setFocus();
            m_callEdit->selectAll();
        }
    }
}

void MainWindow::loadCWMemories()
{
    Settings& settings = Settings::instance();
    QList<CwMemory> memories = settings.getCwMemories();
    if (m_cwConsole) {
        m_cwConsole->setMemories(memories);
    }
}

void MainWindow::onEditCWMemories()
{
    Settings& settings = Settings::instance();
    
    CwMemoriesDialog dialog(this);
    dialog.setMemories(settings.getCwMemories());
    
    if (dialog.exec() == QDialog::Accepted) {
        QList<CwMemory> memories = dialog.getMemories();
        settings.setCwMemories(memories);
        
        // Update CW console with new memories
        if (m_cwConsole) {
            m_cwConsole->setMemories(memories);
        }
        
        m_statusLabel->setText("CW memories updated");
    }
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
    
    // Get all QSOs
    QList<QsoRecord> allQsos = m_qsoModel->getAllQsos();
    
    // Reset contest engine
    m_contestEngine->resetScore();
    
    // Re-score each QSO without popups
    QString myCallsign = Settings::instance().getCallsign();
    
    for (int i = 0; i < allQsos.count(); ++i) {
        QsoRecord qso = allQsos[i];
        
        // Check if out-of-band
        double freqKhz = qso.getFrequency().toDouble();
        if (!m_contestEngine->isValidBand(freqKhz)) {
            qso.setOutOfBand(true);
            qso.setComment("Out of band for contest");
            qso.setPoints(0);
            qso.setDupe(false);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            m_qsoModel->updateQso(i, qso);
            continue;
        }
        
        // Reset dupe and out-of-band flags
        qso.setOutOfBand(false);
        qso.setDupe(false);
        qso.setComment("");
        
        // Check for duplicates (against previously re-scored QSOs)
        QList<QsoRecord> previousQsos = allQsos.mid(0, i);
        bool isDupe = m_contestEngine->isDupe(qso, previousQsos);
        
        if (isDupe) {
            qso.setDupe(true);
            qso.setPoints(0);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            QString dupeReason = m_contestEngine->getDupeReason(qso, previousQsos);
            qso.setComment(QString("Duplicate contact for %1").arg(dupeReason));
            m_qsoModel->updateQso(i, qso);
            continue;
        }
        
        // Calculate points
        int points = m_contestEngine->calculatePoints(qso, myCallsign);
        qso.setPoints(points);
        
        // Get per-QSO multiplier credit
        ContestEngine::QsoMultiplierCredit credit = m_contestEngine->getQsoMultiplierCredit(qso, previousQsos);
        qso.setMultiplierCount(credit.namedMultCount);
        qso.setDxccCount(credit.dxccMultCount);
        qso.setItuRegionCount(credit.ituRegionMultCount);
        
        m_qsoModel->updateQso(i, qso);
    }
    
    // Re-calculate running score
    allQsos = m_qsoModel->getAllQsos();
    m_contestEngine->updateRunningScore(allQsos, myCallsign, false);  // Suppress verbose logging
    
    // Update score widget
    if (m_scoreWidget) {
        ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
        m_scoreWidget->updateScore(score);
    }
    
    m_statusLabel->setText("Score recalculated");
    DebugLogger::instance().log("MainWindow", "Score recalculated for all QSOs");
}

void MainWindow::onCWWindow()
{
    // CW console is now always visible in the right panel
    // This slot can just set focus to it
    if (m_cwConsole) {
        m_cwConsole->setFocus();
    }
}

void MainWindow::onToggleFlrigDebug(bool checked)
{
    DebugLogger::instance().setFlrigDebugEnabled(checked);
    Settings::instance().setFlrigDebugEnabled(checked);
    m_statusLabel->setText(checked ? "Flrig debug logging enabled" : "Flrig debug logging disabled");
}

void MainWindow::onToggleMainWindowDebug(bool checked)
{
    DebugLogger::instance().setMainWindowDebugEnabled(checked);
    Settings::instance().setMainWindowDebugEnabled(checked);
    m_statusLabel->setText(checked ? "MainWindow debug logging enabled" : "MainWindow debug logging disabled");
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

void MainWindow::onToggleCWWindowDebug(bool checked)
{
    DebugLogger::instance().setCWWindowDebugEnabled(checked);
    Settings::instance().setCWWindowDebugEnabled(checked);
    m_statusLabel->setText(checked ? "CWWindow debug logging enabled" : "CWWindow debug logging disabled");
}

void MainWindow::onToggleDxccDatabaseDebug(bool checked)
{
    DebugLogger::instance().setDxccDatabaseDebugEnabled(checked);
    Settings::instance().setDxccDatabaseDebugEnabled(checked);
    m_statusLabel->setText(checked ? "DxccDatabase debug logging enabled" : "DxccDatabase debug logging disabled");
}

void MainWindow::onToggleScpDebug(bool checked)
{
    DebugLogger::instance().setScpDebugEnabled(checked);
    Settings::instance().setScpDebugEnabled(checked);
    m_statusLabel->setText(checked ? "Super Check Partial debug logging enabled" : "Super Check Partial debug logging disabled");
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
    dialog.setCallsign(Settings::instance().getCallsign());
    
    // Get the final contest score from the score widget (which is the only accurate representation)
    int totalScore = m_scoreWidget ? m_scoreWidget->getFinalScore() : 0;
    dialog.setClaimedScore(totalScore);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Ask where to save - use callsign as default filename
    QString defaultDir = QDir::homePath();
    QString callsign = Settings::instance().getCallsign();
    QString defaultFileName = callsign.isEmpty() ? "cabrillo.log" : callsign.toLower() + ".log";
    
    QString fileName = QFileDialog::getSaveFileName(this, "Export Cabrillo Log",
        defaultDir + "/" + defaultFileName, "Log Files (*.log);;Cabrillo Files (*.cbr *.cab);;Text Files (*.txt);;All Files (*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Export
    CabrilloExport exporter;
    QString myCallsign = Settings::instance().getCallsign();
    QString selectedMode = m_contestEngine->getStationClassMode();
    if (!exporter.exportToFile(fileName, m_qsoModel->getAllQsos(), m_contestDefinition, dialog.getHeaderData(), myCallsign, selectedMode)) {
        QMessageBox::critical(this, "Export Failed", "Failed to export Cabrillo log:\n" + exporter.lastError());
        return;
    }
    
    QMessageBox::information(this, "Export Successful", QString("Cabrillo log exported to:\n%1").arg(fileName));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About ContestLogX",
        "ContestLogX - Version 0.0.9 (Alpha)\n\n"
        "Cross-platform amateur radio contest logging software\n\n"
        "Radio control via flrig (http://www.w1hkj.com/)\n\n"
        "Copyright (c) 2025-2026, by Steve Woodruff, N9OH");
}

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
    
    m_callEdit->setFocus();
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
    
    return true;
}

void MainWindow::saveWindowGeometry()
{
    Settings& settings = Settings::instance();
    settings.setWindowGeometry(geometry());
    settings.setWindowMaximized(isMaximized());
}

void MainWindow::restoreWindowGeometry()
{
    Settings& settings = Settings::instance();
    QRect geom = settings.getWindowGeometry();
    
    setGeometry(geom);
    
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
    QString propText = QString("SFI %1  A %2  K %3").arg(sfi).arg(aIndex).arg(kIndex);
    m_propagationLabel->setText(propText);
}

void MainWindow::onDxSpotClicked(const QString& callsign, double frequency, const QString& mode)
{
    DebugLogger::instance().log("MainWindow", QString("DX spot clicked: call=%1, changing rig to %2 kHz, mode %3").arg(callsign).arg(frequency).arg(mode));
    
    // Set callsign in QSO entry field
    m_callEdit->setText(callsign.toUpper());
    m_callEdit->setFocus();
    
    // Change rig frequency and mode via flrig
    if (m_flrigClient && m_flrigClient->isConnected()) {
        m_flrigClient->setFrequency(static_cast<long>(frequency * 1000)); // Convert kHz to Hz
        m_flrigClient->setMode(mode);
    }
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
    
    // Save dock widget state (positions, sizes, floating state)
    QByteArray dockState = saveState();
    settings.setDockWidgetState(dockState);
    DebugLogger::instance().log("MainWindow", 
        QString("Saved dock widget state (%1 bytes)").arg(dockState.size()));
}

void MainWindow::restorePanelState()
{
    Settings& settings = Settings::instance();
    
    // Restore dock widget visibility
    bool dxVisible = settings.getDxClusterVisible();
    bool cwVisible = settings.getCwConsoleVisible();
    
    if (m_dxClusterDock) {
        m_dxClusterDock->setVisible(dxVisible);
    }
    if (m_cwConsoleDock) {
        m_cwConsoleDock->setVisible(cwVisible);
    }
    
    // Update menu actions
    if (m_dxClusterAction) {
        m_dxClusterAction->setChecked(dxVisible);
    }
    if (m_cwConsoleAction) {
        m_cwConsoleAction->setChecked(cwVisible);
    }
    
    // Restore splitter state
    if (m_mainSplitter) {
        QByteArray state = settings.getMainSplitterState();
        if (!state.isEmpty()) {
            m_mainSplitter->restoreState(state);
        }
    }
    
    // Restore dock widget state (positions, sizes, floating state)
    QByteArray dockState = settings.getDockWidgetState();
    if (!dockState.isEmpty()) {
        DebugLogger::instance().log("MainWindow", 
            QString("Restoring dock widget state (%1 bytes)").arg(dockState.size()));
        // Use version 0 for compatibility
        bool success = restoreState(dockState, 0);
        DebugLogger::instance().log("MainWindow", 
            QString("Dock state restore %1").arg(success ? "succeeded" : "failed"));
        
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
    } else {
        DebugLogger::instance().log("MainWindow", "No saved dock widget state found");
    }
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
        DebugLogger::instance().log("MainWindow", "Station class restore skipped (new log)");
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
            StationClassDialog dialog(
                m_contestEngine->getStationClassPrompt(),
                m_contestEngine->getStationClassOptions(),
                this,
                currentClass  // Pass current class as default
            );
            
            if (dialog.exec() == QDialog::Accepted) {
                QString selectedClass = dialog.getSelectedClass();
                m_contestEngine->setStationClass(selectedClass);
                DebugLogger::instance().log("MainWindow", 
                    QString("Station class selected: %1").arg(selectedClass));
                
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
    
    // Update the model
    m_qsoModel->setColumnHeaders(fullHeaders);
    
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
    m_qsoEntryLayout->addWidget(m_logButton);
    
    // Set proper tab order: fields in order -> Log button
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
        setTabOrder(lastWidget, m_logButton);
    }
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
    }
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
    QString callsign = Settings::instance().getCallsign();
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
    
    QTextStream out(&file);
    
    // Header
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "CONTEST SUMMARY SHEET\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "\n";
    
    out << "Contest: " << contestName << "\n";
    out << "Callsign: " << Settings::instance().getCallsign() << "\n";
    
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
        
        // Find gaps of offTimeGapThreshold or more minutes
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
    
    // Score Summary from Score Widget
    if (m_scoreWidget) {
        out << "SCORING SUMMARY\n";
        out << "-" << QString("-").repeated(63) << "-" << "\n";
        
        // Get the score table data
        QTableWidget* scoreTable = nullptr;
        for (QObject* child : m_scoreWidget->children()) {
            if (QTableWidget* table = qobject_cast<QTableWidget*>(child)) {
                scoreTable = table;
                break;
            }
        }
        
        if (scoreTable) {
            // Print table header
            for (int col = 0; col < scoreTable->columnCount(); ++col) {
                QString header = scoreTable->horizontalHeaderItem(col)->text();
                out << QString("%1").arg(header, 12);
            }
            out << "\n";
            out << "-" << QString("-").repeated(63) << "-" << "\n";
            
            // Print table data
            for (int row = 0; row < scoreTable->rowCount(); ++row) {
                for (int col = 0; col < scoreTable->columnCount(); ++col) {
                    QTableWidgetItem* item = scoreTable->item(row, col);
                    QString text = item ? item->text() : "";
                    out << QString("%1").arg(text, 12);
                }
                out << "\n";
            }
        }
        
        out << "\n";
    }
    
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
    if (score.namedMultCount > 0) {
        out << "Named Multipliers:        " << score.namedMultCount << "\n";
        multTypes.append("Named");
    }
    if (score.dxccMultCount > 0) {
        out << "DXCC Multipliers:         " << score.dxccMultCount << "\n";
        multTypes.append("DXCC");
    }
    if (score.ituRegionMultCount > 0) {
        out << "ITU Region Multipliers:   " << score.ituRegionMultCount << "\n";
        multTypes.append("ITU Region");
    }
    
    out << "\n";
    
    // Show the scoring calculation
    if (multTypes.size() == 1) {
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × " << score.multipliers << " multipliers";
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    } else if (multTypes.size() > 1) {
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × " << score.multipliers << " multipliers";
        out << " (" << multTypes.join(" + ") << ")";
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    }
    
    out << "\n";
    out << "CLAIMED SCORE: " << score.contestScore << "\n";
    out << "\n";
    
    // Multiplier Details
    out << "MULTIPLIER DETAILS\n";
    out << "-" << QString("-").repeated(63) << "-" << "\n";
    out << "\n";
    
    QString multType = m_contestEngine->getMultiplierType();
    QStringList multCategories = m_contestEngine->getMultiplierCategories();
    
    // Check if callsigns are multipliers
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
    
    // Handle callsign multipliers (e.g., CWops CWT)
    if (callsignIsMult) {
        QSet<QString> workedCalls;
        QSet<QString> countedCalls;
        
        for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
            QsoRecord qso = m_qsoModel->getQso(i);
            QString call = qso.getCall().toUpper();
            workedCalls.insert(call);
            if (!qso.isDupe()) {
                countedCalls.insert(call);
            }
        }
        
        if (!workedCalls.isEmpty()) {
            out << "Callsigns (Worked: " << countedCalls.size() << ")\n";
            
            QStringList sortedCalls = QStringList(workedCalls.begin(), workedCalls.end());
            std::sort(sortedCalls.begin(), sortedCalls.end());
            
            // Format with ~80 chars per line
            QString line;
            for (const QString& call : sortedCalls) {
                QString mark = countedCalls.contains(call) ? "*" : " ";
                QString entry = QString("%1%2 ").arg(mark).arg(call);
                
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
    
    // Process each multiplier category using a unified approach
    for (const QString& category : multCategories) {
        
        if (multType == "multsOnce") {
            // Simple case: just list all mults for this category
            QSet<QString> workedMults;
            QSet<QString> countedMults;
            
            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);
                
                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category) {
                        workedMults.insert(mult.value);
                        if (!qso.isDupe()) {
                            countedMults.insert(mult.value);
                        }
                    }
                }
            }
            
            if (!workedMults.isEmpty()) {
                QString categoryDisplay = (category == "named" || category == "namedMults") ? "Named Multipliers" : (category == "dxcc") ? "DXCC Entities" : category;
                out << categoryDisplay << " (Worked: " << countedMults.size() << ")\n";
                
                QStringList sortedMults = QStringList(workedMults.begin(), workedMults.end());
                std::sort(sortedMults.begin(), sortedMults.end());
                
                for (int i = 0; i < sortedMults.size(); ++i) {
                    QString mark = countedMults.contains(sortedMults[i]) ? "*" : " ";
                    out << QString("%1%2").arg(mark).arg(QString("%1").arg(sortedMults[i], -4));
                    
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
            QMap<QString, QSet<QString>> multsPerBand, countedPerBand;
            
            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                QString band = qso.getBand();
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);
                
                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category) {
                        multsPerBand[band].insert(mult.value);
                        if (!qso.isDupe()) {
                            countedPerBand[band].insert(mult.value);
                        }
                    }
                }
            }
            
            for (const auto& band : multsPerBand.keys()) {
                int counted = countedPerBand[band].size();
                QString categoryDisplay = (category == "named" || category == "namedMults") ? "Named Multipliers" : (category == "dxcc") ? "DXCC Entities" : category;
                out << categoryDisplay << " - " << band << " (Worked: " << counted << ")\n";
                
                QStringList sortedMults = QStringList(multsPerBand[band].begin(), multsPerBand[band].end());
                std::sort(sortedMults.begin(), sortedMults.end());
                
                for (int i = 0; i < sortedMults.size(); ++i) {
                    QString mark = countedPerBand[band].contains(sortedMults[i]) ? "*" : " ";
                    out << QString("%1%2").arg(mark).arg(QString("%1").arg(sortedMults[i], -4));
                    
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
            // Breakdown by mode
            QMap<QString, QSet<QString>> multsPerMode, countedPerMode;
            
            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                QString mode = qso.getMode();
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);
                
                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category) {
                        multsPerMode[mode].insert(mult.value);
                        if (!qso.isDupe()) {
                            countedPerMode[mode].insert(mult.value);
                        }
                    }
                }
            }
            
            for (const auto& mode : multsPerMode.keys()) {
                int counted = countedPerMode[mode].size();
                QString categoryDisplay = (category == "namedMults") ? "Named Multipliers" : (category == "dxcc") ? "DXCC Entities" : category;
                out << categoryDisplay << " - " << mode << " (Worked: " << counted << ")\n";
                
                QStringList sortedMults = QStringList(multsPerMode[mode].begin(), multsPerMode[mode].end());
                std::sort(sortedMults.begin(), sortedMults.end());
                
                for (int i = 0; i < sortedMults.size(); ++i) {
                    QString mark = countedPerMode[mode].contains(sortedMults[i]) ? "*" : " ";
                    out << QString("%1%2").arg(mark).arg(QString("%1").arg(sortedMults[i], -4));
                    
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
            QMap<QString, QSet<QString>> multsPerBandMode, countedPerBandMode;
            
            for (int i = 0; i < m_qsoModel->rowCount(); ++i) {
                QsoRecord qso = m_qsoModel->getQso(i);
                QString band = qso.getBand();
                QString mode = qso.getMode();
                QString key = band + "/" + mode;
                QList<ContestEngine::MultiplierInfo> mults = m_contestEngine->getMultipliersWithCategory(qso);
                
                for (const ContestEngine::MultiplierInfo& mult : mults) {
                    if (mult.category == category) {
                        multsPerBandMode[key].insert(mult.value);
                        if (!qso.isDupe()) {
                            countedPerBandMode[key].insert(mult.value);
                        }
                    }
                }
            }
            
            for (const auto& key : multsPerBandMode.keys()) {
                int counted = countedPerBandMode[key].size();
                QString categoryDisplay = (category == "namedMults") ? "Named Multipliers" : (category == "dxcc") ? "DXCC Entities" : category;
                out << categoryDisplay << " - " << key << " (Worked: " << counted << ")\n";
                
                QStringList sortedMults = QStringList(multsPerBandMode[key].begin(), multsPerBandMode[key].end());
                std::sort(sortedMults.begin(), sortedMults.end());
                
                for (int i = 0; i < sortedMults.size(); ++i) {
                    QString mark = countedPerBandMode[key].contains(sortedMults[i]) ? "*" : " ";
                    out << QString("%1%2").arg(mark).arg(QString("%1").arg(sortedMults[i], -4));
                    
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
    }
    
    out << "\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "Generated by: ContestLogX " << QApplication::applicationVersion() << "\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    
    file.close();
    
    QMessageBox::information(this, "Success", 
        QString("Summary sheet saved to:\n%1").arg(fileName));
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
    
    QString summary;
    QTextStream out(&summary);
    
    // Header
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "CONTEST SUMMARY SHEET\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "\n";
    
    out << "Contest: " << m_contestEngine->getContestName() << "\n";
    out << "Callsign: " << Settings::instance().getCallsign() << "\n";
    
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
        
        // Find gaps of offTimeGapThreshold or more minutes
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
    if (score.namedMultCount > 0) {
        out << "Named Multipliers:        " << score.namedMultCount << "\n";
        multTypes.append("Named");
    }
    if (score.dxccMultCount > 0) {
        out << "DXCC Multipliers:         " << score.dxccMultCount << "\n";
        multTypes.append("DXCC");
    }
    if (score.ituRegionMultCount > 0) {
        out << "ITU Region Multipliers:   " << score.ituRegionMultCount << "\n";
        multTypes.append("ITU Region");
    }
    
    out << "\n";
    
    // Show the scoring calculation
    if (multTypes.size() == 1) {
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × " << score.multipliers << " multipliers";
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    } else if (multTypes.size() > 1) {
        out << "Score Calculation:\n";
        out << "  " << score.contactScore << " points × " << score.multipliers << " multipliers";
        out << " (" << multTypes.join(" + ") << ")";
        if (score.bonusPoints > 0) {
            out << " + " << score.bonusPoints << " bonus";
        }
        out << " = " << score.contestScore << "\n";
    }
    
    out << "\n";
    out << "CLAIMED SCORE: " << score.contestScore << "\n";
    out << "\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    out << "Generated by: ContestLogX " << QApplication::applicationVersion() << "\n";
    out << "=" << QString("=").repeated(63) << "=" << "\n";
    
    // Log the summary
    DebugLogger::instance().log("MainWindow", "=== SUMMARY SHEET START ===");
    for (const QString& line : summary.split('\n')) {
        DebugLogger::instance().log("MainWindow", line);
    }
    DebugLogger::instance().log("MainWindow", "=== SUMMARY SHEET END ===");
}
