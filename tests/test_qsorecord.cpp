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
#include "../include/qsoRecord.h"

class TestQsoRecord : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor();
    void testSettersAndGetters();
    void testCallsignValidation();
    void testFrequencyConversion();
    void testDateTimeHandling();
};

void TestQsoRecord::testDefaultConstructor()
{
    QsoRecord qso;
    QVERIFY(qso.getCall().isEmpty());
    QVERIFY(qso.getFrequency().isEmpty());
    QCOMPARE(qso.getMode(), QString("USB")); // Default mode is USB
}

void TestQsoRecord::testSettersAndGetters()
{
    QsoRecord qso;
    qso.setCall("W1AW");
    qso.setFrequency("14.250");
    qso.setMode("USB");
    qso.setRstSent("59");
    qso.setRstReceived("59");
    
    QCOMPARE(qso.getCall(), QString("W1AW"));
    QCOMPARE(qso.getFrequency(), QString("14.250"));
    QCOMPARE(qso.getMode(), QString("USB"));
    QCOMPARE(qso.getRstSent(), QString("59"));
    QCOMPARE(qso.getRstReceived(), QString("59"));
}

void TestQsoRecord::testCallsignValidation()
{
    QsoRecord qso;
    qso.setCall("W1AW");
    QVERIFY(!qso.getCall().isEmpty());
    
    qso.setCall("");
    QVERIFY(qso.getCall().isEmpty());
}

void TestQsoRecord::testFrequencyConversion()
{
    QsoRecord qso;
    qso.setFrequency("14.250");
    QString freq = qso.getFrequency();
    QVERIFY(!freq.isEmpty());
    QVERIFY(freq.toDouble() > 14.0 && freq.toDouble() < 15.0);
}

void TestQsoRecord::testDateTimeHandling()
{
    QsoRecord qso;
    QDateTime now = QDateTime::currentDateTime();
    qso.setDateTime(now);
    QCOMPARE(qso.getDateTime().date(), now.date());
    QCOMPARE(qso.getDateTime().time().hour(), now.time().hour());
    QCOMPARE(qso.getDateTime().time().minute(), now.time().minute());
}

QTEST_MAIN(TestQsoRecord)
#include "test_qsorecord.moc"
