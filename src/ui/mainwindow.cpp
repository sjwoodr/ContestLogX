#include "mainwindow.h"
#include "rigcontroldialog.h"
#include "freqmodedialog.h"
#include "cwwindow.h"
#include "cwmemoriesdialog.h"
#include "dxclusterpanel.h"
#include "stationsetupdialog.h"
#include "stationclassdialog.h"
#include "contestselectdialog.h"
#include "contestengine.h"
#include "filehandler.h"
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
    // Save window geometry on exit
    saveWindowGeometry();
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
    
    // RIGHT SIDE: DX Cluster + CW Console stacked vertically with splitter
    m_rightPanelSplitter = new QSplitter(Qt::Vertical, this);
    
    // DX Cluster Panel (top of right side)
    m_dxClusterPanel = new DxClusterPanel(this);
    m_dxClusterPanel->setMinimumHeight(200);
    m_rightPanelSplitter->addWidget(m_dxClusterPanel);
    
    // Connect propagation data signal
    connect(m_dxClusterPanel, &DxClusterPanel::propagationDataReceived, 
            this, &MainWindow::onPropagationDataReceived);
    
    // Connect spot clicked signal to change rig frequency/mode
    connect(m_dxClusterPanel, &DxClusterPanel::spotClicked,
            this, &MainWindow::onDxSpotClicked);
    
    // CW Console (bottom of right side, always visible)
    m_cwConsole = new CWWindow(m_flrigClient, this);
    m_cwConsole->setMinimumHeight(250);
    m_rightPanelSplitter->addWidget(m_cwConsole);
    
    mainSplitter->addWidget(m_rightPanelSplitter);
    
    // Store splitter references
    m_mainSplitter = mainSplitter;
    
    // Set initial splitter sizes (70% left, 30% right)
    mainSplitter->setStretchFactor(0, 7);
    mainSplitter->setStretchFactor(1, 3);
    
    // Restore splitter sizes from settings
    Settings& settings = Settings::instance();
    QList<int> mainSizes = settings.getMainSplitterSizes();
    if (!mainSizes.isEmpty() && mainSizes.size() == 2) {
        m_mainSplitter->setSizes(mainSizes);
    }
    
    QList<int> rightSizes = settings.getRightPanelSplitterSizes();
    if (!rightSizes.isEmpty() && rightSizes.size() == 2) {
        m_rightPanelSplitter->setSizes(rightSizes);
    }
    
    // Connect splitter moved signals to save sizes
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, [this]() {
        Settings::instance().setMainSplitterSizes(m_mainSplitter->sizes());
    });
    
    connect(m_rightPanelSplitter, &QSplitter::splitterMoved, this, [this]() {
        Settings::instance().setRightPanelSplitterSizes(m_rightPanelSplitter->sizes());
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
                // User selected "Open Existing" - load the .clx file
                QList<QsoRecord> loadedQsos;
                QString contestFile;
                QString stationClass;
                FileHandler fileHandler;
                
                if (fileHandler.loadWl2WithContest(selectedFile, loadedQsos, contestFile, stationClass)) {
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
                    
                    m_qsoModel->clear();
                    for (const QsoRecord& qso : loadedQsos) {
                        m_qsoModel->addQso(qso);
                    }
                    m_currentFile = selectedFile;
                    m_isModified = false;
                    updateWindowTitle();
                    updateQsoEntryFields();
                    m_statusLabel->setText(QString("Loaded %1 QSOs").arg(loadedQsos.size()));
                } else {
                    QMessageBox::warning(this, "Error", "Failed to load file");
                }
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
    
    // Load file
    QList<QsoRecord> loadedQsos;
    FileHandler fileHandler;
    
    if (fileHandler.load(fileName, loadedQsos)) {
        m_qsoModel->clear();
        for (const QsoRecord& qso : loadedQsos) {
            m_qsoModel->addQso(qso);
        }
        
        m_currentFile = fileName;
        m_isModified = false;
        updateWindowTitle();
        m_statusLabel->setText("File loaded: " + fileName + " (" + 
            QString::number(loadedQsos.count()) + " QSOs)");
    } else {
        QMessageBox::warning(this, "Load Failed", 
            "Failed to load file:\n\n" + fileHandler.lastError());
    }
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
    qso.setMode(m_lastMode);
    qso.setDateTime(QDateTime::currentDateTimeUtc());
    qso.setSerial(m_qsoModel->count() + 1);
    
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
    if (!m_contestDefinition.isEmpty()) {
        // Check for dupes
        QList<QsoRecord> existingQsos = m_qsoModel->getAllQsos();
        if (m_contestEngine->isDupe(qso, existingQsos)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                "Duplicate QSO",
                QString("Duplicate: %1. Log anyway?").arg(qso.getCall()),
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::No) {
                return;
            }
        }
        
        // Validate exchange fields
        DebugLogger::instance().log("MainWindow", QString("About to validate QSO - RST: '%1'").arg(qso.getRstReceived()));
        QString errorMsg;
        if (!m_contestEngine->validateQso(qso, errorMsg)) {
            DebugLogger::instance().log("MainWindow", QString("Contest validation failed: %1").arg(errorMsg));
            QMessageBox::warning(this, "Invalid Exchange", errorMsg);
            return;
        }
        
        // Calculate points (pass station callsign)
        QString myCallsign = Settings::instance().getCallsign();
        int points = m_contestEngine->calculatePoints(qso, myCallsign);
        qso.setPoints(points);
        DebugLogger::instance().log("MainWindow", 
            QString("QSO worth %1 points").arg(points));
    }
    
    // Calculate and set multiplier/DXCC counts
    QList<QsoRecord> existingQsos = m_qsoModel->getQsos();
    QSet<QString> workedMultipliers;
    
    // Collect all multipliers worked so far
    for (const QsoRecord& existingQso : existingQsos) {
        QStringList mults = m_contestEngine->getMultipliers(existingQso);
        for (const QString& mult : mults) {
            workedMultipliers.insert(mult.toUpper());
        }
    }
    
    // Check if this QSO provides a new multiplier
    QStringList newMults = m_contestEngine->getMultipliers(qso);
    for (const QString& mult : newMults) {
        if (!workedMultipliers.contains(mult.toUpper())) {
            workedMultipliers.insert(mult.toUpper());
        }
    }
    
    qso.setMultiplierCount(workedMultipliers.size());
    qso.setDxccCount(1);  // TODO: Implement DXCC tracking
    
    m_qsoModel->addQso(qso);
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
    double freq = m_flrigClient->getFrequency();
    QString mode = m_flrigClient->getMode();
    int wpm = m_flrigClient->getCWSpeed();
    
    // even though this is mainwindow, this belongs in the Flrig filter
    DebugLogger::instance().log("Flrig", QString("Rig poll: freq=%1 mode=%2 wpm=%3").arg(freq).arg(mode).arg(wpm));
    
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
        "ContestLogX - Version 0.0.1 (Alpha)\n\n"
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
    if (m_dxClusterPanel) {
        m_dxClusterPanel->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::onToggleCwConsole(bool checked)
{
    if (m_cwConsole) {
        m_cwConsole->setVisible(checked);
        savePanelState();
    }
}

void MainWindow::savePanelState()
{
    Settings& settings = Settings::instance();
    
    // Save panel visibility
    settings.setDxClusterVisible(m_dxClusterPanel && m_dxClusterPanel->isVisible());
    settings.setCwConsoleVisible(m_cwConsole && m_cwConsole->isVisible());
    
    // Save splitter states
    if (m_mainSplitter) {
        settings.setMainSplitterState(m_mainSplitter->saveState());
    }
    if (m_rightPanelSplitter) {
        settings.setRightPanelSplitterState(m_rightPanelSplitter->saveState());
    }
}

void MainWindow::restorePanelState()
{
    Settings& settings = Settings::instance();
    
    // Restore panel visibility
    bool dxVisible = settings.getDxClusterVisible();
    bool cwVisible = settings.getCwConsoleVisible();
    
    if (m_dxClusterPanel) {
        m_dxClusterPanel->setVisible(dxVisible);
    }
    if (m_cwConsole) {
        m_cwConsole->setVisible(cwVisible);
    }
    
    // Update menu actions
    if (m_dxClusterAction) {
        m_dxClusterAction->setChecked(dxVisible);
    }
    if (m_cwConsoleAction) {
        m_cwConsoleAction->setChecked(cwVisible);
    }
    
    // Restore splitter states
    if (m_mainSplitter) {
        QByteArray state = settings.getMainSplitterState();
        if (!state.isEmpty()) {
            m_mainSplitter->restoreState(state);
        }
    }
    if (m_rightPanelSplitter) {
        QByteArray state = settings.getRightPanelSplitterState();
        if (!state.isEmpty()) {
            m_rightPanelSplitter->restoreState(state);
        }
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

