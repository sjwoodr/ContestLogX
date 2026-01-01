/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "bandplan.h"

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

QString BandPlan::freq2Band(double freqKhz)
{
    double freqMHz = freqKhz / 1000.0;
    
    // 160m (1.8-2.0 MHz)
    if (freqMHz >= 1.8 && freqMHz < 2.0) return "160m";
    // 80m (3.5-4.0 MHz)
    if (freqMHz >= 3.5 && freqMHz < 4.0) return "80m";
    // 60m (5.3-5.4 MHz)
    if (freqMHz >= 5.3 && freqMHz < 5.4) return "60m";
    // 40m (7.0-7.3 MHz)
    if (freqMHz >= 7.0 && freqMHz < 7.3) return "40m";
    // 30m (10.1-10.15 MHz)
    if (freqMHz >= 10.1 && freqMHz < 10.15) return "30m";
    // 20m (14.0-14.35 MHz)
    if (freqMHz >= 14.0 && freqMHz < 14.35) return "20m";
    // 17m (18.068-18.168 MHz)
    if (freqMHz >= 18.068 && freqMHz < 18.168) return "17m";
    // 15m (21.0-21.45 MHz)
    if (freqMHz >= 21.0 && freqMHz < 21.45) return "15m";
    // 12m (24.89-24.99 MHz)
    if (freqMHz >= 24.89 && freqMHz < 24.99) return "12m";
    // 10m (28.0-29.7 MHz)
    if (freqMHz >= 28.0 && freqMHz < 29.7) return "10m";
    // 6m (50-54 MHz)
    if (freqMHz >= 50.0 && freqMHz < 54.0) return "6m";
    // 4m (70-71 MHz) - not common in US but included
    if (freqMHz >= 70.0 && freqMHz < 71.0) return "4m";
    // 2m (144-148 MHz)
    if (freqMHz >= 144.0 && freqMHz < 148.0) return "2m";
    // 1.25m (222-225 MHz)
    if (freqMHz >= 222.0 && freqMHz < 225.0) return "1.25m";
    // 70cm (420-450 MHz)
    if (freqMHz >= 420.0 && freqMHz < 450.0) return "70cm";
    // 33cm (902-928 MHz)
    if (freqMHz >= 902.0 && freqMHz < 928.0) return "33cm";
    // 23cm (1240-1300 MHz)
    if (freqMHz >= 1240.0 && freqMHz < 1300.0) return "23cm";
    
    return "";  // Unknown band
}
