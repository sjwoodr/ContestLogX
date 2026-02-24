/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include <QtTest/QtTest>
#include "../src/utils/bandPlan.h"

class TestBandPlan : public QObject
{
    Q_OBJECT

private slots:
    void testFreq2BandHF();
    void testFreq2BandVHF();
    void testFreq2BandMicrowave();
    void testFreq2CabrilloBandHF();
    void testFreq2CabrilloBandVHF();
    void testFreq2CabrilloBandMicrowave();
    void testFreq2CabrilloBand10GHz();
};

void TestBandPlan::testFreq2BandHF()
{
    QCOMPARE(BandPlan::freq2Band(1800.0), QString("160m"));
    QCOMPARE(BandPlan::freq2Band(3750.0), QString("80m"));
    QCOMPARE(BandPlan::freq2Band(7100.0), QString("40m"));
    QCOMPARE(BandPlan::freq2Band(14250.0), QString("20m"));
    QCOMPARE(BandPlan::freq2Band(21100.0), QString("15m"));
    QCOMPARE(BandPlan::freq2Band(28500.0), QString("10m"));
}

void TestBandPlan::testFreq2BandVHF()
{
    QCOMPARE(BandPlan::freq2Band(50000.0), QString("6m"));
    QCOMPARE(BandPlan::freq2Band(144000.0), QString("2m"));
    QCOMPARE(BandPlan::freq2Band(222000.0), QString("1.25m"));
    QCOMPARE(BandPlan::freq2Band(432000.0), QString("70cm"));
}

void TestBandPlan::testFreq2BandMicrowave()
{
    QCOMPARE(BandPlan::freq2Band(902000.0), QString("33cm"));
    QCOMPARE(BandPlan::freq2Band(1270000.0), QString("23cm"));
    QCOMPARE(BandPlan::freq2Band(2350000.0), QString("13cm"));
    QCOMPARE(BandPlan::freq2Band(3400000.0), QString("9cm"));
    QCOMPARE(BandPlan::freq2Band(5800000.0), QString("6cm"));
    QCOMPARE(BandPlan::freq2Band(10100000.0), QString("3cm"));
    QCOMPARE(BandPlan::freq2Band(24100000.0), QString("1.25cm"));
    QCOMPARE(BandPlan::freq2Band(47100000.0), QString("6mm"));
}

void TestBandPlan::testFreq2CabrilloBandHF()
{
    // HF bands should return frequency in kHz as string
    QCOMPARE(BandPlan::freq2CabrilloBand(1800.0), QString("1800"));
    QCOMPARE(BandPlan::freq2CabrilloBand(3750.0), QString("3750"));
    QCOMPARE(BandPlan::freq2CabrilloBand(7100.0), QString("7100"));
    QCOMPARE(BandPlan::freq2CabrilloBand(14250.0), QString("14250"));
    QCOMPARE(BandPlan::freq2CabrilloBand(21100.0), QString("21100"));
    QCOMPARE(BandPlan::freq2CabrilloBand(28500.0), QString("28500"));
}

void TestBandPlan::testFreq2CabrilloBandVHF()
{
    // VHF bands should return band shorthand
    QCOMPARE(BandPlan::freq2CabrilloBand(50000.0), QString("50"));
    QCOMPARE(BandPlan::freq2CabrilloBand(144000.0), QString("144"));
    QCOMPARE(BandPlan::freq2CabrilloBand(222000.0), QString("222"));
    QCOMPARE(BandPlan::freq2CabrilloBand(432000.0), QString("432"));
}

void TestBandPlan::testFreq2CabrilloBandMicrowave()
{
    // Microwave bands should return band shorthand with G suffix
    QCOMPARE(BandPlan::freq2CabrilloBand(902000.0), QString("902"));
    QCOMPARE(BandPlan::freq2CabrilloBand(1270000.0), QString("1.2G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(2350000.0), QString("2.3G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(3400000.0), QString("3.4G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(5800000.0), QString("5.7G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(24100000.0), QString("24G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(47100000.0), QString("47G"));
}

void TestBandPlan::testFreq2CabrilloBand10GHz()
{
    // Specific test for 10 GHz (3cm band)
    // This frequency range is 10000-10500 MHz
    QCOMPARE(BandPlan::freq2CabrilloBand(10100000.0), QString("10G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(10000000.0), QString("10G"));
    QCOMPARE(BandPlan::freq2CabrilloBand(10450000.0), QString("10G"));
}

QTEST_MAIN(TestBandPlan)
#include "test_bandplan.moc"
