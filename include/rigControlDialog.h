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
#include "rigInterface.h"

/**
 * @brief Dialog for configuring and testing rig connections
 *
 * Supports multiple rig backends (flrig, Hamlib rigctld)
 */
class RigControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RigControlDialog(RigInterface* client, QWidget *parent = nullptr);
    ~RigControlDialog();

    /**
     * @brief Returns the selected backend name ("flrig" or "hamlib")
     */
    QString selectedBackend() const;

signals:
    void pollIntervalChanged(int ms);
    void backendChanged(const QString& backend);

private slots:
    void onBackendChanged(int index);
    void onConnectClicked();
    void onDisconnectClicked();
    void onTestClicked();
    void onAccepted();
    void onRigConnected();
    void onRigDisconnected();
    void onRigError(const QString& error);

private:
    void setupUi();
    void updateConnectionStatus();
    void loadSettings();
    void saveSettings();

    RigInterface* m_rigClient;

    // Backend selection
    QComboBox* m_backendCombo;
    QStackedWidget* m_settingsStack;
    QLabel* m_featureNoteLabel;

    // flrig settings (page 0)
    QLineEdit* m_flrigHostEdit;
    QSpinBox* m_flrigPortSpin;
    QCheckBox* m_flrigAutoConnectCheck;

    // Hamlib settings (page 1)
    QLineEdit* m_hamlibHostEdit;
    QSpinBox* m_hamlibPortSpin;
    QCheckBox* m_hamlibAutoConnectCheck;

    // Shared controls
    QSpinBox* m_pollIntervalSpin;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_testButton;
    QLabel* m_statusLabel;
    QLabel* m_rigNameLabel;
    QLabel* m_attributionLabel;
};

#endif // RIGCONTROLDIALOG_H
