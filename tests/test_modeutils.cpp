/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest/QtTest>
#include "../src/utils/modeUtils.h"
#include "../include/qsoRecord.h"

class TestModeUtils : public QObject
{
    Q_OBJECT

private slots:
    void testCwSidebandVariants();
    void testRttyVariants();
    void testDataOverSsbVariants();
    void testGenericDigitalSpellings();
    void testVoiceVariants();
    void testCanonicalModesUnchanged();
    void testUnknownModesPassThrough();
    void testIsDigital();
    void testQsoRecordNormalizesOnSet();
};

// The reported bug: a Yaesu FT-710 reports CW as "CW-U" (or "CW-L"), which
// CLX rejected as an invalid mode in a CW/SSB contest.
void TestModeUtils::testCwSidebandVariants()
{
    QCOMPARE(ModeUtils::normalizeMode("CW-U"), QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode("CW-L"), QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode("CWU"),  QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode("CWL"),  QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode("CW-R"), QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode("CWR"),  QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode("CW-N"), QString("CW"));

    // Case and stray whitespace from a CAT response must not matter
    QCOMPARE(ModeUtils::normalizeMode("cw-u"),   QString("CW"));
    QCOMPARE(ModeUtils::normalizeMode(" CW-U "), QString("CW"));
}

void TestModeUtils::testRttyVariants()
{
    QCOMPARE(ModeUtils::normalizeMode("RTTY-L"), QString("RTTY"));
    QCOMPARE(ModeUtils::normalizeMode("RTTY-U"), QString("RTTY"));
    QCOMPARE(ModeUtils::normalizeMode("RTTYR"),  QString("RTTY"));
    QCOMPARE(ModeUtils::normalizeMode("FSK"),    QString("RTTY"));
}

void TestModeUtils::testDataOverSsbVariants()
{
    QCOMPARE(ModeUtils::normalizeMode("DATA-U"), QString("USB-D"));
    QCOMPARE(ModeUtils::normalizeMode("PKTUSB"), QString("USB-D"));
    QCOMPARE(ModeUtils::normalizeMode("DIGU"),   QString("USB-D"));
    QCOMPARE(ModeUtils::normalizeMode("DATA-L"), QString("LSB-D"));
    QCOMPARE(ModeUtils::normalizeMode("PKTLSB"), QString("LSB-D"));
    QCOMPARE(ModeUtils::normalizeMode("DIGL"),   QString("LSB-D"));
}

void TestModeUtils::testGenericDigitalSpellings()
{
    // "DIGITAL" is the name the contest JSON files and the scoring
    // categories use, so every generic spelling folds into it
    QCOMPARE(ModeUtils::normalizeMode("DIG"),  QString("DIGITAL"));
    QCOMPARE(ModeUtils::normalizeMode("DIGI"), QString("DIGITAL"));
    QCOMPARE(ModeUtils::normalizeMode("DATA"), QString("DIGITAL"));
    QCOMPARE(ModeUtils::normalizeMode("PKT"),  QString("DIGITAL"));
}

void TestModeUtils::testVoiceVariants()
{
    QCOMPARE(ModeUtils::normalizeMode("SSB-U"), QString("USB"));
    QCOMPARE(ModeUtils::normalizeMode("SSB-L"), QString("LSB"));
    QCOMPARE(ModeUtils::normalizeMode("FM-N"),  QString("FM"));
    QCOMPARE(ModeUtils::normalizeMode("AM-N"),  QString("AM"));
}

void TestModeUtils::testCanonicalModesUnchanged()
{
    const QStringList canonical = {"CW", "USB", "LSB", "SSB", "FM", "AM",
                                   "RTTY", "DIGITAL", "USB-D", "LSB-D",
                                   "FT8", "FT4", "PSK31"};
    for (const QString& mode : canonical)
        QCOMPARE(ModeUtils::normalizeMode(mode), mode);
}

void TestModeUtils::testUnknownModesPassThrough()
{
    QCOMPARE(ModeUtils::normalizeMode("SSTV"), QString("SSTV"));
    QCOMPARE(ModeUtils::normalizeMode(""),     QString(""));
    QCOMPARE(ModeUtils::normalizeMode("   "),  QString(""));
}

void TestModeUtils::testIsDigital()
{
    QVERIFY(ModeUtils::isDigital("RTTY"));
    QVERIFY(ModeUtils::isDigital("RTTY-L"));   // via normalization
    QVERIFY(ModeUtils::isDigital("DATA-U"));
    QVERIFY(ModeUtils::isDigital("USB-D"));
    QVERIFY(ModeUtils::isDigital("DIG"));
    QVERIFY(ModeUtils::isDigital("FT8"));

    QVERIFY(!ModeUtils::isDigital("CW"));
    QVERIFY(!ModeUtils::isDigital("CW-U"));
    QVERIFY(!ModeUtils::isDigital("USB"));
    QVERIFY(!ModeUtils::isDigital("LSB"));
    QVERIFY(!ModeUtils::isDigital("FM"));
    QVERIFY(!ModeUtils::isDigital(""));
}

// Every logged, loaded and imported QSO goes through QsoRecord::setMode, so
// a rig variant can never reach scoring or Cabrillo export
void TestModeUtils::testQsoRecordNormalizesOnSet()
{
    QsoRecord qso;
    qso.setMode("CW-U");
    QCOMPARE(qso.getMode(), QString("CW"));

    qso.setMode("rtty-l");
    QCOMPARE(qso.getMode(), QString("RTTY"));

    qso.setMode("USB");
    QCOMPARE(qso.getMode(), QString("USB"));
}

QTEST_MAIN(TestModeUtils)
#include "test_modeutils.moc"
