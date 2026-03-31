/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef FLRIGCLIENT_H
#define FLRIGCLIENT_H

#include <QTcpSocket>
#include "rigInterface.h"

/**
 * @brief flrig XML-RPC client for radio control
 *
 * Communicates with flrig server via XML-RPC over TCP
 */
class FlrigClient : public RigInterface
{
    Q_OBJECT

public:
    explicit FlrigClient(QObject *parent = nullptr);
    ~FlrigClient();

    bool connectToRig(const QString& host, int port) override;
    void disconnectFromRig() override;
    bool isConnected() const override;

    QString getMode() override;
    bool setMode(const QString& mode) override;

    double getFrequency() override;
    bool setFrequency(double freqHz) override;

    QString getRigName() override;
    bool sendCW(const QString& text) override;

    int getCWSpeed() override;
    bool setCWSpeed(int wpm) override;
    bool stopCW() override;

    bool getPTT() override;
    bool setPTT(bool enable) override;

    int getPower() override;
    bool setPower(int watts) override;

    int getBandwidth() override;
    bool setBandwidth(int hz) override;

    QString getVFO() override;
    bool setVFO(const QString& vfo) override;

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
