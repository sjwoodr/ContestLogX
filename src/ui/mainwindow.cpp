#include "mainwindow.h"
#include "rigcontroldialog.h"
#include "freqmodedialog.h"
#include "cwwindow.h"
#include "cwmemoriesdialog.h"
#include "dxclusterpanel.h"
#include "scorewidget.h"
#include "stationsetupdialog.h"
#include "stationclassdialog.h"
#include "contestselectdialog.h"
#include "shortcutsdialog.h"
#include "contestengine.h"
#include "filehandler.h"
#include "loadingworker.h"
#include "settings.h"
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
    , m_contestEngine(new ContestEngine(this))
    , m_dxccDatabase(new DxccDatabase(this))
    , m_flrigClient(new FlrigClient(this))
    , m_rigPollTimer(new QTimer(this))
    , m_lastFrequency(14250.0)
    , m_lastMode("USB")
    , m_lastWpm(28)
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
        DebugLogger::instance().log("MainWindow", "Station configured, showing contest selection dialog");
        QTimer::singleShot(100, this, &MainWindow::onNewLog);
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
        
        // Check if Enter or Return was pressed in any QSO entry field
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(obj);
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
    m_callEdit = new QLineEdit();
    m_callEdit->setMaxLength(14);  // Standard callsign length
    m_callEdit->setMaximumWidth(120); // Max width for 10 chars
    // Force uppercase input
    connect(m_callEdit, &QLineEdit::textChanged, [this](const QString& text) {
        if (text != text.toUpper()) {
            int cursorPos = m_callEdit->cursorPosition();
            m_callEdit->setText(text.toUpper());
            m_callEdit->setCursorPosition(cursorPos);
        }
    });
    m_qsoEntryLayout->addWidget(m_callEdit);
    
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
                    FileHandler fileHandler;
                    
                    // We need to parse the file to get contest info - for now we'll load it separately
                    QList<QsoRecord> temp;
                    fileHandler.loadWl2WithContest(selectedFile, temp, contestFile, stationClass);
                    
                    // Load the contest definition if specified
                    if (!contestFile.isEmpty()) {
                        QString contestPath = QCoreApplication::applicationDirPath() + "/contests/" + contestFile;
                        if (!QFile::exists(contestPath)) {
                            contestPath = "contests/" + contestFile;
                        }
                        if (QFile::exists(contestPath)) {
                            loadContestDefinition(contestPath);
                            if (!stationClass.isEmpty()) {
                                m_contestEngine->setStationClass(stationClass);
                            }
                        }
                    }
                    
                    // Clear the model first
                    m_qsoModel->clear();
                    
                    // Recalculate scores on the loaded QSOs
                    progressDialog->setRange(0, loadedQsos.size() + 1);
                    progressDialog->setLabelText("Recalculating scores...");
                    progressDialog->show();
                    QApplication::processEvents();
                    
                    if (m_contestEngine) {
                        QString myCallsign = Settings::instance().getCallsign();
                        m_contestEngine->updateRunningScore(loadedQsos, myCallsign, false);  // Don't spam logs during load
                    }
                    
                    // Now add all QSOs to the model once
                    progressDialog->setLabelText("Updating display...");
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
                    
                    if (m_scoreWidget && m_contestEngine) {
                        m_scoreWidget->updateScore(m_contestEngine->getRunningScore());
                    }
                    
                    m_currentFile = selectedFile;
                    m_isModified = false;
                    updateWindowTitle();
                    updateQsoEntryFields();
                    
                    progressDialog->close();
                    progressDialog->deleteLater();
                    
                    m_statusLabel->setText(QString("Loaded %1 QSOs").arg(loadedQsos.size()));
                });
                
                loadThread->start();
            } else {
                // User selected a contest definition - create new log
                loadContestDefinition(selectedFile);
                
                m_qsoModel->clear();
                m_currentFile.clear();
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
        
        // Clear the model first
        m_qsoModel->clear();
        
        // Recalculate scores on the loaded QSOs
        progressDialog->setRange(0, loadedQsos.size() + 1);
        progressDialog->setLabelText("Recalculating scores...");
        progressDialog->show();
        QApplication::processEvents();
        
        if (m_contestEngine) {
            QString myCallsign = Settings::instance().getCallsign();
            m_contestEngine->updateRunningScore(loadedQsos, myCallsign, false);  // Don't spam logs during load
        }
        
        // Now add all QSOs to the model once
        progressDialog->setLabelText("Updating display...");
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
        
        if (m_scoreWidget && m_contestEngine) {
            m_scoreWidget->updateScore(m_contestEngine->getRunningScore());
        }
        
        progressDialog->close();
        progressDialog->deleteLater();
        
        m_currentFile = fileName;
        m_isModified = false;
        updateWindowTitle();
        m_statusLabel->setText("File loaded: " + fileName + " (" + 
            QString::number(loadedQsos.count()) + " QSOs)");
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
        success = fileHandler.saveWl2WithContest(m_currentFile, m_qsoModel->getQsos(), m_contestFile, m_contestDefinition, stationClass);
    } else {
        success = fileHandler.save(m_currentFile, m_qsoModel->getQsos());
    }
    
    if (success) {
        m_isModified = false;
        updateWindowTitle();
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
    
    // Auto-append .clx if no extension provided (default format)
    QFileInfo fileInfo(fileName);
    if (fileInfo.suffix().isEmpty()) {
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
    
    StationSetupDialog dialog(info, this);
    if (dialog.exec() == QDialog::Accepted) {
        StationInfo newInfo = dialog.stationInfo();
        settings.setCallsign(newInfo.callsign());
        settings.setOperatorName(newInfo.operatorName());
        settings.setGridSquare(newInfo.grid());
        settings.setState(newInfo.state());
        settings.save();
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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
        clearEntryForm();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::onCallChanged(const QString& text)
{
    // Force uppercase
    if (text != text.toUpper()) {
        int cursorPos = m_callEdit->cursorPosition();
        m_callEdit->setText(text.toUpper());
        m_callEdit->setCursorPosition(cursorPos);
    }
    // TODO: Implement call lookup, dupe checking
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
    
    DebugLogger::instance().log("MainWindow", 
        QString("QSO frequency set to: %1 kHz (m_lastFrequency=%2), band=%3").arg(qso.getFrequency()).arg(m_lastFrequency, 0, 'f', 1).arg(band));
    
    // Set RST sent based on mode (always auto-calculated)
    QString rstSent = (m_lastMode == "CW" || m_lastMode == "RTTY") ? "599" : 
                      (m_lastMode.contains("DIGI")) ? "+0" : "59";
    qso.setRstSent(rstSent);
    
    // Get exchange sent from station settings and contest class
    QString stationQth = Settings::instance().getState();
    int nextSerial = m_qsoModel->count() + 1;
    QString exchSent = m_contestEngine->getDefaultSentExchange(stationQth, nextSerial);
    qso.setExchangeSent(exchSent);
    
    // Set exchange fields from dynamic inputs (received exchange)
    if (!m_exchangeFields.isEmpty()) {
        DebugLogger::instance().log("MainWindow", 
            QString("Processing %1 exchange fields").arg(m_exchangeFields.size()));
        
        for (auto it = m_exchangeFields.begin(); it != m_exchangeFields.end(); ++it) {
            QString fieldName = it.key();
            QString value = it.value()->text().trimmed().toUpper();
            
            DebugLogger::instance().log("MainWindow", 
                QString("Field: %1 = '%2'").arg(fieldName).arg(value));
            
            if (fieldName == "CALL") {
                // Already handled above
                continue;
            } else if (fieldName == "RSTr") {
                DebugLogger::instance().log("MainWindow", 
                    QString("Setting RST Received: '%1' (isEmpty=%2)").arg(value).arg(value.isEmpty()));
                if (value.isEmpty()) {
                    QMessageBox::warning(this, "Invalid QSO", "RST Received cannot be empty");
                    return;
                }
                qso.setRstReceived(value);
                DebugLogger::instance().log("MainWindow", "RST Received set successfully");
            } else if (fieldName.startsWith("EXCH")) {
                DebugLogger::instance().log("MainWindow", 
                    QString("Setting Exchange Received: '%1'").arg(value));
                qso.setExchangeReceived(value);
            } else {
                // Store as generic exchange field
                qso.setExchangeField(fieldName, value);
            }
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
        m_contestEngine->updateRunningScore(allQsos, myCallsign, false);  // Suppress verbose logging
        
        // Get the running score which includes calculated multipliers
        ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
        
        // Update the multiplier and DXCC count on the QSO we just added
        // This represents the totals AFTER this QSO
        int lastQsoIndex = m_qsoModel->count() - 1;
        m_qsoModel->updateMultiplierCount(lastQsoIndex, score.multipliers);
        m_qsoModel->updateDxccCount(lastQsoIndex, score.dxccCount);
        
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
            QString dupeReason = m_contestEngine->getDupeReason(qso, previousQsos);
            qso.setComment(QString("Duplicate contact for %1").arg(dupeReason));
            m_qsoModel->updateQso(i, qso);
            continue;
        }
        
        // Calculate points
        int points = m_contestEngine->calculatePoints(qso, myCallsign);
        qso.setPoints(points);
        m_qsoModel->updateQso(i, qso);
    }
    
    // Re-calculate running score
    allQsos = m_qsoModel->getAllQsos();
    m_contestEngine->updateRunningScore(allQsos, myCallsign, false);  // Suppress verbose logging
    
    // Update all QSO multiplier and DXCC counts
    ContestEngine::ContestScore score = m_contestEngine->getRunningScore();
    for (int i = 0; i < allQsos.count(); ++i) {
        // For simplicity, just update the last one's display (user can scroll to see)
        if (i == allQsos.count() - 1) {
            m_qsoModel->updateMultiplierCount(i, score.multipliers);
            m_qsoModel->updateDxccCount(i, score.dxccCount);
        }
    }
    
    // Update score widget
    if (m_scoreWidget) {
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

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About ContestLogX",
        "ContestLogX - Version 0.0.2 (Alpha)\n\n"
        "Cross-platform amateur radio contest logging software\n\n"
        "Radio control via flrig (http://www.w1hkj.com/)\n\n"
        "Copyright (c) 2025 N9OH Software");
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

void MainWindow::loadContestDefinition(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        DebugLogger::instance().log("MainWindow", 
            QString("Failed to load contest definition: %1").arg(filePath));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        DebugLogger::instance().log("MainWindow", "Invalid contest definition format");
        return;
    }
    
    m_contestDefinition = doc.object();
    m_contestFile = QFileInfo(filePath).fileName();
    
    // Load into contest engine
    if (!m_contestEngine->loadContest(m_contestDefinition)) {
        DebugLogger::instance().log("MainWindow", "Failed to load contest into engine");
        return;
    }
    
    // Check if station class selection is needed
    if (m_contestEngine->needsStationClass()) {
        // Use existing station class if available (from loaded file)
        QString currentClass = m_contestEngine->getStationClass();
        
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
        } else {
            DebugLogger::instance().log("MainWindow", "Station class selection cancelled");
            return;
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
    }
    
    updateWindowTitle();
}

void MainWindow::updateLogHeaders()
{
    // Always include standard required fields
    QStringList fullHeaders;
    fullHeaders << "DATE" << "TIME" << "CALL" << "FREQ" << "MODE" 
                << "RSTs" << "RSTr" << "EXCHs" << "EXCHr" 
                << "Nr" << "Dupe" << "M" << "C" << "P" << "COMMENT";
    
    // If contest is loaded, we might add contest-specific columns later
    // but for now all contests use the standard format
    
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

