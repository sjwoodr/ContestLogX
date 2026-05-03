/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest/QtTest>
#include "../src/utils/callsignUtils.h"

class TestCallsignUtils : public QObject
{
    Q_OBJECT

private slots:
    void testGetCountryPrefix();
    void testGetCountry();
    void testIsSameCountry();
    void testIsSameContinent();
    void testWpxPrefixDocumentedExamples();
    void testWpxPrefixNoDigitCalls();
    void testWpxPrefixCallAreaChange();
    void testWpxPrefixPortableDesignator();
    void testWpxPrefixLicenseClassSuffixesIgnored();
    void testWpxPrefixEdgeCases();
    void testWpxPrefixDigitLedPrefixes();
    void testWpxPrefixThreeCharCountryPrefixes();
    void testWpxPrefixSpecialEventMultiDigit();
    void testWpxPrefixCallAreaChangeWithDigitLedBase();
    void testWpxPrefixCallAreaChangeWithMultiDigitBase();
    void testWpxPrefixDigitLedPortableDesignator();
    void testWpxPrefixStackedSuffixes();
    void testWpxPrefixUSLicenseClassIndicators();
    void testWpxPrefixMaritimeAeronautical();
    void testWpxPrefixMixedCaseAndPathological();
    void testWpxPrefixRealWorldCallsigns();
};

void TestCallsignUtils::testGetCountryPrefix()
{
    QString prefix = CallsignUtils::getCountryPrefix("W1AW");
    QVERIFY(!prefix.isEmpty());
    
    prefix = CallsignUtils::getCountryPrefix("N9OH");
    QVERIFY(!prefix.isEmpty());
}

void TestCallsignUtils::testGetCountry()
{
    QString country = CallsignUtils::getCountry("W1AW");
    QVERIFY(!country.isEmpty());
    
    country = CallsignUtils::getCountry("G3XYZ");
    QVERIFY(!country.isEmpty());
}

void TestCallsignUtils::testIsSameCountry()
{
    QVERIFY(CallsignUtils::isSameCountry("W1AW", "N9OH"));
    QVERIFY(CallsignUtils::isSameCountry("K1ABC", "W3DEF"));
}

void TestCallsignUtils::testIsSameContinent()
{
    QVERIFY(CallsignUtils::isSameContinent("W1AW", "VE3XYZ"));
    QVERIFY(CallsignUtils::isSameContinent("G3XYZ", "F5ABC"));
}

// CQ WPX Contest rule examples (verbatim from the published rules).
void TestCallsignUtils::testWpxPrefixDocumentedExamples()
{
    // "Examples: N8, W8, WD8, HG1, HG19, KC2, OE2, OE25, LY1000"
    QCOMPARE(CallsignUtils::extractWpxPrefix("N8XX"),     QStringLiteral("N8"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W8AB"),     QStringLiteral("W8"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("WD8ABC"),   QStringLiteral("WD8"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("HG1ABC"),   QStringLiteral("HG1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("HG19ABC"),  QStringLiteral("HG19"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("KC2DEF"),   QStringLiteral("KC2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("OE2XYZ"),   QStringLiteral("OE2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("OE25HG"),   QStringLiteral("OE25"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("LY1000ABC"),QStringLiteral("LY1000"));
}

// "Calls without numbers will be assigned a 0 after the first two letters."
void TestCallsignUtils::testWpxPrefixNoDigitCalls()
{
    // Rule example: XEFTJW → XE0
    QCOMPARE(CallsignUtils::extractWpxPrefix("XEFTJW"), QStringLiteral("XE0"));
}

// "/<digit>" is a call-area change — combine base letters with the new digit.
void TestCallsignUtils::testWpxPrefixCallAreaChange()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/4"),  QStringLiteral("W4"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("YB1AR/2"), QStringLiteral("YB2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("N8BJQ/3"), QStringLiteral("N3"));
    // Reverse order also accepted.
    QCOMPARE(CallsignUtils::extractWpxPrefix("4/W1AW"),  QStringLiteral("W4"));
}

// "In cases of portable operation, the portable designator will then become the prefix."
// Rule example: PA/N8BJQ → PA0
void TestCallsignUtils::testWpxPrefixPortableDesignator()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("PA/N8BJQ"),  QStringLiteral("PA0"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("N8BJQ/PA"),  QStringLiteral("PA0"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/KH6"),  QStringLiteral("KH6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("KH6/W1AW"),  QStringLiteral("KH6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("PJ2/N9OH"),  QStringLiteral("PJ2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("OE25/N9OH"), QStringLiteral("OE25"));
}

// "/A, /E, /J, /P or other license class identifiers do not count as prefixes."
void TestCallsignUtils::testWpxPrefixLicenseClassSuffixesIgnored()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("N9OH/P"),    QStringLiteral("N9"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/M"),    QStringLiteral("W1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/MM"),   QStringLiteral("W1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("DL1ABC/QRP"),QStringLiteral("DL1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("VK2XYZ/A"),  QStringLiteral("VK2"));
    // License-class suffix layered after a portable designator should still
    // resolve to the designator's prefix.
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/KH6/M"), QStringLiteral("KH6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("PA/N8BJQ/P"), QStringLiteral("PA0"));
}

void TestCallsignUtils::testWpxPrefixEdgeCases()
{
    // Empty / malformed input
    QCOMPARE(CallsignUtils::extractWpxPrefix(""),       QString());
    QCOMPARE(CallsignUtils::extractWpxPrefix("   "),    QString());
    QCOMPARE(CallsignUtils::extractWpxPrefix("123"),    QString());      // digits only
    // Lowercase input must work (WPX is case-insensitive)
    QCOMPARE(CallsignUtils::extractWpxPrefix("n9oh"),   QStringLiteral("N9"));
    // Whitespace gets trimmed
    QCOMPARE(CallsignUtils::extractWpxPrefix(" K1ABC "),QStringLiteral("K1"));
}

// Digit-led prefixes are very common in WPX (3D2 Fiji, 4U1 UN, 9A Croatia,
// 7Q7 Malawi, 8P9 Barbados, etc.). The extractor MUST handle these correctly
// — they are real DXCC entities and a contest entry without them would be a
// significant scoring error.
void TestCallsignUtils::testWpxPrefixDigitLedPrefixes()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("3D2RI"),   QStringLiteral("3D2"));   // Fiji
    QCOMPARE(CallsignUtils::extractWpxPrefix("4U1ITU"),  QStringLiteral("4U1"));   // UN/Vienna
    QCOMPARE(CallsignUtils::extractWpxPrefix("4U1UN"),   QStringLiteral("4U1"));   // UN/HQ
    QCOMPARE(CallsignUtils::extractWpxPrefix("9A1ABC"),  QStringLiteral("9A1"));   // Croatia
    QCOMPARE(CallsignUtils::extractWpxPrefix("9H1XX"),   QStringLiteral("9H1"));   // Malta
    QCOMPARE(CallsignUtils::extractWpxPrefix("9V1AB"),   QStringLiteral("9V1"));   // Singapore
    QCOMPARE(CallsignUtils::extractWpxPrefix("7Q7XX"),   QStringLiteral("7Q7"));   // Malawi
    QCOMPARE(CallsignUtils::extractWpxPrefix("8P9XY"),   QStringLiteral("8P9"));   // Barbados
    QCOMPARE(CallsignUtils::extractWpxPrefix("4X4XX"),   QStringLiteral("4X4"));   // Israel
    QCOMPARE(CallsignUtils::extractWpxPrefix("5B4XYZ"),  QStringLiteral("5B4"));   // Cyprus
    QCOMPARE(CallsignUtils::extractWpxPrefix("3B8XX"),   QStringLiteral("3B8"));   // Mauritius
    QCOMPARE(CallsignUtils::extractWpxPrefix("3W1XX"),   QStringLiteral("3W1"));   // Vietnam
    QCOMPARE(CallsignUtils::extractWpxPrefix("4O3XX"),   QStringLiteral("4O3"));   // Montenegro
}

// Three-character country prefix shapes (digit + 2 letters, or 2 letters + digit).
// "Any difference in the numbering, lettering, or order of same shall count as
// a separate prefix." So 3DA0 (Eswatini) and T88 (Palau) are valid distinct
// prefixes that include their numeric area as part of the prefix string.
void TestCallsignUtils::testWpxPrefixThreeCharCountryPrefixes()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("3DA0RS"),  QStringLiteral("3DA0"));  // Eswatini
    QCOMPARE(CallsignUtils::extractWpxPrefix("T88AB"),   QStringLiteral("T88"));   // Palau
    QCOMPARE(CallsignUtils::extractWpxPrefix("VK0EK"),   QStringLiteral("VK0"));   // Heard Is.
    QCOMPARE(CallsignUtils::extractWpxPrefix("ZL8X"),    QStringLiteral("ZL8"));   // Kermadec
    // VP2 is the WPX prefix for VP2EAA — even though VP2E (Anguilla),
    // VP2M (Montserrat) and VP2V (BVI) are different DXCC entities, they
    // all share the same WPX prefix per the rule "letters + leading digit
    // run". WPX is callsign-prefix-based, not DXCC-entity-based.
    QCOMPARE(CallsignUtils::extractWpxPrefix("VP2EAA"),  QStringLiteral("VP2"));   // Anguilla
    QCOMPARE(CallsignUtils::extractWpxPrefix("VP2MGB"),  QStringLiteral("VP2"));   // Montserrat (same prefix!)
    QCOMPARE(CallsignUtils::extractWpxPrefix("VP8AB"),   QStringLiteral("VP8"));   // Falkland
    QCOMPARE(CallsignUtils::extractWpxPrefix("KH8XX"),   QStringLiteral("KH8"));   // Am. Samoa
    QCOMPARE(CallsignUtils::extractWpxPrefix("KP2A"),    QStringLiteral("KP2"));   // US Virgin Is.
    QCOMPARE(CallsignUtils::extractWpxPrefix("V31AB"),   QStringLiteral("V31"));   // Belize
    QCOMPARE(CallsignUtils::extractWpxPrefix("FK8XYZ"),  QStringLiteral("FK8"));   // New Caledonia
}

// Multi-digit numeric area = special-event / commemorative prefix. All-digit
// runs after the letters are kept as part of the prefix per WPX rules.
void TestCallsignUtils::testWpxPrefixSpecialEventMultiDigit()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("HG19ABC"),  QStringLiteral("HG19"));   // rules example
    QCOMPARE(CallsignUtils::extractWpxPrefix("OE25HG"),   QStringLiteral("OE25"));   // rules example
    QCOMPARE(CallsignUtils::extractWpxPrefix("LY1000A"),  QStringLiteral("LY1000")); // rules example
    QCOMPARE(CallsignUtils::extractWpxPrefix("HG75A"),    QStringLiteral("HG75"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("OE100ABC"), QStringLiteral("OE100"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("DA20XYZ"),  QStringLiteral("DA20"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("YO150ABC"), QStringLiteral("YO150"));
}

// /<digit> against a digit-led base call must combine the base prefix's
// alpha stem (after stripping its trailing digits) with the new area digit.
// 3D2RI/4 → strip "2" off "3D2" → "3D" + "4" = "3D4"
void TestCallsignUtils::testWpxPrefixCallAreaChangeWithDigitLedBase()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("3D2RI/4"),  QStringLiteral("3D4"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("9A1ABC/2"), QStringLiteral("9A2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("7Q7XX/3"),  QStringLiteral("7Q3"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("4X4XYZ/6"), QStringLiteral("4X6"));
}

// Multi-digit areas — /<digit> replaces the entire trailing-digit run with
// the new single digit.
void TestCallsignUtils::testWpxPrefixCallAreaChangeWithMultiDigitBase()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("HG19ABC/8"),  QStringLiteral("HG8"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("OE25HG/4"),   QStringLiteral("OE4"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("LY1000A/3"),  QStringLiteral("LY3"));
}

// Portable designator that itself starts with a digit (Fiji visiting a
// foreign call, etc.). Both sides exercised — designator shape detection
// must work regardless of which side it's on.
void TestCallsignUtils::testWpxPrefixDigitLedPortableDesignator()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/3D2"),  QStringLiteral("3D2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("3D2/K1AAA"),  QStringLiteral("3D2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/9A1"),   QStringLiteral("9A1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("9A1/W1AW"),   QStringLiteral("9A1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("DL1ABC/4U1"), QStringLiteral("4U1"));
    // 4X is Israel's authorized country prefix — it has a digit (the '4')
    // so the WPX 0-padding rule does NOT apply. Designator becomes the
    // prefix verbatim.
    QCOMPARE(CallsignUtils::extractWpxPrefix("4X/G3PQR"),   QStringLiteral("4X"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("9A/DL1ABC"),  QStringLiteral("9A"));
    // Pure-alpha designator → 0-padded.
    QCOMPARE(CallsignUtils::extractWpxPrefix("HB/W1AW"),    QStringLiteral("HB0")); // Liechtenstein, no digit
}

// Stacked / layered suffixes: license-class indicator + portable designator
// + license-class indicator again. Strip license suffixes off the right edge
// repeatedly until none remain, then resolve the designator.
void TestCallsignUtils::testWpxPrefixStackedSuffixes()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/KH6/M"),    QStringLiteral("KH6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("PA/N8BJQ/P"),    QStringLiteral("PA0"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/4/MM"),     QStringLiteral("W4"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("DL1ABC/HB9/QRP"),QStringLiteral("HB9"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("VK2XYZ/VK6/M"),  QStringLiteral("VK6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("3D2RI/4/QRP"),   QStringLiteral("3D4"));
}

// US license-class identifiers (/AE, /AG, /T, /N) and the QRP power-class
// marker — all must NOT count as prefixes per "or other license class
// identifiers" in the WPX rules.
void TestCallsignUtils::testWpxPrefixUSLicenseClassIndicators()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/AE"),  QStringLiteral("K1"));   // Amateur Extra
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/AG"),  QStringLiteral("K1"));   // General
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/T"),   QStringLiteral("K1"));   // Technician
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/N"),   QStringLiteral("K1"));   // Novice
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/QRP"), QStringLiteral("K1"));   // power class
}

// Maritime mobile (/MM), aeronautical mobile (/AM), and basic mobile (/M)
// are all explicitly stripped per WPX rule.
void TestCallsignUtils::testWpxPrefixMaritimeAeronautical()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/MM"),    QStringLiteral("K1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/AM"),    QStringLiteral("K1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1AAA/M"),     QStringLiteral("K1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("DL1ABC/MM"),   QStringLiteral("DL1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("VK2XYZ/M"),    QStringLiteral("VK2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("3D2RI/MM"),    QStringLiteral("3D2"));
}

// Mixed case + pathological inputs.
void TestCallsignUtils::testWpxPrefixMixedCaseAndPathological()
{
    QCOMPARE(CallsignUtils::extractWpxPrefix("k1aaA"),   QStringLiteral("K1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("Pa/N8bjq"),QStringLiteral("PA0"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("W1AW/4"),  QStringLiteral("W4"));
    // Calls with non-alphanumeric chars are rejected
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1@BC"),   QString());
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1-BC"),   QString());
    QCOMPARE(CallsignUtils::extractWpxPrefix("/K1ABC"),  QStringLiteral("K1"));   // leading slash safe
    QCOMPARE(CallsignUtils::extractWpxPrefix("K1ABC/"),  QStringLiteral("K1"));   // trailing slash safe
}

// Real-world callsigns from major DX entities — sanity-check that nothing
// surprising happens on the calls a competitive entry will actually log.
void TestCallsignUtils::testWpxPrefixRealWorldCallsigns()
{
    // North America
    QCOMPARE(CallsignUtils::extractWpxPrefix("K3LR"),    QStringLiteral("K3"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("KC1XX"),   QStringLiteral("KC1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("AA1K"),    QStringLiteral("AA1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("VE3EJ"),   QStringLiteral("VE3"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("VY2ZM"),   QStringLiteral("VY2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("XE2B"),    QStringLiteral("XE2"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("CO8LY"),   QStringLiteral("CO8"));   // Cuba
    QCOMPARE(CallsignUtils::extractWpxPrefix("HK1NA"),   QStringLiteral("HK1"));   // Colombia
    // South America
    QCOMPARE(CallsignUtils::extractWpxPrefix("PY1KN"),   QStringLiteral("PY1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("LU5DX"),   QStringLiteral("LU5"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("CE3CT"),   QStringLiteral("CE3"));
    // Europe
    QCOMPARE(CallsignUtils::extractWpxPrefix("DL5XX"),   QStringLiteral("DL5"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("F6BEE"),   QStringLiteral("F6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("G3SXW"),   QStringLiteral("G3"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("EA8AH"),   QStringLiteral("EA8"));   // Canary Is.
    QCOMPARE(CallsignUtils::extractWpxPrefix("OH8X"),    QStringLiteral("OH8"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("RU1A"),    QStringLiteral("RU1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("HA1AG"),   QStringLiteral("HA1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("OK1XX"),   QStringLiteral("OK1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("S52ZW"),   QStringLiteral("S52"));   // Slovenia
    QCOMPARE(CallsignUtils::extractWpxPrefix("TF3CW"),   QStringLiteral("TF3"));   // Iceland
    QCOMPARE(CallsignUtils::extractWpxPrefix("UA9AYA"),  QStringLiteral("UA9"));
    // Asia / Oceania
    QCOMPARE(CallsignUtils::extractWpxPrefix("JA1NUT"),  QStringLiteral("JA1"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("BV2A"),    QStringLiteral("BV2"));   // Taiwan
    QCOMPARE(CallsignUtils::extractWpxPrefix("BY1QH"),   QStringLiteral("BY1"));   // China
    QCOMPARE(CallsignUtils::extractWpxPrefix("HL5IVL"),  QStringLiteral("HL5"));   // S. Korea
    QCOMPARE(CallsignUtils::extractWpxPrefix("VR2KW"),   QStringLiteral("VR2"));   // Hong Kong
    QCOMPARE(CallsignUtils::extractWpxPrefix("YB0AZ"),   QStringLiteral("YB0"));   // Indonesia
    QCOMPARE(CallsignUtils::extractWpxPrefix("VK4EMM"),  QStringLiteral("VK4"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("ZL3X"),    QStringLiteral("ZL3"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("KH7XS"),   QStringLiteral("KH7"));
    // Africa
    QCOMPARE(CallsignUtils::extractWpxPrefix("ZS6KR"),   QStringLiteral("ZS6"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("CN8KD"),   QStringLiteral("CN8"));
    QCOMPARE(CallsignUtils::extractWpxPrefix("EA9KB"),   QStringLiteral("EA9"));   // Ceuta/Melilla
}

QTEST_MAIN(TestCallsignUtils)
#include "test_callsignutils.moc"
