/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressDialog>
#include <QSplitter>
#include <QTableView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTimer>
#include <QJsonObject>
#include "qsoListModel.h"
#include "qsoRecord.h"
#include "flrigClient.h"
#include "bandMapWidget.h"
#include "qsoEditDialog.h"
#include "qrzcqApi.h"
#include "qrzApi.h"
#include "stationInfo.h"
#include "cwMemory.h"
#include "ssbMemory.h"
#include "memoryRole.h"
#include "rateWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    void loadLogFile(const QString& filename);
    void setTestMode(bool testMode) { m_testMode = testMode; }

    // Get session station callsign (from loaded log or Settings)
    QString getSessionCallsign() const;

    // Public method for self-test

private slots:
    void onNewLog();
    void onOpenLog();
    void onSaveLog();
    void onSaveLogAs();
    void onExportAdif();
    void onImportAdif();
    void onExit();
    
    void onRigControl();
    void onRigConnected();
    void onRigDisconnected();
    void onUpdateRigDisplay();
    void onEditSsbMemories();
    void onSsbKeyingSetup();
    
    void onCallChanged(const QString& text);
    void onModeChanged(int index);
    void onExchangeChanged(const QString& text);
    void onLogQso();
    void onQrzLookup();
    void onQrzcqSessionObtained(const QString& token);
    void onQrzcqSessionError(const QString& error);
    void onQrzcqCallsignFound(const QrzcqCallsignData& data);
    void onQrzcqCallsignNotFound(const QString& callsign);
    void onQrzcqLookupError(const QString& error);

    void onQrzSessionObtained(const QString& token);
    void onQrzSessionError(const QString& error);
    void onQrzCallsignFound(const QrzCallsignData& data);
    void onQrzCallsignNotFound(const QString& callsign);
    void onQrzLookupError(const QString& error);
    
    void onFreqModeButtonClicked();
    void onCWWindow();
    void onEditCWMemories();
    void onRecalculateScore();
    void onContestSetup();
    void onOperatorCallDialog();
    void onExportCabrillo();
    void onCreateSummarySheet();
    void onToggleOnlineScoring(bool enabled);
    void onPostScore();
    void onScorePostSuccess(const QString& timestamp);
    void onScorePostFailed(const QString& error);
    void onScorePostAuthFailed();
    void onAbout();
    void onColumnResized(int logicalIndex, int oldSize, int newSize);
    void onPropagationDataReceived(int sfi, int aIndex, int kIndex);
    void onDxSpotClicked(const QString& callsign, double frequency, const QString& mode);
    void onSpotLastQso();
    void onToggleDxCluster(bool checked);
    void onToggleCwConsole(bool checked);
    void onToggleScoreWidget(bool checked);
    void onToggleScpWidget(bool checked);
    void onToggleSsbMemoriesWidget(bool checked);
    void onToggleMultiplierWidget(bool checked);
    void onToggleRateWidget(bool checked);
    void onShowMultipliers();
    void onResetWidgetPositions();
    void onToggleFlrigDebug(bool checked);
    void onToggleMainWindowDebug(bool checked);
    void onToggleContestEngineDebug(bool checked);
    void onToggleContestSelectDialogDebug(bool checked);
    void onToggleCWWindowDebug(bool checked);
    void onToggleDxccDatabaseDebug(bool checked);
    void onToggleDxClusterDebug(bool checked);
    void onToggleScpDebug(bool checked);
    void onToggleMultiplierWidgetDebug(bool checked);
    void onToggleCallsignLookupDebug(bool checked);
    void onDownloadCtyDat();
    void onDownloadScp();
    void checkDataFileStaleness();
    void onManageCallHistory();
    void onScpDialog();
    void onPreferences();
    void applyFontSettings();
    void onQsoDoubleClicked(const QModelIndex& index);
    void onQsoContextMenuRequested(const QPoint& pos);
    void onEditQso();
    void onDeleteQso();
    void onDupeFlashTimeout();
    void onSsbMemoryTriggered(int memoryNumber, const QString& text);
    void onCwMemoryTriggered(int fKey, const QString& text);
    void onToggleRunSP();
    void onToggleMemoryType();
    void onQsoEntryReturn();
    void onTtsFinished();
    void onTtsError(const QString& error);

    // Band map
    void onSpotReceived(const SpotData &spot);
    void onToggleBandMap(bool checked);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void setupDocks(QSplitter* mainSplitter);
    void setupMenus();
    void createConnections();
    void createBandMapDock();
    ContactStatus resolveSpotStatus(const QString &callsign);
    void loadQsosIntoModel(const QList<QsoRecord>& qsos, QProgressDialog* progressDialog);
    void applyRestrictedModeFromUserPrompts();
    void promptForMissingUserPrompts();
    bool isFieldVisible(const QString& columnName) const;
    void loadCWMemories();
    void loadSsbMemories();
    void updateMemoryTypeLabel();
    void flashDupeWarning();
    
    void clearEntryForm();
    void preSaveCall();
    void triggerMemoryByRole(MemoryRole role);
    int findMemoryIndexByRole(MemoryRole role) const;
    void updateRunSPButtons();
    QMap<QString, QString> getExchangeFieldsForQso();
    QString getDupeQsoDetails(const QString& callsign, const QList<QsoRecord>& allQsos);
    void updateWindowTitle();
    bool maybeSave();
    void updateCallHistory();
    void updateScpWidgetMenuText();
    void saveWindowGeometry();
    void restoreWindowGeometry();
    void restoreColumnWidths();
    void savePanelState();
    void restorePanelState();
    QString freq2Mode(double freqMHz);
    bool loadContestDefinition(const QString& filePath, bool restoreStationClass = true);
    void initCallsignLookup();   // Set up active lookup API from Settings
    void triggerAutoLookup(const QString& callsign);  // Use active service
    void applyPendingStationInfo(QsoRecord& qso, const QString& callsign);
    void updateQsoEntryFields();
    void updateLogHeaders();
    bool isSemanticVersionEqual(const QString& v1, const QString& v2);
    QString generateSummaryString();  // Helper: generates summary and returns as QString
    void generateSummaryToDebugLog();

    // Crash-recovery backup
    void initializeBackup();
    void writeBackup();
    void removeBackup();
    void checkForCrashBackups();
    void resetBackupState();
    static QString sanitizeForFilename(const QString& s);
    
    // UI Components
    QLineEdit *m_callEdit;
    QLineEdit *m_exchangeEdit;
    QPushButton *m_logButton;
    QPushButton *m_clearButton;
    QPushButton *m_qrzButton;
    QLabel *m_returnToDockLabel;
    QTableView *m_qsoTable;
    
    QLabel *m_statusLabel;
    QWidget *m_filterBar;
    QLineEdit *m_filterEdit;
    class QShortcut *m_filterShortcut;
    QPushButton *m_freqModeButton;
    QLabel *m_contestNameLabel;
    QPushButton *m_memoryTypeLabel;
    QLabel *m_qsoCountLabel;
    QLabel *m_rigStatusLabel;
    QLabel *m_wpmLabel;
    QLabel *m_propagationLabel;
    
    // QSO entry dock (floatable, bottom area only)
    class QDockWidget *m_entryDock;

    // Right side panels (now as dock widgets)
    class DxClusterPanel *m_dxClusterPanel;
    class QDockWidget *m_dxClusterDock;
    class CWWindow *m_cwConsole;
    class QDockWidget *m_cwConsoleDock;
    class ScoreWidget *m_scoreWidget;
    class QDockWidget *m_scoreDock;
    class ScpWidget *m_scpWidget;  // ScpWidget IS a QDockWidget
    class QDockWidget *m_scpDock;   // Pointer to m_scpWidget for consistency
    class SsbMemoriesWidget *m_ssbMemoriesWidget;  // SsbMemoriesWidget IS a QDockWidget
    class MultiplierWidget *m_multiplierWidget;
    class QDockWidget *m_multiplierDock;
    RateWidget *m_rateWidget;
    class QDockWidget *m_rateDock;
    class QSplitter *m_mainSplitter;
    class QSplitter *m_rightPanelSplitter;

    // Band map
    class BandMapWidget *m_bandMapWidget = nullptr;
    QAction *m_bandMapWidgetAction = nullptr;
    QString m_lastBand; // tracks last-known band for change detection

    // Menu actions for toggleable panels
    QAction *m_floatEntryAction;
    QAction *m_dxClusterAction;
    QAction *m_cwConsoleAction;
    QAction *m_scoreWidgetAction;
    QAction *m_scpWidgetAction;
    QAction *m_ssbMemoriesWidgetAction;
    QAction *m_multiplierWidgetAction;
    QAction *m_rateWidgetAction;
    QAction *m_flrigDebugAction;
    QAction *m_mainWindowDebugAction;
    QAction *m_contestEngineDebugAction;
    QAction *m_contestSelectDialogDebugAction;
    QAction *m_cwWindowDebugAction;
    QAction *m_dxccDatabaseDebugAction;
    QAction *m_dxClusterDebugAction;
    QAction *m_scpDebugAction;
    QAction *m_multiplierWidgetDebugAction;
    QAction *m_callsignLookupDebugAction;

    // QSO Entry widgets
    QGroupBox *m_qsoEntryGroup;
    QHBoxLayout *m_qsoEntryLayout;
    
    // Data
    QsoListModel *m_qsoModel;
    QString m_currentFile;
    bool m_isModified;
    bool m_showingLogFileNotFoundDialog;
    bool m_testMode;  // Set when --test-only argument is provided
    bool m_debugLogMode;  // Set when --log argument is provided, triggers auto-summary to debug log
    bool m_firstShow;  // Track first show event for geometry restoration
    bool m_restoringState;  // Block save timer during state restoration
    QMetaObject::Connection m_entryWindowVisConn;  // QWindow::visibilityChanged for floating entry dock

    // Session station info - loaded from file or defaults to Settings, NOT saved to Settings
    class StationInfo *m_sessionStationInfo;
    
    // Contest definition
    QJsonObject m_contestDefinition;
    QString m_contestFile;
    QMap<QString, QLineEdit*> m_exchangeFields;
    QList<QLineEdit*> m_entryFieldOrder;  // Maintains order of text input fields for Space-to-advance
    QString m_fieldNavigationKeys;  // "space", "tab", or "both" - from contest definition
    class ContestEngine *m_contestEngine;
    class DxccDatabase *m_dxccDatabase;
    
    // Rig control
    FlrigClient *m_flrigClient;
    QTimer *m_rigPollTimer;
    QTimer *m_dupeFlashTimer;
    QTimer *m_dockStateSaveTimer;  // Debounce timer for saving dock state
    double m_lastFrequency;
    QString m_lastMode;
    int m_lastWpm;
    
    // Online score publishing
    class OnlineScoreClient *m_onlineScoreClient;
    QTimer *m_scorePostTimer;
    QAction *m_onlineScoringAction;
    QLabel *m_onlineScoringLabel;
    bool m_scorePostInFlight = false;

    // Callsign lookup APIs
    QrzcqApi *m_qrzcqApi;
    QrzApi   *m_qrzApi;

    // Most recent successful callsign lookup — cleared when QSO is logged
    QString m_pendingLookupCallsign;
    QMap<QString, QString> m_pendingStationInfo;  // ADIF-keyed station metadata

    // SSB Voice Keying
    class TtsManager *m_ttsManager;

    // Contest-specific memories
    QList<CwMemory> m_contestCwMemories;
    QList<SsbMemory> m_contestSsbMemories;
    bool m_useContestMemories = false;

    // Run / S&P operating mode
    enum class RunMode { Off, Run, SP };
    RunMode m_runMode = RunMode::Off;
    bool m_exchangeSent = false;   // True once Exchange memory has been sent this QSO
    QPushButton *m_offButton = nullptr;
    QPushButton *m_runButton = nullptr;
    QPushButton *m_spButton = nullptr;

    // UI state saving


    // Crash-recovery backup
    QString m_backupPath;
    bool m_backupEnabled;

    // Context menu
    int m_contextMenuRow;
};

#endif // MAINWINDOW_H
