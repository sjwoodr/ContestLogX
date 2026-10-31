/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "bandPlan.h"

QString BandPlan::freq2Mode(double freqMHz)
{
    // Convert frequency in MHz to mode based on band plan
    // This is a simplified version based on IARU Region 1/2 band plans
    
    // 160m (1.8-2.0 MHz)
    if (freqMHz >= 1.8 && freqMHz < 1.84) return "CW";
    if (freqMHz >= 1.84 && freqMHz < 2.0) return "LSB";
    
    // 80m (3.5-4.0 MHz)
    if (freqMHz >= 3.5 && freqMHz < 3.6) return "CW";
    if (freqMHz >= 3.6 && freqMHz < 4.0) return "LSB";
    
    // 60m (5.3-5.4 MHz)
    if (freqMHz >= 5.3 && freqMHz < 5.4) return "USB";
    
    // 40m (7.0-7.3 MHz)
    if (freqMHz >= 7.0 && freqMHz < 7.04) return "CW";
    if (freqMHz >= 7.04 && freqMHz < 7.125) return "RTTY";
    if (freqMHz >= 7.125 && freqMHz < 7.3) return "LSB";
    
    // 30m (10.1-10.15 MHz)
    if (freqMHz >= 10.1 && freqMHz < 10.15) return "CW";
    
    // 20m (14.0-14.35 MHz)
    if (freqMHz >= 14.0 && freqMHz < 14.07) return "CW";
    if (freqMHz >= 14.07 && freqMHz < 14.112) return "RTTY";
    if (freqMHz >= 14.112 && freqMHz < 14.35) return "USB";
    
    // 17m (18.068-18.168 MHz)
    if (freqMHz >= 18.068 && freqMHz < 18.095) return "CW";
    if (freqMHz >= 18.095 && freqMHz < 18.168) return "USB";
    
    // 15m (21.0-21.45 MHz)
    if (freqMHz >= 21.0 && freqMHz < 21.07) return "CW";
    if (freqMHz >= 21.07 && freqMHz < 21.149) return "RTTY";
    if (freqMHz >= 21.149 && freqMHz < 21.45) return "USB";
    
    // 12m (24.89-24.99 MHz)
    if (freqMHz >= 24.89 && freqMHz < 24.915) return "CW";
    if (freqMHz >= 24.915 && freqMHz < 24.99) return "USB";
    
    // 10m (28.0-29.7 MHz)
    if (freqMHz >= 28.0 && freqMHz < 28.07) return "CW";
    if (freqMHz >= 28.07 && freqMHz < 28.15) return "RTTY";
    if (freqMHz >= 28.15 && freqMHz < 29.7) return "USB";
    
    // 6m (50-54 MHz)
    if (freqMHz >= 50.0 && freqMHz < 50.1) return "CW";
    if (freqMHz >= 50.1 && freqMHz < 54.0) return "USB";
    
    // 2m (144-148 MHz)
    if (freqMHz >= 144.0 && freqMHz < 144.15) return "CW";
    if (freqMHz >= 144.15 && freqMHz < 148.0) return "USB";
    
    // Default to USB for unknown frequencies
    return "USB";
}

namespace {
struct BandEdges { const char *name; double startKhz; double endKhz; };

// Amateur band edges (ITU Region 2 where they differ), lowest first
const BandEdges kBands[] = {
    {"160m",   1800.0,     2000.0},
    {"80m",    3500.0,     4000.0},
    {"60m",    5300.0,     5400.0},
    {"40m",    7000.0,     7300.0},
    {"30m",    10100.0,    10150.0},
    {"20m",    14000.0,    14350.0},
    {"17m",    18068.0,    18168.0},
    {"15m",    21000.0,    21450.0},
    {"12m",    24890.0,    24990.0},
    {"10m",    28000.0,    29700.0},
    {"6m",     50000.0,    54000.0},
    {"4m",     70000.0,    71000.0},     // not common in US but included
    {"2m",     144000.0,   148000.0},
    {"1.25m",  222000.0,   225000.0},
    {"70cm",   420000.0,   450000.0},
    {"33cm",   902000.0,   928000.0},
    {"23cm",   1240000.0,  1300000.0},
    {"13cm",   2300000.0,  2450000.0},
    {"9cm",    3300000.0,  3500000.0},
    {"6cm",    5650000.0,  5925000.0},
    {"3cm",    10000000.0, 10500000.0},
    {"1.25cm", 24000000.0, 24250000.0},
    {"6mm",    47000000.0, 47200000.0},
};
} // namespace

QString BandPlan::freq2Band(double freqKhz)
{
    for (const BandEdges &b : kBands) {
        if (freqKhz >= b.startKhz && freqKhz < b.endKhz) return b.name;
    }
    return "";  // Unknown band
}

bool BandPlan::bandEdgesKhz(const QString &band, double &startKhz, double &endKhz)
{
    for (const BandEdges &b : kBands) {
        if (band == QLatin1String(b.name)) {
            startKhz = b.startKhz;
            endKhz   = b.endKhz;
            return true;
        }
    }
    return false;
}

QString BandPlan::freq2CabrilloBand(double freqKhz)
{
    double freqMHz = freqKhz / 1000.0;
    
    // For HF bands (below 50 MHz), return frequency in kHz as a string
    if (freqMHz < 50.0) {
        return QString::number(static_cast<int>(freqKhz));
    }
    
    // For VHF and UHF bands (50 MHz and above), return band shorthand
    // 6m (50-54 MHz)
    if (freqMHz >= 50.0 && freqMHz < 54.0) return "50";
    // 4m (70-71 MHz)
    if (freqMHz >= 70.0 && freqMHz < 71.0) return "70";
    // 2m (144-148 MHz)
    if (freqMHz >= 144.0 && freqMHz < 148.0) return "144";
    // 1.25m (222-225 MHz)
    if (freqMHz >= 222.0 && freqMHz < 225.0) return "222";
    // 70cm (420-450 MHz)
    if (freqMHz >= 420.0 && freqMHz < 450.0) return "432";
    // 33cm (902-928 MHz)
    if (freqMHz >= 902.0 && freqMHz < 928.0) return "902";
    // 23cm (1240-1300 MHz)
    if (freqMHz >= 1240.0 && freqMHz < 1300.0) return "1.2G";
    // 13cm (2300-2450 MHz)
    if (freqMHz >= 2300.0 && freqMHz < 2450.0) return "2.3G";
    // 9cm (3300-3500 MHz)
    if (freqMHz >= 3300.0 && freqMHz < 3500.0) return "3.4G";
    // 6cm (5650-5925 MHz)
    if (freqMHz >= 5650.0 && freqMHz < 5925.0) return "5.7G";
    // 3cm (10000-10500 MHz)
    if (freqMHz >= 10000.0 && freqMHz < 10500.0) return "10G";
    // 1.25cm (24000-24250 MHz)
    if (freqMHz >= 24000.0 && freqMHz < 24250.0) return "24G";
    // 6mm (47000-47200 MHz)
    if (freqMHz >= 47000.0 && freqMHz < 47200.0) return "47G";
    
    return "";  // Unknown band
}
