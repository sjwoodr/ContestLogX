/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "callsignUtils.h"
#include <QRegularExpression>

QString CallsignUtils::getCountryPrefix(const QString& callsign)
{
    if (callsign.isEmpty()) {
        return QString();
    }
    
    QString call = callsign.toUpper();
    
    // Remove portable indicators
    QStringList parts = call.split('/');
    QString mainCall = call;
    
    // If call has /, try to find the main part
    if (parts.size() > 1) {
        // Usually the longest part is the main callsign
        for (const QString& part : parts) {
            if (part.length() > mainCall.length()) {
                mainCall = part;
            }
        }
    }
    
    // Extract prefix (letters + first digit)
    QRegularExpression re("^([A-Z0-9]+?\\d)");
    QRegularExpressionMatch match = re.match(mainCall);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    // Fallback: first 1-2 characters
    if (mainCall.length() >= 2) {
        return mainCall.left(2);
    }
    
    return mainCall.left(1);
}

QString CallsignUtils::getCountry(const QString& callsign)
{
    QString prefix = getCountryPrefix(callsign);
    
    // Very simplified mapping - just major countries for now
    // W/K/N/AA-AL = USA
    if (prefix.startsWith('W') || prefix.startsWith('K') || prefix.startsWith('N') ||
        (prefix.length() == 2 && prefix[0] == 'A' && prefix[1] >= 'A' && prefix[1] <= 'L')) {
        return "USA";
    }
    
    // VE/VA/VO/VY = Canada
    if (prefix.startsWith("VE") || prefix.startsWith("VA") || 
        prefix.startsWith("VO") || prefix.startsWith("VY")) {
        return "Canada";
    }
    
    // XE/XF = Mexico
    if (prefix.startsWith("XE") || prefix.startsWith("XF")) {
        return "Mexico";
    }
    
    // G/M = UK
    if (prefix.startsWith('G') || prefix.startsWith('M')) {
        return "UK";
    }
    
    // DL/DA-DR = Germany
    if (prefix.startsWith("DL") || prefix.startsWith("DA") || 
        prefix.startsWith("DB") || prefix.startsWith("DC") ||
        prefix.startsWith("DD") || prefix.startsWith("DE") ||
        prefix.startsWith("DF") || prefix.startsWith("DG") ||
        prefix.startsWith("DH") || prefix.startsWith("DJ") ||
        prefix.startsWith("DK") || prefix.startsWith("DM") ||
        prefix.startsWith("DN") || prefix.startsWith("DO") ||
        prefix.startsWith("DP") || prefix.startsWith("DQ") ||
        prefix.startsWith("DR")) {
        return "Germany";
    }
    
    // F = France
    if (prefix.startsWith('F')) {
        return "France";
    }
    
    // JA-JS = Japan
    if (prefix.startsWith('J') && prefix.length() >= 2 && 
        prefix[1] >= 'A' && prefix[1] <= 'S') {
        return "Japan";
    }
    
    // VK = Australia
    if (prefix.startsWith("VK")) {
        return "Australia";
    }
    
    // ZL/ZM = New Zealand
    if (prefix.startsWith("ZL") || prefix.startsWith("ZM")) {
        return "New Zealand";
    }
    
    // Default: unknown (treat as DX)
    return "DX";
}

QString CallsignUtils::getContinent(const QString& callsign)
{
    QString country = getCountry(callsign);
    
    // North America
    if (country == "USA" || country == "Canada" || country == "Mexico") {
        return "NA";
    }
    
    // Europe
    if (country == "UK" || country == "Germany" || country == "France") {
        return "EU";
    }
    
    // Asia
    if (country == "Japan") {
        return "AS";
    }
    
    // Oceania
    if (country == "Australia" || country == "New Zealand") {
        return "OC";
    }
    
    // Default
    return "XX";
}

bool CallsignUtils::isSameCountry(const QString& call1, const QString& call2)
{
    return getCountry(call1) == getCountry(call2);
}

bool CallsignUtils::isSameContinent(const QString& call1, const QString& call2)
{
    return getContinent(call1) == getContinent(call2);
}
