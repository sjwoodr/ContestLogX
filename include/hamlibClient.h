/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef HAMLIBCLIENT_H
#define HAMLIBCLIENT_H

#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include "rigInterface.h"

class HamlibWorker;

/**
 * @brief Hamlib rigctld TCP client for radio control
 *
 * Communicates with rigctld daemon via its simple text protocol over TCP.
 * All socket I/O runs on a background thread to avoid blocking the UI.
 * Requires rigctld to be running externally (e.g. rigctld -m <model> -r /dev/ttyUSB0).
 */
class HamlibClient : public RigInterface
{
    Q_OBJECT

public:
    explicit HamlibClient(QObject *parent = nullptr);
    ~HamlibClient();

    bool connectToRig(const QString& host, int port) override;
    void disconnectFromRig() override;
    bool isConnected() const override;

    double getFrequency() override;
    bool setFrequency(double freqHz) override;
    QString getMode() override;
    bool setMode(const QString& mode) override;

    bool sendCW(const QString& text) override;
    bool stopCW() override;
    int getCWSpeed() override;
    bool setCWSpeed(int wpm) override;

    bool getPTT() override;
    bool setPTT(bool enable) override;

    int getPower() override;
    bool setPower(int watts) override;

    int getBandwidth() override;
    bool setBandwidth(int hz) override;

    QString getVFO() override;
    bool setVFO(const QString& vfo) override;

    QString getRigName() override;

    bool supportsCW() const override { return true; }
    bool supportsPTT() const override { return true; }
    bool supportsPower() const override { return true; }
    bool supportsBandwidth() const override { return true; }

private:
    QThread m_workerThread;
    HamlibWorker *m_worker;
    mutable QMutex m_mutex;

    // Cached values from background polling
    double m_cachedFreq;
    QString m_cachedMode;
    int m_cachedWpm;
    bool m_connected;
};

/**
 * @brief Worker object that runs on a background thread for rigctld I/O
 */
class HamlibWorker : public QObject
{
    Q_OBJECT

public:
    explicit HamlibWorker(QObject *parent = nullptr);
    ~HamlibWorker();

    bool isConnected() const { return m_connected; }
    QString lastRigName() const { return m_lastRigName; }

public slots:
    void doConnect(const QString& host, int port);
    void doDisconnect();
    void doPoll();

    // Setter commands — called via invokeMethod from main thread
    void doSetFrequency(double freqHz);
    void doSetMode(const QString& mode);
    void doSendCW(const QString& text);
    void doStopCW();
    void doSetCWSpeed(int wpm);
    void doSetPTT(bool enable);
    void doSetPower(int watts);
    void doSetBandwidth(int hz);
    void doSetVFO(const QString& vfo);
    void doGetRigName();

signals:
    void connectResult(bool success);
    void disconnected();
    void socketError(const QString& errorString);
    void pollResult(double freq, const QString& mode, int wpm);
    void rigNameResult(const QString& name);
    void setterResult(bool success);

private:
    QString sendCommand(const QString& command, int timeoutMs = 500);
    bool isSuccess(const QString& response) const;

    QTcpSocket *m_socket;
    bool m_connected;
    QString m_lastRigName;
};

#endif // HAMLIBCLIENT_H
