/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "callsignUtils.h"
#include <QRegularExpression>
#include <QSet>

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

namespace {

// CQ WPX rule: "A PREFIX is the letter/numeral combination which forms the
// first part of the amateur call." Operationally: the prefix is the call
// up through the LAST digit, with the trailing alpha suffix dropped. If the
// call has no digits at all, append "0" after the first two letters.
//
// This last-digit-anchored definition handles every shape of legal prefix:
//   N9OH       → N9         (1 letter + 1 digit + alpha suffix)
//   WD8ABC     → WD8        (2 letters + 1 digit + alpha suffix)
//   HG19ABC    → HG19       (multi-digit area, e.g. Hungarian special)
//   LY1000ABC  → LY1000     (4-digit special-event area)
//   OE25HG     → OE25       (Austrian commemorative)
//   3D2RI      → 3D2        (digit-led: Fiji)
//   4U1ITU     → 4U1        (digit-led: UN, Vienna)
//   9A1ABC     → 9A1        (digit-led: Croatia)
//   7Q7XX      → 7Q7        (digit-led: Malawi)
//   T88AB      → T88        (digit-suffixed prefix, Palau)
QString computeWpxPrefixCore(const QString& call)
{
    if (call.isEmpty())
        return QString();

    // Reject anything that contains characters other than A-Z and 0-9, or that
    // is pure digits (a real callsign must contain at least one letter).
    static const QRegularExpression validChars(QStringLiteral("^[A-Z0-9]+$"));
    static const QRegularExpression hasLetter(QStringLiteral("[A-Z]"));
    if (!validChars.match(call).hasMatch() || !hasLetter.match(call).hasMatch())
        return QString();

    // Find the last digit. If there is one, the prefix is the call up through
    // and including it (everything after is the operator-issued alpha suffix).
    for (int i = call.length() - 1; i >= 0; --i) {
        if (call[i].isDigit())
            return call.left(i + 1);
    }

    // No digits anywhere - apply the WPX "append 0 after second letter" rule.
    // Examples from the rules: XEFTJW → XE0, PA (designator only) → PA0.
    if (call.length() >= 2)
        return call.left(2) + QStringLiteral("0");
    return call + QStringLiteral("0");
}

// Compute the WPX prefix for a string we KNOW is a portable designator
// (slash notation already resolved to a single side). The WPX rule says
// "the portable designator will then become the prefix" - so if the
// designator already contains any digit, the designator string IS the
// prefix verbatim (e.g. KH6, OE25, 3D2, 4U1, 4X). The 0-padding rule only
// fires for designators with NO digits at all (PA → PA0, XE → XE0).
QString computeWpxPrefixForDesignator(const QString& d)
{
    if (d.isEmpty())
        return QString();

    static const QRegularExpression validChars(QStringLiteral("^[A-Z0-9]+$"));
    static const QRegularExpression hasLetter(QStringLiteral("[A-Z]"));
    if (!validChars.match(d).hasMatch() || !hasLetter.match(d).hasMatch())
        return QString();

    static const QRegularExpression hasDigit(QStringLiteral("[0-9]"));
    if (hasDigit.match(d).hasMatch())
        return d;                         // designator already has area digit

    // Pure-alpha designator - apply WPX 0-padding rule.
    if (d.length() >= 2)
        return d.left(2) + QStringLiteral("0");
    return d + QStringLiteral("0");
}

} // anonymous namespace

QString CallsignUtils::extractWpxPrefix(const QString& callsign)
{
    QString call = callsign.toUpper().trimmed();
    if (call.isEmpty())
        return QString();

    // Step 1: strip trailing license-class / non-prefix-affecting portable suffixes.
    // WPX rule (verbatim): "Maritime mobile, mobile, /A, /E, /J, /P, or other
    // license class identifiers do not count as prefixes." We strip the four
    // explicit identifiers, both maritime forms (/MM, /AM aeronautical mobile,
    // /M mobile), and the common "other" license-class indicators seen in real
    // logs: US Amateur Extra (/AE), US General (/AG), Technician (/T), Novice
    // (/N), and the QRP power-class marker (/QRP). Stripping is repeated so
    // stacked suffixes like W1AW/KH6/M peel down to W1AW/KH6 first, then the
    // designator logic resolves to KH6.
    static const QSet<QString> stripSuffixes = {
        QStringLiteral("MM"), QStringLiteral("AM"), QStringLiteral("M"),
        QStringLiteral("AE"), QStringLiteral("AG"), QStringLiteral("A"),
        QStringLiteral("E"),  QStringLiteral("J"),  QStringLiteral("P"),
        QStringLiteral("T"),  QStringLiteral("N"),  QStringLiteral("QRP")
    };

    while (call.contains('/')) {
        const int slashPos = call.lastIndexOf('/');
        const QString tail = call.mid(slashPos + 1);
        if (stripSuffixes.contains(tail)) {
            call = call.left(slashPos);
            continue;
        }
        break;
    }

    // Trim leading and trailing slashes (typos like "/K1ABC" or "K1ABC/" -
    // be lenient since these are operator entry mistakes, not WPX-relevant).
    while (call.startsWith('/')) call.remove(0, 1);
    while (call.endsWith('/'))   call.chop(1);

    if (call.isEmpty())
        return QString();

    // Step 2: handle remaining slash (single portable designator).
    if (call.contains('/')) {
        const int slashPos = call.indexOf('/');
        QString left  = call.left(slashPos);
        QString right = call.mid(slashPos + 1);

        // If multi-slash remains (rare/illegal), just take the first two parts.
        const int extraSlash = right.indexOf('/');
        if (extraSlash >= 0)
            right = right.left(extraSlash);

        if (left.isEmpty() || right.isEmpty())
            return QString();

        // Case A: one side is purely digits - call-area change. Replace the
        // base call's prefix-digit run with the new digit. Examples:
        //   W1AW/4    → W4    (W1 → W4)
        //   HG19ABC/8 → HG8   (HG19 → HG8)
        //   3D2RI/4   → 3D4   (3D2 → 3D4) - digit-led prefix supported
        //   4/W1AW    → W4    (designator on the left side)
        static const QRegularExpression digitsOnly(QStringLiteral("^[0-9]+$"));

        auto combineWithDigit = [&](const QString& base, const QString& digit) -> QString {
            QString basePrefix = computeWpxPrefixCore(base);
            if (basePrefix.isEmpty())
                return QString();
            // Strip the trailing digit run to get the letter "stem" of the
            // prefix, then append the new call-area digit.
            int stemEnd = basePrefix.length();
            while (stemEnd > 0 && basePrefix[stemEnd - 1].isDigit())
                --stemEnd;
            return basePrefix.left(stemEnd) + digit;
        };

        if (digitsOnly.match(right).hasMatch())
            return combineWithDigit(left, right);
        if (digitsOnly.match(left).hasMatch())
            return combineWithDigit(right, left);

        // Case B: pick the portable designator. A real base callsign has both
        //   (a) at least one alpha character before any digit, and
        //   (b) at least one alpha character after the last digit
        // (e.g. K1AAA, 9A1ABC, OE25HG). A designator either ends in a digit
        // (KH6, OE25, 3D2, 4U1) or has no digits at all (PA, XE), or is a
        // short letter-then-digit form (4X, 9A) that's too short to be a
        // base call. Pick by: end-in-digit > shorter length > left fallback.
        auto endsInDigit = [](const QString& s) {
            return !s.isEmpty() && s.back().isDigit();
        };
        auto looksLikeBaseCall = [](const QString& s) {
            // Has alpha after a digit AND length >= 4 (rules out short forms
            // like "4X" which is digit-letter and could be a designator).
            int lastDigit = -1;
            for (int i = s.length() - 1; i >= 0; --i) {
                if (s[i].isDigit()) { lastDigit = i; break; }
            }
            if (lastDigit < 0) return false;
            const bool hasTrailingAlpha = lastDigit < s.length() - 1;
            return hasTrailingAlpha && s.length() >= 4;
        };

        QString designator;
        if (endsInDigit(left) && !endsInDigit(right))
            designator = left;            // PJ2/N9OH → PJ2
        else if (endsInDigit(right) && !endsInDigit(left))
            designator = right;           // W1AW/KH6 → KH6, K1AAA/3D2 → 3D2
        else if (looksLikeBaseCall(left) && !looksLikeBaseCall(right))
            designator = right;           // K1AAA/4X → 4X, PA/N8BJQ → PA
        else if (looksLikeBaseCall(right) && !looksLikeBaseCall(left))
            designator = left;            // 4X/G3PQR → 4X, PA/N8BJQ → PA
        else if (!left.isEmpty() && !right.isEmpty())
            // Fall through: prefer the shorter side as the designator.
            designator = (left.length() <= right.length()) ? left : right;

        return computeWpxPrefixForDesignator(designator);
    }

    // Step 3: no slash - extract prefix directly from the call.
    return computeWpxPrefixCore(call);
}
