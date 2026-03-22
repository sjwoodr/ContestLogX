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
#include <QPushButton>
#include <QLabel>
#include "flrigClient.h"

/**
 * @brief Dialog for configuring and testing flrig connection
 */
class RigControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RigControlDialog(FlrigClient* client, QWidget *parent = nullptr);
    ~RigControlDialog();

signals:
    void pollIntervalChanged(int ms);

private slots:
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
    
    FlrigClient* m_flrigClient;
    
    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpin;
    QSpinBox* m_pollIntervalSpin;
    QCheckBox* m_autoConnectCheck;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_testButton;
    QLabel* m_statusLabel;
    QLabel* m_rigNameLabel;
};

#endif // RIGCONTROLDIALOG_H
