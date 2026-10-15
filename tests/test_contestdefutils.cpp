/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#include "utils/contestDefUtils.h"

// ContestDefUtils::sendsSerialNumber decides whether the status bar and the
// Next S/N panel show a next serial number. Run it against the shipped
// contest definitions so a new contest (or a definition edit) that changes
// the answer is noticed.
class TestContestDefUtils : public QObject
{
    Q_OBJECT

private slots:
    void serialContestsWithDefaultSetup();
    void dxOnlySerialDependsOnStationType();
    void stationClassSerials();

private:
    static QJsonObject load(const QString& name);
    static bool sends(const QJsonObject& def, const QString& stationClass = QString(),
                      const QMap<QString, QString>& prompts = {});
};

QJsonObject TestContestDefUtils::load(const QString& name)
{
    QFile f(QStringLiteral(CLX_SOURCE_DIR "/contests/") + name + ".json");
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool TestContestDefUtils::sends(const QJsonObject& def, const QString& stationClass,
                                const QMap<QString, QString>& prompts)
{
    return ContestDefUtils::sendsSerialNumber(def, stationClass,
        [&prompts](const QString& id) { return prompts.value(id); });
}

// With no station class and no setup answers, exactly these definitions send
// a serial number. Update this list when a contest with a serial is added.
// (OK-OM, RDXC and SP DX send one only from DX stations - see below.)
void TestContestDefUtils::serialContestsWithDefaultSetup()
{
    const QStringList expected = {
        "arrl_ss", "cqp", "cqwpx", "nasprint_ssb", "ocdx", "paqp", "rsgb_iota", "vaqp"
    };

    QDir dir(QStringLiteral(CLX_SOURCE_DIR "/contests"));
    const QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
    QVERIFY(files.size() > 50);

    QStringList actual;
    for (const QString& file : files) {
        const QString name = QFileInfo(file).completeBaseName();
        const QJsonObject def = load(name);
        QVERIFY2(!def.isEmpty(), qPrintable("cannot parse " + file));
        if (sends(def))
            actual << name;
    }
    QCOMPARE(actual, expected);
}

// OK-OM, RDXC and SP DX: DX stations send a serial; home stations send their
// county / oblast / province instead.
void TestContestDefUtils::dxOnlySerialDependsOnStationType()
{
    const QMap<QString, QString> home = {{"okomdx", "OK_OM"}, {"rdxc", "UA"}, {"spdx", "SP"}};
    for (auto it = home.cbegin(); it != home.cend(); ++it) {
        const QJsonObject def = load(it.key());
        QVERIFY2(!def.isEmpty(), qPrintable(it.key()));
        QVERIFY2(sends(def, QString(), {{"stationType", "DX"}}), qPrintable(it.key() + " DX"));
        QVERIFY2(!sends(def, QString(), {{"stationType", it.value()}}), qPrintable(it.key() + " home"));
    }
}

// ARRL 10m: only the DX class sends a serial. YBDX: its serial classes do.
void TestContestDefUtils::stationClassSerials()
{
    const QJsonObject arrl10 = load("arrl_10m");
    QVERIFY(!arrl10.isEmpty());
    QVERIFY(sends(arrl10, "DX"));
    QVERIFY(!sends(arrl10, "W_VE_XE"));

    const QJsonObject ybdx = load("ybdx");
    QVERIFY(!ybdx.isEmpty());
    QVERIFY(sends(ybdx, "SO_ANY_POWER"));

    const QJsonObject naqp = load("naqp");
    QVERIFY(!naqp.isEmpty());
    QVERIFY(!sends(naqp, "SO_UNASSISTED_CW"));
}

QTEST_MAIN(TestContestDefUtils)
#include "test_contestdefutils.moc"
