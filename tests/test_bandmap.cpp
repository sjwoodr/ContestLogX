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
#include "../include/bandMapWidget.h"

class TestBandMap : public QObject
{
    Q_OBJECT

private slots:
    void testDedupKey_sameCallSameFreq();
    void testDedupKey_sameCallWithin01kHz();
    void testDedupKey_sameCallDifferentFreq();
    void testDedupKey_differentCall();

    void testAddOrUpdateSpot_newSpot();
    void testAddOrUpdateSpot_duplicateUpdatesTimestamp();
    void testAddOrUpdateSpot_duplicateKeepsSingleMarker();
    void testAddOrUpdateSpot_maxSpotsEvictsOldest();
    void testAddOrUpdateSpot_ignoresEmptyCallsign();
    void testAddOrUpdateSpot_ignoresZeroFreq();

    void testRefreshAllStatuses();
    void testOnExpiryTimer_removesExpiredSpots();
    void testOnExpiryTimer_keepsActiveSpots();

    void testSetBandRange_clearsSpots();
    void testSetBandRange_resetsViewport();
    void testAddOrUpdateSpot_rejectsOutOfBandSpot();

private:
    SpotData makeSpot(const QString &call, double freqMhz,
                      const QString &mode = "CW",
                      ContactStatus status = ContactStatus::Unknown,
                      int ageSecs = 0)
    {
        SpotData s;
        s.callsign  = call;
        s.freqMhz   = freqMhz;
        s.mode      = mode;
        s.spotter   = "K9TEST";
        s.timestamp = QDateTime::currentDateTimeUtc().addSecs(-ageSecs);
        s.status    = status;
        return s;
    }
};

// ── dedupKey tests ─────────────────────────────────────────────────────────

void TestBandMap::testDedupKey_sameCallSameFreq()
{
    SpotData a = makeSpot("W1AW", 14.025);
    SpotData b = makeSpot("W1AW", 14.025);
    QCOMPARE(BandMapWidget::dedupKey(a), BandMapWidget::dedupKey(b));
}

void TestBandMap::testDedupKey_sameCallWithin01kHz()
{
    // 14.025000 vs 14.025049 — both round to the same 0.1 kHz bucket (14.0250)
    SpotData a = makeSpot("W1AW", 14.025000);
    SpotData b = makeSpot("W1AW", 14.025049);
    QCOMPARE(BandMapWidget::dedupKey(a), BandMapWidget::dedupKey(b));
}

void TestBandMap::testDedupKey_sameCallDifferentFreq()
{
    // 14.025 vs 14.026 — 1 kHz apart → different buckets
    SpotData a = makeSpot("W1AW", 14.025);
    SpotData b = makeSpot("W1AW", 14.026);
    QVERIFY(BandMapWidget::dedupKey(a) != BandMapWidget::dedupKey(b));
}

void TestBandMap::testDedupKey_differentCall()
{
    SpotData a = makeSpot("W1AW", 14.025);
    SpotData b = makeSpot("K1TTT", 14.025);
    QVERIFY(BandMapWidget::dedupKey(a) != BandMapWidget::dedupKey(b));
}

// ── addOrUpdateSpot tests ──────────────────────────────────────────────────

void TestBandMap::testAddOrUpdateSpot_newSpot()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    bm.addOrUpdateSpot(makeSpot("W1AW", 14.025));
    QCOMPARE(bm.spotCount(), 1);
}

void TestBandMap::testAddOrUpdateSpot_duplicateUpdatesTimestamp()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");

    SpotData s = makeSpot("W1AW", 14.025, "CW", ContactStatus::Unknown, 100);
    bm.addOrUpdateSpot(s);

    // Re-spot with a fresh timestamp
    SpotData fresh = makeSpot("W1AW", 14.025, "CW", ContactStatus::Worked, 0);
    bm.addOrUpdateSpot(fresh);

    // Still only one marker
    QCOMPARE(bm.spotCount(), 1);

    // The stored spot should reflect the latest status (Worked)
    QCOMPARE(bm.spotStatus("W1AW"), ContactStatus::Worked);

    // Timestamp should be within the last few seconds
    QDateTime ts = bm.spotTimestamp("W1AW");
    qint64 ageMs = ts.msecsTo(QDateTime::currentDateTimeUtc());
    QVERIFY(ageMs >= 0 && ageMs < 5000);
}

void TestBandMap::testAddOrUpdateSpot_duplicateKeepsSingleMarker()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    for (int i = 0; i < 5; ++i)
        bm.addOrUpdateSpot(makeSpot("W1AW", 14.025));
    QCOMPARE(bm.spotCount(), 1);
}

void TestBandMap::testAddOrUpdateSpot_maxSpotsEvictsOldest()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    bm.setMaxSpots(5);

    // Add 5 spots with increasing age so we know which is oldest
    for (int i = 0; i < 5; ++i) {
        QString call = QString("K%1AAA").arg(i);
        SpotData s = makeSpot(call, 14.020 + i * 0.001, "CW",
                              ContactStatus::Unknown, 100 - i);
        bm.addOrUpdateSpot(s);
    }
    QCOMPARE(bm.spotCount(), 5);

    // Adding one more should evict the oldest (K0AAA, ageSecs=100)
    bm.addOrUpdateSpot(makeSpot("W9NEW", 14.030));
    QCOMPARE(bm.spotCount(), 5);

    // The oldest spot (K0AAA) should be gone
    QCOMPARE(bm.spotStatus("K0AAA"), ContactStatus::Unknown); // Unknown = not found
}

void TestBandMap::testAddOrUpdateSpot_ignoresEmptyCallsign()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    SpotData s = makeSpot("", 14.025);
    bm.addOrUpdateSpot(s);
    QCOMPARE(bm.spotCount(), 0);
}

void TestBandMap::testAddOrUpdateSpot_ignoresZeroFreq()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    SpotData s = makeSpot("W1AW", 0.0);
    bm.addOrUpdateSpot(s);
    QCOMPARE(bm.spotCount(), 0);
}

// ── refreshAllStatuses tests ───────────────────────────────────────────────

void TestBandMap::testRefreshAllStatuses()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    bm.addOrUpdateSpot(makeSpot("W1AW",  14.025, "CW", ContactStatus::Unknown));
    bm.addOrUpdateSpot(makeSpot("K1TTT", 14.030, "CW", ContactStatus::Unknown));

    bm.refreshAllStatuses([](const QString &call) -> ContactStatus {
        if (call == "W1AW")  return ContactStatus::NewMultiplier;
        if (call == "K1TTT") return ContactStatus::Worked;
        return ContactStatus::Unknown;
    });

    QCOMPARE(bm.spotStatus("W1AW"),  ContactStatus::NewMultiplier);
    QCOMPARE(bm.spotStatus("K1TTT"), ContactStatus::Worked);
}

// ── onExpiryTimer tests ────────────────────────────────────────────────────

void TestBandMap::testOnExpiryTimer_removesExpiredSpots()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    bm.setExpirySeconds(60); // 1 minute

    // Add a spot that is already 90 seconds old (past expiry)
    bm.addOrUpdateSpot(makeSpot("W1AW", 14.025, "CW", ContactStatus::Unknown, 90));
    QCOMPARE(bm.spotCount(), 1);

    bm.onExpiryTimer();
    QCOMPARE(bm.spotCount(), 0);
}

void TestBandMap::testOnExpiryTimer_keepsActiveSpots()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    bm.setExpirySeconds(60); // 1 minute

    // Add a spot that is only 10 seconds old (not expired)
    bm.addOrUpdateSpot(makeSpot("W1AW", 14.025, "CW", ContactStatus::Unknown, 10));
    QCOMPARE(bm.spotCount(), 1);

    bm.onExpiryTimer();
    QCOMPARE(bm.spotCount(), 1);
}

// ── setBandRange tests ─────────────────────────────────────────────────────

void TestBandMap::testSetBandRange_clearsSpots()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    bm.addOrUpdateSpot(makeSpot("W1AW", 14.025));
    QCOMPARE(bm.spotCount(), 1);

    // Changing band clears all spots
    bm.setBandRange(7.0, 7.3, "40m");
    QCOMPARE(bm.spotCount(), 0);
}

void TestBandMap::testSetBandRange_resetsViewport()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    QCOMPARE(bm.visibleMinMhz(), 14.0);
    QCOMPARE(bm.visibleMaxMhz(), 14.35);
}

void TestBandMap::testAddOrUpdateSpot_rejectsOutOfBandSpot()
{
    BandMapWidget bm;
    bm.setBandRange(14.0, 14.35, "20m");
    // 21 MHz is outside the 20m band range
    bm.addOrUpdateSpot(makeSpot("JA1ABC", 21.025));
    QCOMPARE(bm.spotCount(), 0);
}

QTEST_MAIN(TestBandMap)
#include "test_bandmap.moc"
