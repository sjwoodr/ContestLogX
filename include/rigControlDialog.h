/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef RIGCONTROLDIALOG_H
#define RIGCONTROLDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QStackedWidget>
#include <QTabWidget>
#include "rigInterface.h"

/**
 * @brief Dialog for configuring and testing rig connections
 *
 * Supports multiple rig backends (flrig, Hamlib rigctld).
 * When SO2R is enabled, shows tabbed interface for Radio L and Radio R.
 */
class RigControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RigControlDialog(RigInterface* clientL, RigInterface* clientR,
                              bool so2rEnabled, QWidget *parent = nullptr);
    ~RigControlDialog();

signals:
    void pollIntervalChanged(int ms);
    void backendChanged(const QString& backend);
    void backendChangedR(const QString& backend);
    void so2rChanged(bool enabled);
    // Emitted on Apply/OK when the audio input device or mute settings change
    // for a specific radio. MainWindow responds by spawning or destroying the
    // corresponding CwDecoderWidget (SPEC-005 FR-001..FR-006).
    void audioConfigChanged(bool isRightRadio);

private slots:
    void onAccepted();
    void onSo2rToggled(bool checked);

private:
    // Per-radio UI widgets
    struct RadioWidgets {
        RigInterface* originalClient = nullptr;  // MainWindow's client (never deleted by dialog)
        RigInterface* tempClient = nullptr;      // dialog-owned temp for backend testing
        RigInterface* rigClient = nullptr;       // active client (original or temp)
        bool isRadioR = false;
        QString originalBackend;  // backend the live client was created with
        QComboBox* backendCombo = nullptr;
        QLabel* featureNoteLabel = nullptr;
        QStackedWidget* settingsStack = nullptr;
        QLineEdit* flrigHostEdit = nullptr;
        QSpinBox* flrigPortSpin = nullptr;
        QCheckBox* flrigAutoConnectCheck = nullptr;
        QLineEdit* hamlibHostEdit = nullptr;
        QSpinBox* hamlibPortSpin = nullptr;
        QCheckBox* hamlibAutoConnectCheck = nullptr;
        QCheckBox* mockedAutoConnectCheck = nullptr;
        QPushButton* connectButton = nullptr;
        QPushButton* disconnectButton = nullptr;
        QPushButton* testButton = nullptr;
        QLabel* statusLabel = nullptr;
        QLabel* rigNameLabel = nullptr;
        QLabel* attributionLabel = nullptr;
        // CW Decoder — per-radio audio input (SPEC-005)
        QComboBox* audioInputCombo = nullptr;    // populated from QMediaDevices + "(none)"
        QCheckBox* muteDecoderOnPttCheck = nullptr;
        QSpinBox* decoderPttGraceSpin = nullptr;
    };

    QWidget* createRadioPage(RadioWidgets& w);
    void initRadioPage(RadioWidgets& w);
    void loadSettings(RadioWidgets& w);
    void saveSettings(RadioWidgets& w);
    QString selectedBackend(const RadioWidgets& w) const;
    void onBackendChanged(RadioWidgets& w, int index);
    void onConnectClicked(RadioWidgets& w);
    void onDisconnectClicked(RadioWidgets& w);
    void onTestClicked(RadioWidgets& w);
    void updateConnectionStatus(RadioWidgets& w);
    void swapToTempClient(RadioWidgets& w, const QString& backend);
    void cleanupTempClient(RadioWidgets& w);

    RadioWidgets m_radioL;
    RadioWidgets m_radioR;
    bool m_so2rEnabled;
    QCheckBox* m_so2rCheck;
    QSpinBox* m_pollIntervalSpin;
    QTabWidget* m_tabWidget;
    QWidget* m_radioRPage;
};

#endif // RIGCONTROLDIALOG_H
