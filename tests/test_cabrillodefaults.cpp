/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "utils/cabrilloDefaults.h"

// CabrilloDefaults pre-fills the Cabrillo export dialog from contest setup.
// Checked against the shipped definitions.
class TestCabrilloDefaults : public QObject
{
    Q_OBJECT

private slots:
    void paqpMobileQrp();
    void jidxCompactClassCode();
    void naqpStationClass();
    void cqpCategoryPrompts();
    void locationPromptsAreNotRead();
    void stateCodeInClassIsNotMultiOp();
    void rsgbMultiNIsTransmitterNotBand();
    void explicitHeaderMaps();
    void singleModeContest();
    void iotaHeaderMap();
    void legacyValuesToCabrillo3();
    void everyOptionYieldsValidValues();

private:
    static QJsonObject load(const QString& name);
};

QJsonObject TestCabrilloDefaults::load(const QString& name)
{
    QFile f(QStringLiteral(CLX_SOURCE_DIR "/contests/") + name + ".json");
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    return QJsonDocument::fromJson(f.readAll()).object();
}

// Bob K4RFK's case: PAQP mobile entry, QRP.
void TestCabrilloDefaults::paqpMobileQrp()
{
    const auto h = CabrilloDefaults::fromSetup(load("paqp"), QString(),
        {{"stationType", "PA"}, {"operatingCategory", "MOBILE-SO"}, {"powerCategory", "QRP"}});
    QCOMPARE(h.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(h.value("CATEGORY-STATION"), QStringLiteral("MOBILE"));
    QCOMPARE(h.value("CATEGORY-POWER"), QStringLiteral("QRP"));
    QCOMPARE(h.value("CATEGORY-TRANSMITTER"), QStringLiteral("ONE"));

    const auto ph = CabrilloDefaults::fromSetup(load("paqp"), QString(),
        {{"operatingCategory", "SO-PH"}, {"powerCategory", "LP"}});
    QCOMPARE(ph.value("CATEGORY-MODE"), QStringLiteral("SSB"));
    QCOMPARE(ph.value("CATEGORY-POWER"), QStringLiteral("LOW"));
}

void TestCabrilloDefaults::jidxCompactClassCode()
{
    const auto h = CabrilloDefaults::fromSetup(load("jidx"), "DX",
        {{"contestMode", "SSB"}, {"operatingCategory", "SOHP-AB"}});
    QCOMPARE(h.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(h.value("CATEGORY-POWER"), QStringLiteral("HIGH"));
    QCOMPARE(h.value("CATEGORY-BAND"), QStringLiteral("ALL"));
    QCOMPARE(h.value("CATEGORY-MODE"), QStringLiteral("SSB"));

    const auto sb = CabrilloDefaults::fromSetup(load("jidx"), "DX",
        {{"contestMode", "CW"}, {"operatingCategory", "SOLP-40"}});
    QCOMPARE(sb.value("CATEGORY-BAND"), QStringLiteral("40M"));
    QCOMPARE(sb.value("CATEGORY-POWER"), QStringLiteral("LOW"));

    const auto m2 = CabrilloDefaults::fromSetup(load("jidx"), "JA",
        {{"contestMode", "CW"}, {"operatingCategory", "M2"}});
    QCOMPARE(m2.value("CATEGORY-OPERATOR"), QStringLiteral("MULTI-OP"));
    QCOMPARE(m2.value("CATEGORY-TRANSMITTER"), QStringLiteral("TWO"));
}

void TestCabrilloDefaults::naqpStationClass()
{
    const auto h = CabrilloDefaults::fromSetup(load("naqp"), "SO_ASSISTED_RTTY", {});
    QCOMPARE(h.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(h.value("CATEGORY-ASSISTED"), QStringLiteral("ASSISTED"));
    QCOMPARE(h.value("CATEGORY-MODE"), QStringLiteral("RTTY"));

    const auto u = CabrilloDefaults::fromSetup(load("naqp"), "SO_UNASSISTED_CW", {});
    QCOMPARE(u.value("CATEGORY-ASSISTED"), QStringLiteral("NON-ASSISTED"));
    QCOMPARE(u.value("CATEGORY-MODE"), QStringLiteral("CW"));
}

void TestCabrilloDefaults::cqpCategoryPrompts()
{
    const auto h = CabrilloDefaults::fromSetup(load("cqp"), QString(),
        {{"stationType", "CA"}, {"operatingCategory", "MS"}, {"powerCategory", "HP"}});
    QCOMPARE(h.value("CATEGORY-OPERATOR"), QStringLiteral("MULTI-OP"));
    QCOMPARE(h.value("CATEGORY-TRANSMITTER"), QStringLiteral("ONE"));
    QCOMPARE(h.value("CATEGORY-POWER"), QStringLiteral("HIGH"));

    const auto soa = CabrilloDefaults::fromSetup(load("cqp"), QString(),
        {{"operatingCategory", "SOA"}, {"powerCategory", "LP"}});
    QCOMPARE(soa.value("CATEGORY-ASSISTED"), QStringLiteral("ASSISTED"));
    QCOMPARE(soa.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
}

// stationType answers are places: MS (Mississippi) must not become
// Multi-Single.
void TestCabrilloDefaults::locationPromptsAreNotRead()
{
    const auto h = CabrilloDefaults::fromSetup(load("msqp"), QString(), {{"stationType", "MS"}});
    QVERIFY(!h.contains("CATEGORY-OPERATOR"));
    QVERIFY(!h.contains("CATEGORY-TRANSMITTER"));
}

// MSQP "MS_SO_FIXED" is a Mississippi single op; MOQP "MO_FIXED_MO" is a
// Missouri multi-op; YBDX "MO_ST" is a multi-op.
void TestCabrilloDefaults::stateCodeInClassIsNotMultiOp()
{
    QCOMPARE(CabrilloDefaults::fromSetup(load("msqp"), QString(), {{"operatingClass", "MS_SO_FIXED"}})
                 .value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    const auto mo = CabrilloDefaults::fromSetup(load("moqp"), QString(), {{"operatingClass", "MO_MOB_SO_LP_CW"}});
    QCOMPARE(mo.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(mo.value("CATEGORY-STATION"), QStringLiteral("MOBILE"));
    QCOMPARE(mo.value("CATEGORY-TRANSMITTER"), QStringLiteral("ONE"));
    QCOMPARE(CabrilloDefaults::fromSetup(load("moqp"), QString(), {{"operatingClass", "MO_FIXED_MO"}})
                 .value("CATEGORY-OPERATOR"), QStringLiteral("MULTI-OP"));
    QCOMPARE(CabrilloDefaults::fromSetup(load("ybdx"), "MO_ST_ANY_POWER", {})
                 .value("CATEGORY-OPERATOR"), QStringLiteral("MULTI-OP"));
}

void TestCabrilloDefaults::rsgbMultiNIsTransmitterNotBand()
{
    const auto h = CabrilloDefaults::fromSetup(load("rsgb_iota"), QString(), {{"operatingCategory", "MULTI-2"}});
    QCOMPARE(h.value("CATEGORY-OPERATOR"), QStringLiteral("MULTI-OP"));
    QCOMPARE(h.value("CATEGORY-TRANSMITTER"), QStringLiteral("TWO"));
    QVERIFY(!h.contains("CATEGORY-BAND"));
    QCOMPARE(CabrilloDefaults::fromSetup(load("rsgb_iota"), QString(), {{"operatingCategory", "MULTI-1"}})
                 .value("CATEGORY-TRANSMITTER"), QStringLiteral("ONE"));
}

void TestCabrilloDefaults::explicitHeaderMaps()
{
    const auto u = CabrilloDefaults::fromSetup(load("arrl_ss"), QString(),
        {{"contestMode", "CW"}, {"operatingCategory", "U"}, {"powerCategory", "HP"}});
    QCOMPARE(u.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(u.value("CATEGORY-ASSISTED"), QStringLiteral("ASSISTED"));
    QCOMPARE(u.value("CATEGORY-POWER"), QStringLiteral("HIGH"));
    const auto sch = CabrilloDefaults::fromSetup(load("arrl_ss"), QString(), {{"operatingCategory", "S"}});
    QCOMPARE(sch.value("CATEGORY-STATION"), QStringLiteral("SCHOOL"));

    const auto rl = CabrilloDefaults::fromSetup(load("arrl_vhf"), "RL", {{"contestMonth", "june"}});
    QCOMPARE(rl.value("CATEGORY-STATION"), QStringLiteral("ROVER-LIMITED"));
    QCOMPARE(rl.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(CabrilloDefaults::fromSetup(load("arrl_vhf"), "LM", {}).value("CATEGORY-TRANSMITTER"),
             QStringLiteral("LIMITED"));
}

void TestCabrilloDefaults::singleModeContest()
{
    const auto h = CabrilloDefaults::fromSetup(load("cqww_rtty"), QString(),
        {{"operatingCategory", "SOA"}, {"powerCategory", "LP"}});
    QCOMPARE(h.value("CATEGORY-MODE"), QStringLiteral("RTTY"));
    QCOMPARE(h.value("CATEGORY-ASSISTED"), QStringLiteral("ASSISTED"));
}

void TestCabrilloDefaults::iotaHeaderMap()
{
    const auto h = CabrilloDefaults::fromSetup(load("rsgb_iota"), QString(),
        {{"stationType", "DXPEDITION"}, {"islandName", "Lundy Island"},
         {"operatingCategory", "SO"}, {"operatingTime", "12-HOURS"}, {"powerCategory", "LP"}});
    QCOMPARE(h.value("CATEGORY-LOCATION"), QStringLiteral("DXPEDITION"));
    QCOMPARE(h.value("IOTA-ISLAND-NAME"), QStringLiteral("Lundy Island"));
    QCOMPARE(h.value("CATEGORY-TIME"), QStringLiteral("12-HOURS"));
    QCOMPARE(h.value("CATEGORY-OPERATOR"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(h.value("CATEGORY-POWER"), QStringLiteral("LOW"));

    const auto island = CabrilloDefaults::fromSetup(load("rsgb_iota"), QString(), {{"stationType", "ISLAND"}});
    QCOMPARE(island.value("CATEGORY-LOCATION"), QStringLiteral("NON-DXPEDITION"));
    const auto world = CabrilloDefaults::fromSetup(load("rsgb_iota"), QString(), {{"stationType", "WORLD"}});
    QCOMPARE(world.value("CATEGORY-LOCATION"), QStringLiteral("WORLD"));
    QVERIFY(!world.contains("IOTA-ISLAND-NAME"));
}

void TestCabrilloDefaults::legacyValuesToCabrillo3()
{
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-OPERATOR", "SINGLE"), QStringLiteral("SINGLE-OP"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-OPERATOR", "MULTI"), QStringLiteral("MULTI-OP"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-ASSISTED", "YES"), QStringLiteral("ASSISTED"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-ASSISTED", "NO"), QStringLiteral("NON-ASSISTED"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-BAND", "ALL-BAND"), QStringLiteral("ALL"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-MODE", "DIGITAL"), QStringLiteral("DIGI"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-MODE", "FT8"), QStringLiteral("DIGI"));
    QCOMPARE(CabrilloDefaults::toCabrillo3("CATEGORY-POWER", "HIGH"), QStringLiteral("HIGH"));
}

// Every option of every category prompt and station class in every shipped
// contest produces only legal Cabrillo 3.0 values. Prints the table (-v2) for
// review.
void TestCabrilloDefaults::everyOptionYieldsValidValues()
{
    const QMap<QString, QStringList> legal = {
        {"CATEGORY-OPERATOR", {"SINGLE-OP", "MULTI-OP", "CHECKLOG"}},
        {"CATEGORY-ASSISTED", {"ASSISTED", "NON-ASSISTED"}},
        {"CATEGORY-POWER", {"HIGH", "LOW", "QRP"}},
        {"CATEGORY-MODE", {"CW", "SSB", "RTTY", "DIGI", "FM", "MIXED"}},
        {"CATEGORY-BAND", {"ALL", "160M", "80M", "40M", "20M", "15M", "10M", "6M", "2M"}},
        {"CATEGORY-TRANSMITTER", {"ONE", "TWO", "LIMITED", "UNLIMITED", "SWL"}},
        {"CATEGORY-STATION", {"FIXED", "MOBILE", "PORTABLE", "ROVER", "ROVER-LIMITED", "ROVER-UNLIMITED",
                              "EXPEDITION", "HQ", "SCHOOL", "DISTRIBUTED"}},
    };
    static const QStringList categoryPrompts = {
        "operatingCategory", "operatingClass", "powerCategory", "contestMode", "modeCategory"};

    QDir dir(QStringLiteral(CLX_SOURCE_DIR "/contests"));
    for (const QString& file : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
        const QString name = QFileInfo(file).completeBaseName();
        const QJsonObject def = load(name);
        QStringList choices;  // "promptId=value" or "class=id"
        for (const QJsonValue& v : def["userPrompts"].toArray()) {
            const QJsonObject p = v.toObject();
            if (!categoryPrompts.contains(p["id"].toString())) continue;
            for (const QJsonValue& o : p["options"].toArray())
                choices << p["id"].toString() + "=" + o.toObject()["value"].toString();
        }
        for (const QJsonValue& c : def["stationClasses"].toObject()["classes"].toArray())
            choices << "class=" + c.toObject()["id"].toString();

        for (const QString& choice : choices) {
            const QString key = choice.section('=', 0, 0);
            const QString value = choice.section('=', 1);
            const auto h = (key == "class")
                ? CabrilloDefaults::fromSetup(def, value, {})
                : CabrilloDefaults::fromSetup(def, QString(), {{key, value}});
            QStringList shown;
            for (auto it = h.cbegin(); it != h.cend(); ++it) {
                shown << it.key().mid(9) + "=" + it.value();
                if (legal.contains(it.key()))
                    QVERIFY2(legal[it.key()].contains(it.value()),
                             qPrintable(QString("%1 %2 -> %3 %4").arg(name, choice, it.key(), it.value())));
            }
            qInfo().noquote() << name << choice << "->" << shown.join(" ");
        }
    }
}

QTEST_MAIN(TestCabrilloDefaults)
#include "test_cabrillodefaults.moc"
