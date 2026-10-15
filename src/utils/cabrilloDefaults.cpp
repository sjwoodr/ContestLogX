/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "cabrilloDefaults.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {

// Split a setup value such as "SOHP-AB", "SO_ASSISTED_RTTY" or "MOBILE-SO"
// into upper-case tokens.
QStringList tokensOf(const QString& value)
{
    static const QRegularExpression sep(QStringLiteral("[-_ /]+"));
    return value.toUpper().split(sep, Qt::SkipEmptyParts);
}

QString powerOf(const QString& t)
{
    if (t.contains("QRP")) return "QRP";
    if (t == "HP" || t == "HIGH") return "HIGH";
    if (t == "LP" || t == "LOW") return "LOW";
    // Compact class codes: SOHP, SOLP, MMHP, SOABHP, MOSTLP
    if (t.size() > 2 && (t.startsWith('S') || t.startsWith('M'))) {
        if (t.endsWith("HP")) return "HIGH";
        if (t.endsWith("LP")) return "LOW";
    }
    return QString();
}

bool isSingleOpToken(const QString& t)
{
    static const QSet<QString> single = {"SO", "SOA", "SOU", "SOAB", "SOSB", "SOTB", "SINGLE"};
    // Compact codes that start with SO: SOHP, SOLP, SOQRP, SOABHP, SOAHP
    return single.contains(t) || (t.startsWith("SO") && t.size() <= 7);
}

bool isMultiOpToken(const QString& t)
{
    static const QSet<QString> multi = {"M1", "M2", "MM", "MS", "MO", "MD", "MOST", "MO2T", "MOMT",
                                        "MOAB", "MULTI", "UNLIMITED", "UNLIM", "UNL"};
    return multi.contains(t) || t.startsWith("MULTI") || t.startsWith("MOST")
           || ((t.startsWith("MM") || t.startsWith("M1")) && t.size() <= 5);
}

QString assistedOf(const QString& t)
{
    if (t == "SOA" || t == "SOU" || t == "ASSISTED" || t == "ASST") return "ASSISTED";
    // Compact SOA + power (RAC SOAHP / SOALP) - but SOAB... is all-band, not assisted
    if (t.startsWith("SOA") && !t.startsWith("SOAB") && t.size() > 3) return "ASSISTED";
    if (t == "UNASSISTED" || t == "NON-ASSISTED" || t == "UNASST") return "NON-ASSISTED";
    return QString();
}

QString transmitterOf(const QString& t)
{
    if (t == "M1" || t == "MS" || t.startsWith("MOST") || t == "M1HP" || t == "M1LP") return "ONE";
    if (t == "M2" || t == "MO2T") return "TWO";
    if (t == "MM" || t == "MOMT" || t == "UNLIMITED" || t == "UNLIM" || t == "UNL" || t == "MMHP" || t == "MMLP") return "UNLIMITED";
    return QString();
}

QString modeOf(const QString& t)
{
    if (t == "CW") return "CW";
    if (t == "SSB" || t == "PH" || t == "PHONE") return "SSB";
    if (t == "RTTY" || t == "RY") return "RTTY";
    if (t == "DIGITAL" || t == "DIGI" || t == "DIG" || t == "FT8" || t == "FT4") return "DIGI";
    if (t == "MIXED" || t == "MIX" || t == "ANALOG") return "MIXED";
    if (t == "FM") return "FM";
    // Compact class codes with a mode suffix: RAC SOABCW / SOABPH
    if (t.size() > 4 && (t.startsWith("SO") || t.startsWith("MO"))) {
        if (t.endsWith("CW")) return "CW";
        if (t.endsWith("PH") || t.endsWith("SSB")) return "SSB";
    }
    return QString();
}

QString bandOf(const QString& t)
{
    if (t == "AB" || t == "ALL" || t == "ALLBAND" || t.startsWith("SOAB") || t.startsWith("MOAB")) return "ALL";
    static const QMap<QString, QString> mhz = {
        {"1.8", "160M"}, {"3.5", "80M"}, {"7", "40M"}, {"14", "20M"}, {"21", "15M"}, {"28", "10M"}};
    if (mhz.contains(t)) return mhz.value(t);
    // A bare number is a band only for the HF contest bands - "MULTI-2" is a
    // transmitter count, not 2 meters. VHF bands need the M (6M, 2M).
    static const QRegularExpression hf(QStringLiteral("^(160|80|40|20|15|10)M?$"));
    static const QRegularExpression vhf(QStringLiteral("^(6|2)M$"));
    QRegularExpressionMatch m = hf.match(t);
    if (m.hasMatch()) return m.captured(1) + "M";
    m = vhf.match(t);
    if (m.hasMatch()) return m.captured(1) + "M";
    return QString();
}

QString stationOf(const QString& t)
{
    static const QMap<QString, QString> stations = {
        {"FIXED", "FIXED"}, {"MOBILE", "MOBILE"}, {"MOB", "MOBILE"},
        {"PORTABLE", "PORTABLE"}, {"POR", "PORTABLE"}, {"PORT", "PORTABLE"},
        {"ROVER", "ROVER"}, {"ROV", "ROVER"}, {"EXPEDITION", "EXPEDITION"}, {"EXP", "EXPEDITION"},
        {"HQ", "HQ"}, {"SCHOOL", "SCHOOL"}, {"DISTRIBUTED", "DISTRIBUTED"}, {"DIST", "DISTRIBUTED"},
        {"MD", "DISTRIBUTED"}};
    return stations.value(t);
}

} // namespace

QMap<QString, QString> CabrilloDefaults::fromSetup(const QJsonObject& contestDef,
                                                   const QString& stationClass,
                                                   const QMap<QString, QString>& promptValues)
{
    QMap<QString, QString> headers;

    // Values to read: the station class id and the answers to the category
    // prompts. Location prompts (stationType, stationLocation) are left out -
    // their answers are places, and "MS" / "MO" there mean Mississippi and
    // Missouri, not Multi-Single. Free-text answers are never read.
    static const QStringList categoryPrompts = {
        "operatingCategory", "operatingClass", "powerCategory", "contestMode", "modeCategory"};
    QStringList values;
    if (!stationClass.isEmpty())
        values << stationClass;
    for (const QString& id : categoryPrompts) {
        const QString answer = promptValues.value(id);
        if (!answer.isEmpty())
            values << answer;
    }

    auto setFirst = [&headers](const QString& header, const QString& value) {
        if (!value.isEmpty() && !headers.contains(header))
            headers.insert(header, value);
    };
    static const QRegularExpression multiN(QStringLiteral("MULTI[-_ ]?([12])\\b"));
    for (const QString& value : values) {
        const QStringList tokens = tokensOf(value);

        // Operator: within one value an explicit single-op token wins, because
        // a leading state code can look like a multi-op code ("MS_SO_FIXED" is
        // a Mississippi single op, not Multi-Single).
        bool single = false, multi = false, checklog = false;
        for (const QString& t : tokens) {
            single |= isSingleOpToken(t);
            multi |= isMultiOpToken(t);
            checklog |= (t == "CHECKLOG");
        }
        setFirst("CATEGORY-OPERATOR", checklog ? "CHECKLOG" : single ? "SINGLE-OP" : multi ? "MULTI-OP" : QString());

        // RSGB "MULTI-1" / "MULTI-2" name the transmitter count.
        const QRegularExpressionMatch mn = multiN.match(value.toUpper());
        if (mn.hasMatch())
            setFirst("CATEGORY-TRANSMITTER", mn.captured(1) == "1" ? "ONE" : "TWO");

        // Transmitter: the strongest claim in the value wins (MSQP "MS_UNL" is
        // unlimited, not Multi-Single).
        if (!single) {
            QString tx;
            for (const QString& t : tokens) {
                const QString c = transmitterOf(t);
                if (c == "UNLIMITED" || (c == "TWO" && tx != "UNLIMITED") || (c == "ONE" && tx.isEmpty()))
                    tx = c;
            }
            setFirst("CATEGORY-TRANSMITTER", tx);
        }

        for (const QString& t : tokens) {
            setFirst("CATEGORY-POWER", powerOf(t));
            setFirst("CATEGORY-ASSISTED", assistedOf(t));
            setFirst("CATEGORY-MODE", modeOf(t));
            setFirst("CATEGORY-BAND", bandOf(t));
            setFirst("CATEGORY-STATION", stationOf(t));
        }
    }

    // A single-mode contest needs no mode answer.
    const QJsonArray modes = contestDef["contest"].toObject()["modes"].toArray();
    if (!headers.contains("CATEGORY-MODE") && modes.size() == 1)
        setFirst("CATEGORY-MODE", modeOf(modes.first().toString().toUpper()));

    // Single operators run one transmitter.
    if (headers.value("CATEGORY-OPERATOR") == "SINGLE-OP")
        setFirst("CATEGORY-TRANSMITTER", "ONE");

    // Explicit contest mappings win over inference.
    const QJsonObject headerMap = contestDef["logging"].toObject()["cabrillo"].toObject()["headerMap"].toObject();
    for (auto it = headerMap.constBegin(); it != headerMap.constEnd(); ++it) {
        if (it.key().startsWith("_")) continue;
        const QJsonObject rule = it.value().toObject();
        // "promptId": a setup answer; "stationClass": true - the station class id
        const QString answer = rule["stationClass"].toBool(false)
            ? stationClass : promptValues.value(rule["promptId"].toString());
        if (answer.isEmpty()) continue;
        QString value = answer;
        if (rule.contains("values")) {
            const QJsonObject map = rule["values"].toObject();
            if (!map.contains(answer)) continue;
            value = map.value(answer).toString();
        }
        if (!value.isEmpty())
            headers.insert(it.key().toUpper(), value);
    }

    return headers;
}

QString CabrilloDefaults::toCabrillo3(const QString& header, const QString& value)
{
    const QString v = value.trimmed().toUpper();
    if (header == "CATEGORY-OPERATOR") {
        if (v == "SINGLE") return "SINGLE-OP";
        if (v == "MULTI") return "MULTI-OP";
    } else if (header == "CATEGORY-ASSISTED") {
        if (v == "YES") return "ASSISTED";
        if (v == "NO") return "NON-ASSISTED";
    } else if (header == "CATEGORY-BAND") {
        if (v == "ALL-BAND") return "ALL";
    } else if (header == "CATEGORY-MODE") {
        if (v == "DIGITAL" || v == "FT8") return "DIGI";
    }
    return value;
}
