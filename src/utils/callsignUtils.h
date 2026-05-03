/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CALLSIGNUTILS_H
#define CALLSIGNUTILS_H

#include <QString>

class CallsignUtils
{
public:
    // Extract the country prefix from a callsign
    static QString getCountryPrefix(const QString& callsign);
    
    // Get the country name (simplified - major countries only)
    static QString getCountry(const QString& callsign);
    
    // Get the continent code (NA, EU, AS, OC, SA, AF)
    static QString getContinent(const QString& callsign);
    
    // Check if two callsigns are from the same country
    static bool isSameCountry(const QString& call1, const QString& call2);
    
    // Check if two callsigns are from the same continent
    static bool isSameContinent(const QString& call1, const QString& call2);

    // Extract the CQ WPX prefix from a callsign per official WPX rules.
    // The prefix is the leading letters + leading digits of the active call;
    // license-class/portable suffixes (/M, /MM, /A, /E, /J, /P, /AM, /AE, /AG, /QRP)
    // are stripped, slash-notation portable designators become the prefix
    // (e.g. PA/N8BJQ → PA0, W1AW/4 → W4, W1AW/KH6 → KH6), and calls without
    // numbers get a "0" appended after the second letter (XEFTJW → XE0, PA → PA0).
    // Returns an empty string if the call cannot be parsed.
    static QString extractWpxPrefix(const QString& callsign);
};

#endif // CALLSIGNUTILS_H
