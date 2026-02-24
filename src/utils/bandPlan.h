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
