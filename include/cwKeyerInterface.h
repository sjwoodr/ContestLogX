/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CWKEYERINTERFACE_H
#define CWKEYERINTERFACE_H

#include <QString>

/**
 * @brief Abstract interface for anything that can key CW.
 *
 * This is the CW-keying subset shared by rig backends (which key via flrig
 * cwio or hamlib send_morse) and dedicated keyers (e.g. a serial K1EL
 * WinKeyer). It is deliberately NOT a QObject: RigInterface already derives
 * from QObject and a class may inherit QObject only once. Concrete keyers that
 * need signals (connected/error/busy) declare them on their own
 * QObject-derived class.
 *
 * RigInterface derives from this, so the existing rig backends satisfy it for
 * free and "key via the rig" remains the default, unchanged path. A future
 * WinKeyerClient implements this on its own serial port, independent of the
 * CAT backend.
 */
class CwKeyerInterface
{
public:
    virtual ~CwKeyerInterface() = default;

    virtual bool isConnected() const = 0;

    // CW keying
    virtual bool sendCW(const QString& text) = 0;
    virtual bool stopCW() = 0;
    virtual int getCWSpeed() = 0;
    virtual bool setCWSpeed(int wpm) = 0;

    /**
     * @brief Whether this keyer can actually key CW.
     *
     * flrig supports full CW keying via cwio; Hamlib rigctld supports
     * send_morse but with limited speed control; a WinKeyer always can.
     */
    virtual bool supportsCW() const { return true; }
};

#endif // CWKEYERINTERFACE_H
