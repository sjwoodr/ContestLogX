/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "mockedRigClient.h"
#include "debugLogger.h"

MockedRigClient::MockedRigClient(QObject *parent)
    : RigInterface(parent)
    , m_connected(false)
    , m_frequency(14200000.0)  // 14.200 MHz
    , m_mode("USB")
    , m_cwSpeed(25)
    , m_ptt(false)
    , m_power(100)
    , m_bandwidth(2400)
    , m_vfo("VFOA")
{
}

void MockedRigClient::emitInitialPttState()
{
    emit pttStateChanged(m_ptt);
}

bool MockedRigClient::connectToRig(const QString& host, int port)
{
    Q_UNUSED(host);
    Q_UNUSED(port);

    DebugLogger::instance().log("RigClient", "Connecting to mocked rig");
    m_connected = true;
    DebugLogger::instance().log("RigClient", "Connected to mocked rig");
    emit connected();
    return true;
}

void MockedRigClient::disconnectFromRig()
{
    DebugLogger::instance().log("RigClient", "Disconnected from mocked rig");
    m_connected = false;
    emit disconnected();
}

bool MockedRigClient::isConnected() const
{
    return m_connected;
}

double MockedRigClient::getFrequency()
{
    return m_frequency;
}

bool MockedRigClient::setFrequency(double freqHz)
{
    m_frequency = freqHz;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set frequency: %1 Hz").arg(freqHz, 0, 'f', 0));
    return true;
}

QString MockedRigClient::getMode()
{
    return m_mode;
}

bool MockedRigClient::setMode(const QString& mode)
{
    m_mode = mode;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set mode: %1").arg(mode));
    return true;
}

bool MockedRigClient::sendCW(const QString& text)
{
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("CW send: %1").arg(text));
    return true;
}

bool MockedRigClient::stopCW()
{
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("CW stop"));
    return true;
}

int MockedRigClient::getCWSpeed()
{
    return m_cwSpeed;
}

bool MockedRigClient::setCWSpeed(int wpm)
{
    m_cwSpeed = wpm;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set CW speed: %1 WPM").arg(wpm));
    return true;
}

bool MockedRigClient::getPTT()
{
    return m_ptt;
}

bool MockedRigClient::setPTT(bool enable)
{
    bool changed = (m_ptt != enable);
    m_ptt = enable;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set PTT: %1").arg(enable ? "ON" : "OFF"));
    if (changed)
        emit pttStateChanged(m_ptt);
    return true;
}

int MockedRigClient::getPower()
{
    return m_power;
}

bool MockedRigClient::setPower(int watts)
{
    m_power = watts;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set power: %1W").arg(watts));
    return true;
}

int MockedRigClient::getBandwidth()
{
    return m_bandwidth;
}

bool MockedRigClient::setBandwidth(int hz)
{
    m_bandwidth = hz;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set bandwidth: %1 Hz").arg(hz));
    return true;
}

QString MockedRigClient::getVFO()
{
    return m_vfo;
}

bool MockedRigClient::setVFO(const QString& vfo)
{
    m_vfo = vfo;
    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("MockedRig", QString("Set VFO: %1").arg(vfo));
    return true;
}

QString MockedRigClient::getRigName()
{
    return "Mocked Rig";
}
