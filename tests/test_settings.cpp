/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest/QtTest>
#include <QStandardPaths>
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
    // Redirect QStandardPaths to a test-specific location so we don't
    // clobber the user's real settings (operator name, grid, etc.)
    QStandardPaths::setTestModeEnabled(true);
}

void TestSettings::cleanupTestCase()
{
    // Remove the test settings file
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QFile::remove(configPath + "/ContestLogX/ContestLogX.json");
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
