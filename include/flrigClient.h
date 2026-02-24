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

#ifndef FLRIGCLIENT_H
#define FLRIGCLIENT_H

#include <QObject>
#include <QString>
#include <QTcpSocket>

/**
 * @brief Qt-based flrig XML-RPC client
 * 
 * Communicates with flrig server for radio control
 * Replaces the Windows-specific FlrigClient implementation
 */
class FlrigClient : public QObject
{
    Q_OBJECT

public:
    explicit FlrigClient(QObject *parent = nullptr);
    ~FlrigClient();
    
    bool connectToRig(const QString& host, int port);
    void disconnectFromRig();
    bool isConnected() const;
    
    // Rig control methods
    QString getMode();
    bool setMode(const QString& mode);
    
    double getFrequency();
    bool setFrequency(double freqHz);
    
    QString getRigName();
    bool sendCW(const QString& text);
    
    int getCWSpeed();
    bool setCWSpeed(int wpm);
    bool stopCW();  // Stop/clear CW buffer
    
    // PTT (Push To Talk) control
    bool getPTT();
    bool setPTT(bool enable);
    
    // Power level control
    int getPower();
    bool setPower(int watts);
    
    // Bandwidth control
    int getBandwidth();
    bool setBandwidth(int hz);
    
    // VFO control
    QString getVFO();  // Returns "A" or "B"
    bool setVFO(const QString& vfo);
    
signals:
    void connected();
    void disconnected();
    void error(const QString& errorString);
    void frequencyChanged(double freqHz);
    void modeChanged(const QString& mode);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onReadyRead();

private:
    QString buildXmlRpcCall(const QString& method, const QVariantList& params = QVariantList());
    QVariant parseXmlRpcResponse(const QString& xml);
    void sendRequest(const QString& xmlRequest);
    QVariant waitForResponse(int timeoutMs = 2000);
    void handleTimeout();
    
    QTcpSocket *m_socket;
    QString m_host;
    int m_port;
    QString m_responseBuffer;
};

#endif // FLRIGCLIENT_H
