/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include <QtTest/QtTest>
#include "../include/settings.h"
#include <QFile>
#include <QDir>

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testCallsignStorage();
    void testOperatorNameStorage();
    void testGridSquareStorage();
    void testFlrigSettings();
    void testCwWpmStorage();
};

void TestSettings::initTestCase()
{
    // Test will use default settings location
}

void TestSettings::cleanupTestCase()
{
    // Clean up test settings if needed
}

void TestSettings::testCallsignStorage()
{
    Settings& settings = Settings::instance();
    settings.setCallsign("N9OH");
    QCOMPARE(settings.getCallsign(), QString("N9OH"));
}

void TestSettings::testOperatorNameStorage()
{
    Settings& settings = Settings::instance();
    settings.setOperatorName("Test Operator");
    QCOMPARE(settings.getOperatorName(), QString("Test Operator"));
}

void TestSettings::testGridSquareStorage()
{
    Settings& settings = Settings::instance();
    settings.setGridSquare("EN52");
    QCOMPARE(settings.getGridSquare(), QString("EN52"));
}

void TestSettings::testFlrigSettings()
{
    Settings& settings = Settings::instance();
    settings.setFlrigHost("localhost");
    settings.setFlrigPort(12345);
    settings.setFlrigAutoConnect(true);
    
    QCOMPARE(settings.getFlrigHost(), QString("localhost"));
    QCOMPARE(settings.getFlrigPort(), 12345);
    QCOMPARE(settings.getFlrigAutoConnect(), true);
}

void TestSettings::testCwWpmStorage()
{
    Settings& settings = Settings::instance();
    settings.setCwWpm(28);
    QCOMPARE(settings.getCwWpm(), 28);
}

QTEST_MAIN(TestSettings)
#include "test_settings.moc"
