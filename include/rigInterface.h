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

#include "cwKeyerInterface.h"

/**
 * @brief Abstract base class for rig control interfaces
 *
 * Provides a common interface for different rig control backends
 * (flrig XML-RPC, Hamlib rigctld, etc.)
 *
 * Derives from CwKeyerInterface: every rig backend is also a CW keyer (it keys
 * via cwio / send_morse). The CW-keying methods (isConnected, sendCW, stopCW,
 * get/setCWSpeed, supportsCW) are declared there.
 */
class RigInterface : public QObject, public CwKeyerInterface
{
    Q_OBJECT

public:
    explicit RigInterface(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~RigInterface() = default;

    virtual bool connectToRig(const QString& host, int port) = 0;
    virtual void disconnectFromRig() = 0;
    // isConnected() is inherited from CwKeyerInterface.

    // Frequency and mode
    virtual double getFrequency() = 0;
    virtual bool setFrequency(double freqHz) = 0;
    virtual QString getMode() = 0;
    virtual bool setMode(const QString& mode) = 0;

    // CW keying (sendCW/stopCW/get+setCWSpeed) and supportsCW() are inherited
    // from CwKeyerInterface.

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

    // supportsCW() is inherited from CwKeyerInterface.

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
