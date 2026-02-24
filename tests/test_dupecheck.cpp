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

class TestDupeChecking : public QObject
{
    Q_OBJECT

private slots:
    void testBasicDupeLogic();
    void testBandAndModeDupeLogic();

private:
    // Simple dupe checking logic for testing
    bool isDupePerBandAndMode(const QsoRecord& qso, const QList<QsoRecord>& existing);
    bool isDupePerBand(const QsoRecord& qso, const QList<QsoRecord>& existing);
};

// Simplified dupe checking for unit testing
bool TestDupeChecking::isDupePerBandAndMode(const QsoRecord& qso, const QList<QsoRecord>& existing)
{
    for (const QsoRecord& ex : existing) {
        if (ex.getCall().toUpper() == qso.getCall().toUpper()) {
            if (ex.getBand() == qso.getBand() && ex.getMode() == qso.getMode()) {
                return true;
            }
        }
    }
    return false;
}

bool TestDupeChecking::isDupePerBand(const QsoRecord& qso, const QList<QsoRecord>& existing)
{
    for (const QsoRecord& ex : existing) {
        if (ex.getCall().toUpper() == qso.getCall().toUpper()) {
            if (ex.getBand() == qso.getBand()) {
                return true;
            }
        }
    }
    return false;
}

void TestDupeChecking::testBasicDupeLogic()
{
    QList<QsoRecord> qsos;
    
    // Create first QSO: W5TEST on 20m SSB
    QsoRecord qso1;
    qso1.setCall("W5TEST");
    qso1.setBandName("20m");
    qso1.setMode("USB");
    qso1.setFrequency("14250");
    qsos.append(qso1);
    
    // Same call, same band, same mode - should be dupe
    QsoRecord qso2;
    qso2.setCall("W5TEST");
    qso2.setBandName("20m");
    qso2.setMode("USB");
    qso2.setFrequency("14275");
    
    QVERIFY(isDupePerBandAndMode(qso2, qsos));
    QVERIFY(isDupePerBand(qso2, qsos));
    
    // Same call, different band - should NOT be dupe for per-band
    QsoRecord qso3;
    qso3.setCall("W5TEST");
    qso3.setBandName("40m");
    qso3.setMode("USB");
    qso3.setFrequency("7100");
    
    QVERIFY(!isDupePerBand(qso3, qsos));
}

void TestDupeChecking::testBandAndModeDupeLogic()
{
    // Winter Field Day: perBandAndMode dupe checking
    // This test simulates the user's scenario from the bug report
    
    QList<QsoRecord> qsos;
    
    // First QSO: W4WOD on 20m SSB
    QsoRecord qso1;
    qso1.setCall("W4WOD");
    qso1.setBandName("20m");
    qso1.setMode("USB");
    qso1.setFrequency("14250");
    qsos.append(qso1);
    
    // Second QSO: W4WOD on 10GHz CW (3cm band)
    // Should NOT be a dupe (different band and mode)
    QsoRecord qso2;
    qso2.setCall("W4WOD");
    qso2.setBandName("3cm");
    qso2.setMode("CW");
    qso2.setFrequency("10100000");
    
    QVERIFY(!isDupePerBandAndMode(qso2, qsos));
    
    // Third QSO: W4WOD on 20m CW
    // Should NOT be a dupe (different mode on same band)
    QsoRecord qso3;
    qso3.setCall("W4WOD");
    qso3.setBandName("20m");
    qso3.setMode("CW");
    qso3.setFrequency("14050");
    
    QVERIFY(!isDupePerBandAndMode(qso3, qsos));
    
    // Fourth QSO: W4WOD on 20m SSB again
    // Should be a dupe (same band and mode as first QSO)
    QsoRecord qso4;
    qso4.setCall("W4WOD");
    qso4.setBandName("20m");
    qso4.setMode("USB");
    qso4.setFrequency("14300");
    
    QVERIFY(isDupePerBandAndMode(qso4, qsos));
}

QTEST_MAIN(TestDupeChecking)
#include "test_dupecheck.moc"
