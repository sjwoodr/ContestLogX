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

#ifndef DXCLUSTERPANEL_H
#define DXCLUSTERPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTcpSocket>
#include <QComboBox>
#include <QTextEdit>
#include <QDialog>
#include <QCheckBox>

class DxClusterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DxClusterPanel(QWidget *parent = nullptr);
    ~DxClusterPanel();
    
    void loadSettings();
    void saveSettings();
    void removeSpot(const QString& callsign);
    void setTableFont(const QFont& font);

public slots:
    void setSpotCommand(const QString& callsign, double freqKhz);

signals:
    void propagationDataReceived(int sfi, int aIndex, int kIndex);
    void spotClicked(const QString& callsign, double frequency, const QString& mode);
    void spotLastQsoRequested();

private slots:
    void onSpotClicked(int row, int column);
    void onConnect();
    void onDisconnect();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError();
    void onViewChanged(int index);
    void onSendCommand();
    void onPropagationTimerTimeout();
    void onExpireSpots();
    void onSpotLastQso();

private:
    void setupUi();
    void addSpot(const QString& callsign, double frequency, const QString& spotter, const QString& comment);
    void showLoginDialog();
    void sendLoginAndCommands();
    
    QTableWidget *m_spotTable;
    QTextEdit *m_consoleText;
    QComboBox *m_clusterEdit;
    QLineEdit *m_commandEdit;
    QPushButton *m_connectButton;
    QComboBox *m_viewCombo;
    QCheckBox *m_autoScrollCheckBox;
    QTcpSocket *m_socket;
    QTimer *m_propagationTimer;
    QTimer *m_expirationTimer;
    bool m_isConnected;
    bool m_loginSent;
    QString m_loginBuffer;
    QString m_callsign;
};

#endif // DXCLUSTERPANEL_H
