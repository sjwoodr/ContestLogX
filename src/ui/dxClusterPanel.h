/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
#include "bandMapWidget.h"

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
    void setBands(const QStringList& bands);

public slots:
    void setSpotCommand(const QString& callsign, double freqKhz);

signals:
    void propagationDataReceived(int sfi, int aIndex, int kIndex);
    void spotClicked(const QString& callsign, double frequency, const QString& mode);
    void spotLastQsoRequested();
    void spotReceived(const SpotData &spot);   // emitted after each parsed cluster spot
    void clusterConnectedChanged(bool connected); // emitted on connect/disconnect

private slots:
    void onSpotClicked(int row, int column);
    void onConnect();
    void onDisconnect();
    void onClusterSelectionChanged(const QString& text);
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError();
    void onViewChanged(int index);
    void onSendCommand();
    void onPropagationTimerTimeout();
    void onExpireSpots();
    void onSpotLastQso();
    void onBandFilterChanged(const QString& band);
    void onModeFilterChanged(const QString& mode);

private:
    void setupUi();
    void addSpot(const QString& callsign, double frequency, const QString& spotter, const QString& comment);
    void showLoginDialog();
    void sendLoginAndCommands();
    void applyRowFilter(int row);
    static QString modeCategory(const QString& mode, const QString& comment);

    QTableWidget *m_spotTable;
    QTextEdit *m_consoleText;
    QComboBox *m_clusterEdit;
    QLineEdit *m_commandEdit;
    QPushButton *m_connectButton;
    QComboBox *m_viewCombo;
    QCheckBox *m_autoScrollCheckBox;
    QComboBox *m_bandFilterCombo;
    QComboBox *m_modeFilterCombo;
    QTcpSocket *m_socket;
    QTimer *m_propagationTimer;
    QTimer *m_expirationTimer;
    bool m_isConnected;
    bool m_loginSent;
    QString m_loginBuffer;
    QString m_callsign;
    QString m_connectedServer;
};

#endif // DXCLUSTERPANEL_H
