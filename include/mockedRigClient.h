/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef MOCKEDRIGCLIENT_H
#define MOCKEDRIGCLIENT_H

#include "rigInterface.h"

/**
 * @brief Mocked rig client for testing and practice
 *
 * Simulates a CAT-controlled radio without requiring real hardware.
 * Useful for SO2R practice, UI testing, and configuration setup
 * when a second radio is not available.
 */
class MockedRigClient : public RigInterface
{
    Q_OBJECT

public:
    explicit MockedRigClient(QObject *parent = nullptr);
    ~MockedRigClient() = default;

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

    // Emit the current PTT state as a pttStateChanged signal. Used once after
    // construction so consumers who subscribe late still receive an initial value.
    void emitInitialPttState();

    bool supportsCW() const override { return true; }
    bool supportsPTT() const override { return true; }
    bool supportsPower() const override { return true; }
    bool supportsBandwidth() const override { return true; }

private:
    bool m_connected;
    double m_frequency;
    QString m_mode;
    int m_cwSpeed;
    bool m_ptt;
    int m_power;
    int m_bandwidth;
    QString m_vfo;
};

#endif // MOCKEDRIGCLIENT_H
