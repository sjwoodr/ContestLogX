/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include <QtTest/QtTest>
#include "../src/utils/callsignutils.h"

class TestCallsignUtils : public QObject
{
    Q_OBJECT

private slots:
    void testGetCountryPrefix();
    void testGetCountry();
    void testIsSameCountry();
    void testIsSameContinent();
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

QTEST_MAIN(TestCallsignUtils)
#include "test_callsignutils.moc"
