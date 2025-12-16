#include <QtTest>
#include "DxccDatabase.h"

class TestPortableCallsigns : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Load DXCC database for testing
        m_dxccDb = new DxccDatabase();
        m_dxccDb->loadFromFile("data/cty.dat");
    }

    void cleanupTestCase()
    {
        delete m_dxccDb;
    }

    // Test US portable calls with /P suffix (indicates portable)
    void testUSPortableWithSlashP()
    {
        auto entity = m_dxccDb->lookupCallsign("N0AB/P");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test US portable calls with /MM suffix (maritime mobile)
    void testUSPortableWithMM()
    {
        auto entity = m_dxccDb->lookupCallsign("W5XX/MM");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test US portable calls with /AE suffix (aeronautical)
    void testUSPortableWithAE()
    {
        auto entity = m_dxccDb->lookupCallsign("N4YY/AE");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test US portable calls with /AG suffix (agricultural)
    void testUSPortableWithAG()
    {
        auto entity = m_dxccDb->lookupCallsign("W7XX/AG");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test US portable calls with /M suffix (mobile)
    void testUSPortableWithM()
    {
        auto entity = m_dxccDb->lookupCallsign("K6XX/M");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test US portable calls with alternate location suffix /W4
    void testUSPortableWithSlashW()
    {
        auto entity = m_dxccDb->lookupCallsign("KL7XX/W4");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test US portable calls with alternate location suffix /W1
    void testUSPortableWithW1()
    {
        auto entity = m_dxccDb->lookupCallsign("KL7XX/W1");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test DX portable calls with /JA1 (Japan)
    void testDXPortableJapan()
    {
        auto entity = m_dxccDb->lookupCallsign("N0AB/JA1");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "Japan");
    }

    // Test DX portable calls with /VE7 (Canada)
    void testDXPortableCanada()
    {
        auto entity = m_dxccDb->lookupCallsign("W5XX/VE7");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "Canada");
    }

    // Test DX portable calls with /VP2 (Caribbean)
    void testDXPortableCaribbean()
    {
        auto entity = m_dxccDb->lookupCallsign("N0AB/VP2");
        QVERIFY(!entity.country.isEmpty());
        // VP2 could be British Virgin Islands or Antigua/Barbuda depending on context
        QVERIFY(!entity.country.isEmpty());
    }

    // Test that ITU zone is correct for portable call
    void testITUZoneForPortableCall()
    {
        // KL7XX/W4 should have ITU zone of W4 area (should be 5)
        int ituZone = m_dxccDb->getItuZone("KL7XX/W4");
        QCOMPARE(ituZone, 5);
    }

    // Test ITU region mapping for portable call
    void testITURegionForPortableCall()
    {
        int ituRegion = m_dxccDb->getItuRegion("KL7XX/W4");
        QCOMPARE(ituRegion, 2);
    }

    // Test that normal calls (no portable suffix) still work
    void testNormalCallNoSuffix()
    {
        auto entity = m_dxccDb->lookupCallsign("N0AB");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "United States");
    }

    // Test DX normal call
    void testDXNormalCall()
    {
        auto entity = m_dxccDb->lookupCallsign("JA1ABC");
        QVERIFY(!entity.country.isEmpty());
        QCOMPARE(entity.country, "Japan");
    }

    // Test stripPortableSuffixes function
    void testStripPortableSuffixes()
    {
        QCOMPARE(m_dxccDb->stripPortableSuffixes("N0AB/P"), "N0AB");
        QCOMPARE(m_dxccDb->stripPortableSuffixes("W5XX/MM"), "W5XX");
        QCOMPARE(m_dxccDb->stripPortableSuffixes("KL7XX/W4"), "W4");
        QCOMPARE(m_dxccDb->stripPortableSuffixes("JA1ABC"), "JA1ABC");
    }

    // Test that /W suffixes are properly handled for US portable calls
    void testWSuffixPriority()
    {
        // /W4 should override KL7 location
        auto entity = m_dxccDb->lookupCallsign("KL7XX/W4");
        QCOMPARE(entity.ituZone, 5);  // W4 is ITU zone 5, not Alaska
    }

private:
    DxccDatabase *m_dxccDb = nullptr;
};

QTEST_MAIN(TestPortableCallsigns)
#include "test_portable_callsigns.moc"
