/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
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
