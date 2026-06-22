/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/practiceContentGenerator.h"
#include "contestEngine.h"

#include <QChar>
#include <QStringList>
#include <chrono>

namespace clx::audio {

namespace {

// Canonical US + common DX prefixes. Weighted list - more US entries so
// the random mix sounds like a typical North American contest band.
const char* const kPrefixes[] = {
    "K", "K", "K", "K", "W", "W", "W", "W", "N", "N", "N",
    "AA", "AB", "AC", "AD", "AE", "AF", "AG", "AI", "AJ", "AK",
    "KA", "KB", "KC", "KD", "KE", "KF", "KG", "KH", "KI", "KJ",
    "VE", "VE", "VA", "VY",
    "G", "M", "F", "DL", "DK", "DJ", "PA", "ON",
    "OK", "OH", "OE", "OM", "SM", "SP", "LA", "EA",
    "JA", "JA", "JA", "JH", "JR", "JE", "JF", "JK", "JM", "JN",
    "VK", "ZL", "LU", "PY", "CE", "HC", "PJ", "HK",
    "UA", "UB", "UR", "YL", "LY", "ES", "YU", "9A",
    "UT", "UY", "UX",
    "HA", "HB", "HL", "BY", "BV",
};
constexpr size_t kPrefixCount = sizeof(kPrefixes) / sizeof(kPrefixes[0]);

const char* const kSuffixChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

const char* const kNames[] = {
    "BOB", "JIM", "TOM", "DAVE", "MIKE", "JOHN", "BILL", "RICH",
    "STEVE", "GARY", "PAUL", "DAN", "TIM", "JEFF", "MARK", "CHRIS",
    "KEN", "DOUG", "SCOTT", "BRIAN", "ERIC", "KEVIN", "LARRY",
    "CHARLIE", "PETE", "FRANK", "ED", "AL", "ANDY", "HANS", "OLE",
    "RAY", "RUSS", "JACK", "GEORGE", "TONY",
};
constexpr size_t kNameCount = sizeof(kNames) / sizeof(kNames[0]);

// US states + common Canadian provinces - good enough for rag-chew
// "QTH is XX" fills. Contest mode uses named mults from the engine.
const char* const kStates[] = {
    "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA","HI","ID","IL",
    "IN","IA","KS","KY","LA","ME","MD","MA","MI","MN","MS","MO","MT",
    "NE","NV","NH","NJ","NM","NY","NC","ND","OH","OK","OR","PA","RI",
    "SC","SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
    "ON","QC","BC","AB","NS",
};
constexpr size_t kStateCount = sizeof(kStates) / sizeof(kStates[0]);

const char* const kRigs[] = {
    "IC7300", "IC7610", "IC705", "IC9700", "K4", "K3", "KX3",
    "FT991A", "FT710", "FTDX10", "FTDX101", "TS590SG", "TS890",
    "IC7851", "FT897",
};
constexpr size_t kRigCount = sizeof(kRigs) / sizeof(kRigs[0]);

const char* const kAntennas[] = {
    "DIPOLE", "VERTICAL", "YAGI", "3EL YAGI", "4EL YAGI", "HEXBEAM",
    "BEAM", "LOOP", "EFHW", "OCF", "G5RV", "ZEPP", "RANDOM WIRE",
};
constexpr size_t kAntCount = sizeof(kAntennas) / sizeof(kAntennas[0]);

const char* const kPowers[] = {
    "100", "100", "100", "200", "500", "1K", "1KW", "5", "QRP", "50",
};
constexpr size_t kPwrCount = sizeof(kPowers) / sizeof(kPowers[0]);

// Rag-chew templates. {mycall} is the station calling; other placeholders
// get filled per-fragment. Keep fragments on the shorter side so the
// decoder exercises realistically - a CQ + short info blob.
const char* const kRagchewTemplates[] = {
    "CQ CQ CQ DE {mycall} {mycall} K",
    "CQ DX CQ DX DE {mycall} {mycall} K",
    "DE {mycall} UR 599 HR NAME {name} QTH {state} HW?",
    "TNX FER CALL UR 599 NAME IS {name} QTH {state} BK",
    "FB {name} RIG HR IS {rig} INTO {ant} PWR {pwr} W",
    "RIG HR {rig} ANT {ant} PWR {pwr} W HW CPY?",
    "QSL TNX FER NICE QSO ES 73 GL DE {mycall} SK",
    "GE {name} TNX FER CALL UR 599 599 IN {state} BK",
    "NAME IS {name} NAME IS {name} QTH {state} BK",
    "WX HR SUNNY ES WARM {name} BK",
    "ANT HR {ant} UP 50 FT BK",
    "OP {name} OP {name} QTH {state} BK",
    "BK TO U {name} UR 599 IN {state} BK",
    "NICE SIG {name} UR 599 PLUS 10 DB BK",
    "73 ES GL {name} CUL DE {mycall} SK",
    "R R TNX {name} BK DE {mycall}",
    "DE {mycall} NAME {name} QTH {state} RIG {rig} BK",
    "TU {name} 73 GL CUL DE {mycall} SK",
    "DE {mycall} PSE QSL VIA BURO TNX 73",
    "FB SIG {name} UR 579 WID QSB HR BK",
    "CQ CQ CQ DE {mycall} PSE K",
    "ANT HR IS {ant} UP 70 FT PWR {pwr} W BK",
    "HR RUNNING {pwr} W INTO {ant} HW CPY?",
    "TU FER NICE QSO {name} 73 DE {mycall} SK",
    "UR 599 NAME {name} QTH {state} HW?",
    "GM {name} TNX FER CALL UR RST 599 BK",
    "GA {name} UR 599 IN {state} NAME IS {name} BK",
    "TU TU DE {mycall} SK",
    "CQ CQ DE {mycall} PSE CPY K",
    "DE {mycall} K",
};
constexpr size_t kRagchewCount = sizeof(kRagchewTemplates)
                               / sizeof(kRagchewTemplates[0]);

QString substitute(QString templ, const QString& mycall, const QString& name,
                   const QString& state, const QString& rig, const QString& ant,
                   const QString& pwr)
{
    templ.replace(QStringLiteral("{mycall}"), mycall);
    templ.replace(QStringLiteral("{name}"),   name);
    templ.replace(QStringLiteral("{state}"),  state);
    templ.replace(QStringLiteral("{rig}"),    rig);
    templ.replace(QStringLiteral("{ant}"),    ant);
    templ.replace(QStringLiteral("{pwr}"),    pwr);
    return templ;
}

} // namespace

PracticeContentGenerator::PracticeContentGenerator()
{
    const auto seed = static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    m_rng.seed(seed);
}

QString PracticeContentGenerator::nextFragment(PracticeMode mode)
{
    QString frag = (mode == PracticeMode::Contest) ? nextContest() : nextRagchew();
    if (!frag.endsWith(QLatin1Char(' '))) frag.append(QLatin1Char(' '));
    return frag.toUpper();
}

QString PracticeContentGenerator::nextRagchew()
{
    // Avoid immediate self-repeats so practice doesn't feel stuck in a
    // groove when one template happens to come up twice in a row.
    int idx;
    do {
        idx = static_cast<int>(m_rng() % kRagchewCount);
    } while (kRagchewCount > 1 && idx == m_lastRagchewTemplate);
    m_lastRagchewTemplate = idx;

    const QString mycall = randomCall();
    const QString name   = randomName();
    const QString state  = randomState();
    const QString rig    = randomRig();
    const QString ant    = randomAntenna();
    const QString pwr    = randomPower();
    return substitute(QString::fromLatin1(kRagchewTemplates[idx]),
                      mycall, name, state, rig, ant, pwr);
}

QString PracticeContentGenerator::nextContest()
{
    // Contest mode needs a live ContestEngine. Callers should only invoke
    // this when a contest is loaded, but be defensive.
    if (!m_engine || m_engine->getContestName().isEmpty()) {
        return nextRagchew();
    }

    const QString mycall = randomCall();
    const QString mult   = randomNamedMult(m_engine);
    const int serial     = randomSerial();

    // Contest exchanges always send 5NN as RST. The varying part is the
    // multiplier or serial. Produce both CQ fragments (Run side) and
    // reply/exchange fragments (S&P side) so practice covers both.
    // Roughly 30% CQ calls, 70% exchanges - matches a real band's ratio
    // of short answers to the occasional CQ.
    const int roll = static_cast<int>(m_rng() % 10);
    if (roll < 3) {
        return QStringLiteral("CQ TEST %1 %1 TEST").arg(mycall);
    }

    // Exchange format. If the contest has named mults (states, sections,
    // zones, counties, etc.) in its validation table, pick one at random
    // and use that format: "{call} 5NN {mult}". Otherwise it's a
    // serial-number contest (WPX-style), so use a 3-digit serial instead.
    if (!mult.isEmpty()) {
        return QStringLiteral("%1 5NN %2").arg(mycall, mult);
    }
    return QStringLiteral("%1 5NN %2").arg(mycall).arg(serial, 3, 10, QChar('0'));
}

QString PracticeContentGenerator::randomCall()
{
    const int prefixIdx = static_cast<int>(m_rng() % kPrefixCount);
    const QString prefix = QString::fromLatin1(kPrefixes[prefixIdx]);

    // Single-digit number for most calls; 0 or 1 are less common.
    const int digit = static_cast<int>(m_rng() % 10);

    // Suffix: 1-3 letters. 2-letter is most common (~60%), 1-letter ~15%,
    // 3-letter ~25% - matches rough distribution of real callsigns.
    const int suffLenRoll = static_cast<int>(m_rng() % 100);
    const int suffLen = (suffLenRoll < 15) ? 1 : (suffLenRoll < 75) ? 2 : 3;
    QString suffix;
    for (int i = 0; i < suffLen; ++i) {
        suffix.append(QChar(kSuffixChars[m_rng() % 26]));
    }
    return prefix + QString::number(digit) + suffix;
}

QString PracticeContentGenerator::randomName()
{
    return QString::fromLatin1(kNames[m_rng() % kNameCount]);
}

QString PracticeContentGenerator::randomState()
{
    return QString::fromLatin1(kStates[m_rng() % kStateCount]);
}

QString PracticeContentGenerator::randomRig()
{
    return QString::fromLatin1(kRigs[m_rng() % kRigCount]);
}

QString PracticeContentGenerator::randomAntenna()
{
    return QString::fromLatin1(kAntennas[m_rng() % kAntCount]);
}

QString PracticeContentGenerator::randomPower()
{
    return QString::fromLatin1(kPowers[m_rng() % kPwrCount]);
}

int PracticeContentGenerator::randomSerial()
{
    return 1 + static_cast<int>(m_rng() % 999);
}

QString PracticeContentGenerator::randomNamedMult(const ContestEngine* engine)
{
    if (!engine) return QString();
    const QStringList mults = engine->getNamedMultiplierList();
    if (mults.isEmpty()) return QString();
    return mults.at(static_cast<int>(m_rng() % static_cast<uint32_t>(mults.size())));
}

} // namespace clx::audio
