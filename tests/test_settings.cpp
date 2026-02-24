/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
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
