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
#include "../src/utils/callsignUtils.h"

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
