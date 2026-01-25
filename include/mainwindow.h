/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTimer>
#include <QJsonObject>
#include "qsolistmodel.h"
#include "qsorecord.h"
#include "flrigclient.h"
#include "qsoeditdialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    void loadLogFile(const QString& filename);
    void setTestMode(bool testMode) { m_testMode = testMode; }
    
    // Public method for self-test

private slots:
    void onNewLog();
    void onOpenLog();
    void onSaveLog();
    void onSaveLogAs();
    void onStationSetup();
    void onExit();
    
    void onRigControl();
    void onRigConnected();
    void onRigDisconnected();
    void onUpdateRigDisplay();
    
    void onCallChanged(const QString& text);
    void onModeChanged(int index);
    void onExchangeChanged(const QString& text);
    void onLogQso();
    void onQrzLookup();
    
    void onFreqModeButtonClicked();
    void onCWWindow();
    void onEditCWMemories();
    void onRecalculateScore();
    void onContestSetup();
    void onExportCabrillo();
    void onCreateSummarySheet();
    void onAbout();
    void onColumnResized(int logicalIndex, int oldSize, int newSize);
    void onPropagationDataReceived(int sfi, int aIndex, int kIndex);
    void onDxSpotClicked(const QString& callsign, double frequency, const QString& mode);
    void onToggleDxCluster(bool checked);
    void onToggleCwConsole(bool checked);
    void onToggleScoreWidget(bool checked);
    void onToggleScpWidget(bool checked);
    void onToggleFlrigDebug(bool checked);
    void onToggleMainWindowDebug(bool checked);
    void onToggleContestEngineDebug(bool checked);
    void onToggleContestSelectDialogDebug(bool checked);
    void onToggleCWWindowDebug(bool checked);
    void onToggleDxccDatabaseDebug(bool checked);
    void onToggleScpDebug(bool checked);
    void onDownloadCtyDat();
    void onDownloadScp();
    void onManageCallHistory();
    void onScpDialog();
    void onShortcuts();
    void onQsoDoubleClicked(const QModelIndex& index);
    void onQsoContextMenuRequested(const QPoint& pos);
    void onEditQso();
    void onDeleteQso();
    void onDupeFlashTimeout();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void createConnections();
    void loadCWMemories();
    void flashDupeWarning();
    
    void clearEntryForm();
    void preSaveCall();
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
    void updateQsoEntryFields();
    void updateLogHeaders();
    bool isSemanticVersionEqual(const QString& v1, const QString& v2);
    QString generateSummaryString();  // Helper: generates summary and returns as QString
    void generateSummaryToDebugLog();
    
    // UI Components
    QLineEdit *m_callEdit;
    QLineEdit *m_exchangeEdit;
    QPushButton *m_logButton;
    QPushButton *m_qrzButton;
    QTableView *m_qsoTable;
    
    QLabel *m_statusLabel;
    QPushButton *m_freqModeButton;
    QLabel *m_contestNameLabel;
    QLabel *m_qsoCountLabel;
    QLabel *m_rigStatusLabel;
    QLabel *m_wpmLabel;
    QLabel *m_propagationLabel;
    
    // Right side panels (now as dock widgets)
    class DxClusterPanel *m_dxClusterPanel;
    class QDockWidget *m_dxClusterDock;
    class CWWindow *m_cwConsole;
    class QDockWidget *m_cwConsoleDock;
    class ScoreWidget *m_scoreWidget;
    class QDockWidget *m_scoreDock;
    class ScpWidget *m_scpWidget;  // ScpWidget IS a QDockWidget
    class QDockWidget *m_scpDock;   // Pointer to m_scpWidget for consistency
    class QSplitter *m_mainSplitter;
    class QSplitter *m_rightPanelSplitter;
    
    // Menu actions for toggleable panels
    QAction *m_dxClusterAction;
    QAction *m_cwConsoleAction;
    QAction *m_scoreWidgetAction;
    QAction *m_scpWidgetAction;
    QAction *m_flrigDebugAction;
    QAction *m_mainWindowDebugAction;
    QAction *m_contestEngineDebugAction;
    QAction *m_contestSelectDialogDebugAction;
    QAction *m_cwWindowDebugAction;
    QAction *m_dxccDatabaseDebugAction;
    QAction *m_scpDebugAction;
    
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
    double m_lastFrequency;
    QString m_lastMode;
    int m_lastWpm;
    
    // UI state saving

    
    // Context menu
    int m_contextMenuRow;
};

#endif // MAINWINDOW_H
