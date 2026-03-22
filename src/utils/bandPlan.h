/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef BANDPLAN_H
#define BANDPLAN_H

#include <QString>

class BandPlan
{
public:
    static QString freq2Mode(double freqMHz);
    static QString freq2Band(double freqKhz);
    static QString freq2CabrilloBand(double freqKhz);
};

#endif // BANDPLAN_H
