/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef RIGINTERFACE_H
#define RIGINTERFACE_H

#include <QObject>
#include <QString>

/**
 * @brief Abstract base class for rig control interfaces
 *
 * Provides a common interface for different rig control backends
 * (flrig XML-RPC, Hamlib rigctld, etc.)
 */
class RigInterface : public QObject
{
    Q_OBJECT

public:
    explicit RigInterface(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~RigInterface() = default;

    virtual bool connectToRig(const QString& host, int port) = 0;
    virtual void disconnectFromRig() = 0;
    virtual bool isConnected() const = 0;

    // Frequency and mode
    virtual double getFrequency() = 0;
    virtual bool setFrequency(double freqHz) = 0;
    virtual QString getMode() = 0;
    virtual bool setMode(const QString& mode) = 0;

    // CW keying
    virtual bool sendCW(const QString& text) = 0;
    virtual bool stopCW() = 0;
    virtual int getCWSpeed() = 0;
    virtual bool setCWSpeed(int wpm) = 0;

    // PTT
    virtual bool getPTT() = 0;
    virtual bool setPTT(bool enable) = 0;

    // Power
    virtual int getPower() = 0;
    virtual bool setPower(int watts) = 0;

    // Bandwidth
    virtual int getBandwidth() = 0;
    virtual bool setBandwidth(int hz) = 0;

    // VFO
    virtual QString getVFO() = 0;
    virtual bool setVFO(const QString& vfo) = 0;

    // Rig info
    virtual QString getRigName() = 0;

    /**
     * @brief Whether this backend supports CW keying
     *
     * flrig supports full CW keying via cwio; Hamlib rigctld supports
     * send_morse but with limited speed control.
     */
    virtual bool supportsCW() const { return true; }

    /**
     * @brief Whether this backend supports PTT control
     */
    virtual bool supportsPTT() const { return true; }

    /**
     * @brief Whether this backend supports power level control
     */
    virtual bool supportsPower() const { return true; }

    /**
     * @brief Whether this backend supports bandwidth control
     */
    virtual bool supportsBandwidth() const { return true; }

signals:
    void connected();
    void disconnected();
    void error(const QString& errorString);
    void frequencyChanged(double freqHz);
    void modeChanged(const QString& mode);
    // Emitted on every observed PTT state transition. Consumers (e.g., CW decoder)
    // gate on this signal; may lag physical radio by up to one backend poll interval.
    void pttStateChanged(bool active);
};

#endif // RIGINTERFACE_H
