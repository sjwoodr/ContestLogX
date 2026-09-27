/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "cabrilloFile.h"
#include "qsoRecord.h"

/**
 * Cabrillo export tests.
 *
 * The focus is the QSO-line template: placeholders that are not one of the
 * built-in names are resolved from the QSO's exchange field map, which is what
 * lets a contest define its own exchange elements (ARRL SS precedence and
 * check) without a code change.
 */
class TestCabrillo : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void exportsArrlSsQsoLine();
    void exportsRigCwVariantAsCw();
    void mapsCabrilloContestNameToMode();
    void resolvesUnknownPlaceholderToEmpty();
    void leavesClassicTemplatesUnchanged();
    void exportsCqwwRttyQsoLine();
    void exportsZeroPointCqwwQso();
    void exportsAmAsPhone();
    void mapsJidxPhoneContestName();

private:
    QStringList exportLines(const QJsonObject& contestDef,
                            const QList<QsoRecord>& qsos,
                            const QString& myCall,
                            const QString& selectedMode);
    static QsoRecord makeSsQso();

    QJsonObject m_ssDef;
    QTemporaryDir m_tmp;
};

void TestCabrillo::initTestCase()
{
    QVERIFY(m_tmp.isValid());

    // The shipped ARRL SS definition, so the test covers the real template.
    const QString path = QStringLiteral(CLX_SOURCE_DIR "/contests/arrl_ss.json");
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable("cannot open " + path));
    QJsonParseError err {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    QVERIFY2(err.error == QJsonParseError::NoError, qPrintable(err.errorString()));
    m_ssDef = doc.object();
    QVERIFY(!m_ssDef.isEmpty());
}

QStringList TestCabrillo::exportLines(const QJsonObject& contestDef,
                                      const QList<QsoRecord>& qsos,
                                      const QString& myCall,
                                      const QString& selectedMode)
{
    const QString out = m_tmp.filePath("out.cbr");
    CabrilloFile cab;
    const bool ok = cab.exportToFile(out, qsos, contestDef, QJsonObject(), myCall, selectedMode);
    if (!ok) {
        qWarning() << "export failed:" << cab.lastError();
        return QStringList();
    }
    QFile f(out);
    if (!f.open(QIODevice::ReadOnly))
        return QStringList();
    return QString::fromUtf8(f.readAll()).split('\n');
}

// One CW contact: we send 3 A N9OH 79 IN, we receive 52 B K1UI 72 CT.
QsoRecord TestCabrillo::makeSsQso()
{
    QsoRecord qso;
    qso.setCall("K1UI");
    QDateTime dt = QDateTime::fromString("2026-11-07T21:45:00", Qt::ISODate);
    dt.setTimeSpec(Qt::UTC);
    qso.setDateTime(dt);
    qso.setFrequency("7040");
    qso.setMode("CW");
    qso.setExchangeField("SNs", "3");
    qso.setExchangeField("PRECs", "A");
    qso.setExchangeField("CHKs", "79");
    qso.setExchangeField("EXCHs", "IN");
    qso.setExchangeField("SNr", "52");
    qso.setExchangeField("PRECr", "B");
    qso.setExchangeField("CHKr", "72");
    qso.setExchangeField("EXCHr", "CT");
    qso.setPoints(2);  // exportToFile skips zero-point QSOs
    return qso;
}

// The ARRL SS QSO line carries both callsigns: the sent block uses our call,
// the received block the worked station's. Layout per the SS Cabrillo spec is
// freq, mode, date, time, then serial/prec/call/check/section for each side.
void TestCabrillo::exportsArrlSsQsoLine()
{
    const QStringList lines = exportLines(m_ssDef, {makeSsQso()}, "N9OH", "CW");
    QVERIFY(!lines.isEmpty());

    QString qsoLine;
    for (const QString& l : lines) {
        if (l.startsWith("QSO:")) { qsoLine = l; break; }
    }
    QVERIFY2(!qsoLine.isEmpty(), "no QSO: line was written");

    QCOMPARE(qsoLine, QStringLiteral("QSO:  7040 CW 2026-11-07 2145 3 A N9OH 79 IN 52 B K1UI 72 CT"));

    // No placeholder may survive into a submitted log.
    QVERIFY2(!qsoLine.contains('{') && !qsoLine.contains('}'),
             qPrintable("unresolved placeholder in: " + qsoLine));
}

// A Yaesu reports CW as "CW-U", and that is what reaches the QSO when the
// mode comes from the rig poll. The submitted log must still say CW.
void TestCabrillo::exportsRigCwVariantAsCw()
{
    for (const QString& rigMode : {"CW-U", "CW-L", "CWR"}) {
        QsoRecord qso = makeSsQso();
        qso.setMode(rigMode);

        QString qsoLine;
        for (const QString& l : exportLines(m_ssDef, {qso}, "N9OH", "CW")) {
            if (l.startsWith("QSO:")) { qsoLine = l; break; }
        }
        QVERIFY2(!qsoLine.isEmpty(), qPrintable(QString("no QSO: line for %1").arg(rigMode)));
        QCOMPARE(qsoLine, QStringLiteral("QSO:  7040 CW 2026-11-07 2145 3 A N9OH 79 IN 52 B K1UI 72 CT"));
    }
}

void TestCabrillo::mapsCabrilloContestNameToMode()
{
    auto contestHeader = [&](const QString& selectedMode) {
        for (const QString& l : exportLines(m_ssDef, {makeSsQso()}, "N9OH", selectedMode)) {
            if (l.startsWith("CONTEST:")) return l.trimmed();
        }
        return QString();
    };

    QCOMPARE(contestHeader("CW"),  QStringLiteral("CONTEST: ARRL-SS-CW"));
    QCOMPARE(contestHeader("SSB"), QStringLiteral("CONTEST: ARRL-SS-SSB"));
    // No mode selected falls back to the definition's default contest name.
    QCOMPARE(contestHeader(QString()), QStringLiteral("CONTEST: ARRL-SS-CW"));
}

// A template naming an exchange field the QSO does not carry produces an empty
// string rather than literal braces.
void TestCabrillo::resolvesUnknownPlaceholderToEmpty()
{
    QJsonObject cabrillo {
        {"contest", "TEST"},
        {"qsoTemplate", "QSO: {call} [{NOSUCHFIELD}] {EXCHr}"}
    };
    QJsonObject def { {"logging", QJsonObject{{"cabrillo", cabrillo}}} };

    QString qsoLine;
    for (const QString& l : exportLines(def, {makeSsQso()}, "N9OH", QString())) {
        if (l.startsWith("QSO:")) { qsoLine = l; break; }
    }
    QCOMPARE(qsoLine, QStringLiteral("QSO: K1UI [] CT"));
}

// The built-in placeholders used by the other 40-odd contest definitions keep
// their existing behaviour.
void TestCabrillo::leavesClassicTemplatesUnchanged()
{
    QJsonObject cabrillo {
        {"contest", "TEST"},
        {"qsoTemplate", "QSO: {freq} {mode} {date} {time} {mycall} {rst_sent} {exch_sent} "
                        "{call} {rst_rcvd} {exch_rcvd}"}
    };
    QJsonObject def { {"logging", QJsonObject{{"cabrillo", cabrillo}}} };

    QsoRecord qso = makeSsQso();
    qso.setRstSent("599");
    qso.setRstReceived("599");

    QString qsoLine;
    for (const QString& l : exportLines(def, {qso}, "N9OH", QString())) {
        if (l.startsWith("QSO:")) { qsoLine = l; break; }
    }
    QCOMPARE(qsoLine,
             QStringLiteral("QSO:  7040 CW 2026-11-07 2145 N9OH 599 IN K1UI 599 CT"));
}

// CQ WW RTTY sends RST + zone + QTH on both sides; zone and QTH are separate
// exchange fields (ZONEs/EXCHs, ZONEr/EXCHr), resolved by name from the
// shipped template. RTTY exports as RY.
void TestCabrillo::exportsCqwwRttyQsoLine()
{
    const QString path = QStringLiteral(CLX_SOURCE_DIR "/contests/cqww_rtty.json");
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable("cannot open " + path));
    const QJsonObject def = QJsonDocument::fromJson(f.readAll()).object();
    QVERIFY(!def.isEmpty());

    QsoRecord qso;
    qso.setCall("DL4EX");
    QDateTime dt = QDateTime::fromString("2026-09-26T00:07:00", Qt::ISODate);
    dt.setTimeSpec(Qt::UTC);
    qso.setDateTime(dt);
    qso.setFrequency("14085");
    qso.setMode("RTTY");
    qso.setRstSent("599");
    qso.setRstReceived("599");
    qso.setExchangeField("ZONEs", "04");
    qso.setExchangeField("EXCHs", "IN");
    qso.setExchangeField("ZONEr", "14");
    qso.setExchangeField("EXCHr", "DX");
    qso.setPoints(3);

    QString qsoLine;
    for (const QString& l : exportLines(def, {qso}, "N9OH", QString())) {
        if (l.startsWith("QSO:")) { qsoLine = l; break; }
    }
    QCOMPARE(qsoLine,
             QStringLiteral("QSO: 14085 RY 2026-09-26 0007 N9OH 599 04 IN DL4EX 599 14 DX"));
}

// CQ WW: a same-country QSO is worth 0 points but is a valid contact that
// counts for mults, so it must be in the submitted log. Dupes stay out. The
// CONTEST header follows the selected weekend.
void TestCabrillo::exportsZeroPointCqwwQso()
{
    const QString path = QStringLiteral(CLX_SOURCE_DIR "/contests/cqww.json");
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable("cannot open " + path));
    const QJsonObject def = QJsonDocument::fromJson(f.readAll()).object();
    QVERIFY(!def.isEmpty());

    auto make = [](const QString& call, bool dupe) {
        QsoRecord qso;
        qso.setCall(call);
        QDateTime dt = QDateTime::fromString("2026-11-28T00:01:00", Qt::ISODate);
        dt.setTimeSpec(Qt::UTC);
        qso.setDateTime(dt);
        qso.setFrequency("14025");
        qso.setMode("CW");
        qso.setRstSent("599");
        qso.setRstReceived("599");
        qso.setExchangeField("ZONEs", "04");
        qso.setExchangeField("ZONEr", "05");
        qso.setPoints(0);   // same country
        qso.setDupe(dupe);
        return qso;
    };

    const QStringList lines = exportLines(def, {make("K1UI", false), make("K1UI", true)}, "N9OH", "CW");
    QVERIFY(lines.contains(QStringLiteral("CONTEST: CQ-WW-CW")));
    QStringList qsoLines;
    for (const QString& l : lines)
        if (l.startsWith("QSO:")) qsoLines << l;
    QCOMPARE(qsoLines, QStringList{QStringLiteral("QSO: 14025 CW 2026-11-28 0001 N9OH 599 04 K1UI 599 05")});
}

// AM is not a Cabrillo mode; voice QSOs export as PH (NYQP allows PH and FM
// only for phone).
void TestCabrillo::exportsAmAsPhone()
{
    QJsonObject cabrillo {
        {"contest", "TEST"},
        {"qsoTemplate", "QSO: {freq} {mode} {date} {time} {mycall} {rst_sent} {exch_sent} "
                        "{call} {rst_rcvd} {exch_rcvd}"}
    };
    QJsonObject def { {"logging", QJsonObject{{"cabrillo", cabrillo}}} };

    QsoRecord qso = makeSsQso();
    qso.setMode("AM");
    qso.setFrequency("3885");
    qso.setRstSent("59");
    qso.setRstReceived("59");

    QString qsoLine;
    for (const QString& l : exportLines(def, {qso}, "N9OH", QString())) {
        if (l.startsWith("QSO:")) { qsoLine = l; break; }
    }
    QCOMPARE(qsoLine, QStringLiteral("QSO:  3885 PH 2026-11-07 2145 N9OH 59 IN K1UI 59 CT"));
}

// JIDX log upload accepts JIDX-CW and JIDX-PHONE; the Phone weekend's
// CONTEST header must match.
void TestCabrillo::mapsJidxPhoneContestName()
{
    const QString path = QStringLiteral(CLX_SOURCE_DIR "/contests/jidx.json");
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable("cannot open " + path));
    const QJsonObject def = QJsonDocument::fromJson(f.readAll()).object();
    QVERIFY(!def.isEmpty());

    QsoRecord qso = makeSsQso();
    qso.setMode("SSB");
    qso.setFrequency("14200");
    qso.setRstSent("59");
    qso.setRstReceived("59");

    QVERIFY(exportLines(def, {qso}, "N9OH", "SSB").contains(QStringLiteral("CONTEST: JIDX-PHONE")));
    QVERIFY(exportLines(def, {qso}, "N9OH", "CW").contains(QStringLiteral("CONTEST: JIDX-CW")));
}

QTEST_MAIN(TestCabrillo)
#include "test_cabrillo.moc"
